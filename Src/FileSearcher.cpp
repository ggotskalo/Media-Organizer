#include "FileSearcher.h"
#include "StatusManager.h"

#include <QDir>
#include <QDirIterator>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QSet>
#include <QThread>
#include <QTimer>

namespace {

QString normalizedRoot(const QString &path)
{
    QString p = QDir::fromNativeSeparators(QFileInfo(path).absoluteFilePath());
    while (p.size() > 3 && p.endsWith('/'))
        p.chop(1);
    return p;
}

QString formatBytes(qint64 bytes)
{
    const char *units[] = {"B", "KB", "MB", "GB", "TB"};
    double value = double(bytes);
    int unit = 0;
    while (value >= 1024.0 && unit < 4) {
        value /= 1024.0;
        ++unit;
    }
    return QString::number(value, 'f', unit == 0 ? 0 : (value >= 100 ? 0 : 1)) + " " + units[unit];
}

} // namespace

// ---------------------------------------------------------------- searcher

FileSearcher::FileSearcher(StatusManager *status, QObject *parent)
    : QObject(parent), status_(status)
{
}

FileSearcher::~FileSearcher()
{
    if (cancelFlag_)
        *cancelFlag_ = true;
}

// Removes duplicates (case-insensitive) and folders that lie inside another
// listed folder, so no file is scanned twice.
QStringList FileSearcher::uniqueRoots(const QStringList &folders)
{
    QStringList roots;
    QSet<QString> seen;
    for (const QString &f : folders) {
        if (f.isEmpty())
            continue;
        const QString n = normalizedRoot(f);
        if (QFileInfo(n).isDir() && !seen.contains(n.toLower())) {
            seen.insert(n.toLower());
            roots.append(n);
        }
    }
    QStringList result;
    for (const QString &r : roots) {
        bool nested = false;
        for (const QString &other : roots) {
            if (other == r)
                continue;
            const QString prefix = other.endsWith('/') ? other : other + '/';
            if (r.startsWith(prefix, Qt::CaseInsensitive)) {
                nested = true;
                break;
            }
        }
        if (!nested)
            result.append(r);
    }
    return result;
}

