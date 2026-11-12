#include "MoveHelper.h"
#include "CopyManager.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
#include <QSet>
#include <QStorageInfo>
#include <QThread>
#include <QVariantMap>

namespace {

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

qint64 pathSize(const QString &path, const std::atomic<bool> &cancelled)
{
    QFileInfo info(path);
    if (!info.isDir())
        return info.size();
    qint64 total = 0;
    QDirIterator it(path, QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext() && !cancelled) {
        it.next();
        total += it.fileInfo().size();
    }
    return total;
}

} // namespace

MoveHelper::MoveHelper(CopyManager *copyManager, QObject *parent)
    : QObject(parent), copyManager_(copyManager)
{
}

MoveHelper::~MoveHelper()
{
    if (cancelFlag_)
        *cancelFlag_ = true;
}

QString MoveHelper::sizeText() const
{
    return calculating_ ? tr("calculating...") : formatBytes(size_);
}

void MoveHelper::prepare(const QString &path, const QStringList &srcRoots, const QStringList &destRoots)
{
    cancel();
    srcRoots_ = srcRoots;
    destRoots_ = destRoots;
    sourcePath_ = QDir::fromNativeSeparators(QFileInfo(path).absoluteFilePath());
    size_ = 0;
    calculating_ = true;
    const quint64 token = ++token_;
    auto flag = std::make_shared<std::atomic<bool>>(false);
    cancelFlag_ = flag;
    rebuildTargets();
    emit changed();

    const QString src = sourcePath_;
    QThread *thread = QThread::create([this, src, token, flag]() {
        const qint64 size = pathSize(src, *flag);
        if (*flag)
            return;
        QMetaObject::invokeMethod(this, "onSizeCalculated", Qt::QueuedConnection,
                                  Q_ARG(quint64, token), Q_ARG(qint64, size));
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void MoveHelper::cancel()
{
    if (cancelFlag_)
        *cancelFlag_ = true;
    ++token_;
}

void MoveHelper::onSizeCalculated(quint64 token, qint64 size)
{
    if (token != token_)
        return;
    size_ = size;
    calculating_ = false;
    rebuildTargets();
    emit changed();
}

void MoveHelper::rebuildTargets()
{
    targets_.clear();
    if (sourcePath_.isEmpty())
        return;

    QSet<QString> seen;
    for (int i = 0; i < destRoots_.size() && i < srcRoots_.size(); ++i) {
        const QString srcRoot = QDir::fromNativeSeparators(srcRoots_.at(i));
        const QString destRoot = QDir::fromNativeSeparators(destRoots_.at(i));
        if (!QFileInfo(destRoot).isDir())
            continue;

        const QString relative = QDir(srcRoot).relativeFilePath(sourcePath_);
        const QString destPath = QDir(destRoot).absoluteFilePath(relative);
        const QString destDir = QFileInfo(destPath).absolutePath();
        if (seen.contains(destPath.toLower()))
            continue;
        seen.insert(destPath.toLower());

        const QStorageInfo volume(destRoot);
        const bool sameVolume = QStorageInfo(srcRoot) == volume;
        const qint64 free = volume.bytesAvailable();

        bool enabled = false;
        QString note;
        if (!volume.isValid() || !volume.isReady() || volume.isReadOnly()) {
            note = tr("drive is not available");
        } else if (calculating_) {
            note = tr("calculating size...");
        } else if (!sameVolume && free < size_) {
            note = tr("not enough space (need %1 more)").arg(formatBytes(size_ - free));
        } else {
            enabled = true;
            const QFileInfo existing(destPath);
            if (existing.isDir() && QFileInfo(sourcePath_).isDir())
                note = tr("folder already exists there, contents will be merged");
            else if (existing.exists())
                note = tr("already exists there, will be saved with a new name");
        }

        QVariantMap t;
        t["drive"] = QDir::toNativeSeparators(destRoot);
        t["label"] = volume.displayName();
        t["destPath"] = QDir::toNativeSeparators(destPath);
        t["destDir"] = destDir;
        t["freeText"] = tr("%1 free of %2").arg(formatBytes(free), formatBytes(volume.bytesTotal()));
        t["enabled"] = enabled;
        t["note"] = note;
        targets_.append(t);
    }
}

bool MoveHelper::move(int targetIndex)
{
    if (calculating_ || targetIndex < 0 || targetIndex >= targets_.size())
        return false;
    const QVariantMap t = targets_.at(targetIndex).toMap();
    if (!t.value("enabled").toBool())
        return false;
    const QString destDir = t.value("destDir").toString();
    if (!QDir().mkpath(destDir))
        return false;
    return copyManager_->start({sourcePath_}, destDir, true, false, true);
}
