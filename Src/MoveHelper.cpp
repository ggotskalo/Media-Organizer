#include "MoveHelper.h"
#include "CopyManager.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>
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

void MoveHelper::prepare(const QString &path)
{
    cancel();
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

    const QStorageInfo srcVolume(sourcePath_);
    const QString srcRoot = QDir::fromNativeSeparators(srcVolume.rootPath());
    if (!srcVolume.isValid() || srcRoot.isEmpty())
        return;
    const QString relative = QDir(srcRoot).relativeFilePath(sourcePath_);
    // top-level folder of the path, it must already exist on the target drive
    const QString topFolder = relative.section('/', 0, 0);
    if (topFolder.isEmpty() || topFolder == "." || topFolder == "..")
        return;

    for (const QStorageInfo &volume : QStorageInfo::mountedVolumes()) {
        const QString root = QDir::fromNativeSeparators(volume.rootPath());
        if (!volume.isValid() || !volume.isReady() || volume.isReadOnly()
            || root.compare(srcRoot, Qt::CaseInsensitive) == 0)
            continue;

        if (!QFileInfo(QDir(root).absoluteFilePath(topFolder)).isDir())
            continue;

        const QString destPath = QDir(root).absoluteFilePath(relative);
        const QString destDir = QFileInfo(destPath).absolutePath();
        const qint64 free = volume.bytesAvailable();

        bool enabled = false;
        QString note;
        if (calculating_) {
            note = tr("calculating size...");
        } else if (free < size_) {
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
        t["drive"] = QDir::toNativeSeparators(root);
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