void FileSearcher::start(const QString &text, bool rescan, const QStringList &folders)
{
    cancel();

    const QStringList words = text.toLower().split(QRegExp("\\s+"), QString::SkipEmptyParts);
    query_ = text.trimmed();
    emit queryChanged();
    found_ = 0;
    emit foundChanged();
    if (words.isEmpty())
        return;
    emit searchStarted(query_);

    const QStringList roots = uniqueRoots(folders);

    const quint64 token = ++token_;
    auto cancelled = std::make_shared<std::atomic<bool>>(false);
    cancelFlag_ = cancelled;
    running_ = true;
    progress_ = 0;
    currentPath_.clear();
    emit runningChanged();
    emit progressChanged();
    updateStatus();

    QThread *thread = QThread::create([this, token, cancelled, words, roots, rescan]() {
        auto matches = [&words](const QString &lower) {
            for (const QString &w : words)
                if (!lower.contains(w))
                    return false;
            return true;
        };

        QStringList batchPaths;
        QVariantList batchSizes;
        QElapsedTimer flushTimer, progressTimer;
        flushTimer.start();
        progressTimer.start();

        auto flush = [&](bool force) {
            if (batchPaths.isEmpty() || (!force && flushTimer.elapsed() < 100))
                return;
            QMetaObject::invokeMethod(this, "onHits", Qt::QueuedConnection, Q_ARG(quint64, token),
                                      Q_ARG(QStringList, batchPaths), Q_ARG(QVariantList, batchSizes));
            batchPaths.clear();
            batchSizes.clear();
            flushTimer.restart();
        };
        auto report = [&](double p, const QString &path, bool force) {
            if (!force && progressTimer.elapsed() < 100)
                return;
            progressTimer.restart();
            QMetaObject::invokeMethod(this, "onProgress", Qt::QueuedConnection, Q_ARG(quint64, token),
                                      Q_ARG(double, p), Q_ARG(QString, path));
        };

        const int rootCount = roots.size();
        for (int r = 0; r < rootCount && !*cancelled; ++r) {
            const QString &root = roots.at(r);
            const QString key = root.toLower();

            std::shared_ptr<QVector<SearchEntry>> cached;
            if (!rescan) {
                QMutexLocker lock(&cacheMutex_);
                cached = cache_.value(key);
            }

            if (cached) {
                // search in the cache, no disk access
                report(double(r) / rootCount, root, true);
                const int n = cached->size();
                for (int i = 0; i < n && !*cancelled; ++i) {
                    const SearchEntry &e = cached->at(i);
                    if (matches(e.lower)) {
                        batchPaths << e.path;
                        batchSizes << QVariant::fromValue<qlonglong>(e.size);
                        flush(false);
                    }
                    if ((i & 0x3fff) == 0)
                        report((r + double(i) / qMax(1, n)) / rootCount, root, false);
                }
                flush(true);
                continue;
            }

            // scan the disk: top-level entries give the progress of this root
            auto fresh = std::make_shared<QVector<SearchEntry>>();
            QDir rootDir(root);
            const QFileInfoList top = rootDir.entryInfoList(
                QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden);

            auto addFile = [&](const QFileInfo &fi) {
                SearchEntry e;
                e.path = fi.absoluteFilePath();
                e.lower = fi.fileName().toLower();
                e.size = fi.size();
                fresh->append(e);
                if (matches(e.lower)) {
                    batchPaths << e.path;
                    batchSizes << QVariant::fromValue<qlonglong>(e.size);
                    flush(false);
                }
            };

            for (int t = 0; t < top.size() && !*cancelled; ++t) {
                const QFileInfo &info = top.at(t);
                const double base = r + double(t) / qMax(1, top.size());
                if (!info.isDir() || info.isSymLink()) {
                    addFile(info);
                } else {
                    report(base / rootCount, QDir::toNativeSeparators(info.absoluteFilePath()), false);
                    QDirIterator it(info.absoluteFilePath(),
                                    QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden,
                                    QDirIterator::Subdirectories);
                    int counter = 0;
                    while (it.hasNext() && !*cancelled) {
                        it.next();
                        const QFileInfo fi = it.fileInfo();
                        if (!fi.isDir() || fi.isSymLink())
                            addFile(fi);
                        if ((++counter & 0xff) == 0)
                            report(base / rootCount, QDir::toNativeSeparators(fi.absolutePath()), false);
                    }
                }
                report((r + double(t + 1) / qMax(1, top.size())) / rootCount,
                       QDir::toNativeSeparators(info.absoluteFilePath()), false);
            }
            flush(true);

            if (!*cancelled) {
                QMutexLocker lock(&cacheMutex_);
                cache_.insert(key, fresh); // only complete scans are cached
            }
        }
        flush(true);
        QMetaObject::invokeMethod(this, "onFinished", Qt::QueuedConnection, Q_ARG(quint64, token),
                                  Q_ARG(bool, bool(*cancelled)));
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void FileSearcher::cancel()
{
    if (cancelFlag_)
        *cancelFlag_ = true;
    ++token_; // late signals of the old search are ignored
    if (running_) {
        running_ = false;
        emit runningChanged();
        if (status_)
            status_->clearPriorityStatus();
    }
}

void FileSearcher::onHits(quint64 token, QStringList paths, QVariantList sizes)
{
    if (token != token_)
        return;
    Q_UNUSED(sizes)
    found_ += paths.size();
    emit foundChanged();
    emit hitsFound(paths);
}

void FileSearcher::onProgress(quint64 token, double progress, QString path)
{
    if (token != token_)
        return;
    progress_ = progress;
    currentPath_ = path;
    emit progressChanged();
    updateStatus();
}

void FileSearcher::onFinished(quint64 token, bool cancelled)
{
    if (token != token_)
        return;
    running_ = false;
    progress_ = cancelled ? progress_ : 1.0;
    emit runningChanged();
    emit progressChanged();
    if (status_) {
        status_->setPriorityStatus(tr("Search finished: %1 found").arg(found_));
        QTimer::singleShot(5000, this, [this, token]() {
            if (!running_ && token == token_ && status_)
                status_->clearPriorityStatus();
        });
    }
}

void FileSearcher::updateStatus()
{
    if (!status_ || !running_)
        return;
    status_->setPriorityStatus(tr("Searching: %1 - %2%").arg(currentPath_.isEmpty() ? query_ : currentPath_)
                           .arg(int(progress_ * 100)));
}
