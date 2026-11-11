#include "CopyManager.h"
#include "DiskOperations.h"

#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QClipboard>
#include <QThread>
#include <QElapsedTimer>
#include <QDateTime>

namespace {

qint64 itemSize(const QString &path)
{
    QFileInfo info(path);
    if (!info.isDir())
        return info.size();
    qint64 total = 0;
    QDirIterator it(path, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        total += it.fileInfo().size();
    }
    return total;
}

int itemFiles(const QString &path)
{
    QFileInfo info(path);
    if (!info.isDir())
        return 1;
    int n = 0;
    QDirIterator it(path, QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        it.next();
        ++n;
    }
    return n;
}

struct Job {
    const std::atomic<bool> &cancelled;
    qint64 total = 0;
    qint64 done = 0;
    int filesDone = 0;
    int filesTotal = 0;
    QString currentFile;
    QElapsedTimer timer;
    CopyManager *owner = nullptr;

    void report(bool force = false)
    {
        if (!force && timer.isValid() && timer.elapsed() < 40)
            return;
        timer.restart();
        const double p = total > 0 ? double(done) / double(total) : 0.0;
        const QString summary = QObject::tr("%1 of %2 files").arg(qMin(filesDone + 1, filesTotal)).arg(filesTotal);
        QMetaObject::invokeMethod(owner, "onProgress", Qt::QueuedConnection,
                                  Q_ARG(double, p), Q_ARG(QString, currentFile), Q_ARG(QString, summary));
    }
};

bool copyFileChunked(const QString &src, const QString &dst, Job &job)
{
    QFile in(src);
    if (!in.open(QIODevice::ReadOnly))
        return false;
    QFile out(dst);
    if (!out.open(QIODevice::WriteOnly)) {
        return false;
    }
    job.currentFile = QFileInfo(src).fileName();
    const qint64 chunk = 1024 * 1024;
    QByteArray buf;
    while (!in.atEnd()) {
        if (job.cancelled) {
            out.close();
            QFile::remove(dst);
            return false;
        }
        buf = in.read(chunk);
        if (buf.isEmpty() && in.error() != QFileDevice::NoError) {
            out.close();
            QFile::remove(dst);
            return false;
        }
        if (out.write(buf) != buf.size()) {
            out.close();
            QFile::remove(dst);
            return false;
        }
        job.done += buf.size();
        job.report();
    }
    out.close();
    ++job.filesDone;
    // keep modification time like a normal copy
    out.open(QIODevice::ReadWrite);
    out.setFileTime(QFileInfo(src).lastModified(), QFileDevice::FileModificationTime);
    out.close();
    return true;
}

bool copyDirChunked(const QString &src, const QString &dst, Job &job)
{
    QDir srcDir(src);
    if (!srcDir.exists())
        return false;
    QDir().mkpath(dst);
    for (const QFileInfo &info : srcDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden)) {
        if (job.cancelled)
            return false;
        const QString target = dst + "/" + info.fileName();
        const bool ok = info.isDir() ? copyDirChunked(info.absoluteFilePath(), target, job)
                                     : copyFileChunked(info.absoluteFilePath(), target, job);
        if (!ok)
            return false;
    }
    return true;
}

// Moves one file; renames when possible, otherwise copies and deletes the source.
bool moveFile(const QString &src, const QString &dst, Job &job)
{
    const qint64 size = QFileInfo(src).size();
    if (QFile::rename(src, dst)) {
        job.done += size;
        ++job.filesDone;
        job.currentFile = QFileInfo(src).fileName();
        job.report();
        return true;
    }
    if (!copyFileChunked(src, dst, job))
        return false;
    return QFile::remove(src);
}

// Moves the content of src into the existing folder dst (merge). Files that
// already exist there with the same size are considered duplicates and dropped
// from the source; different files get a new name, nothing is overwritten.
bool mergeMove(const QString &src, const QString &dst, Job &job)
{
    QDir srcDir(src);
    QDir().mkpath(dst);
    bool ok = true;
    for (const QFileInfo &info : srcDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot | QDir::Hidden)) {
        if (job.cancelled)
            return false;
        const QString name = info.fileName();
        const QString target = dst + "/" + name;
        if (info.isDir()) {
            if (QFileInfo(target).isDir()) {
                ok = mergeMove(info.absoluteFilePath(), target, job) && ok;
            } else {
                const QString unique = DiskOperations::uniquePath(dst, name);
                ok = !unique.isEmpty() && mergeMove(info.absoluteFilePath(), unique, job) && ok;
            }
        } else if (QFileInfo::exists(target)) {
            if (QFileInfo(target).size() == info.size()) {
                job.done += info.size();
                ++job.filesDone;
                ok = QFile::remove(info.absoluteFilePath()) && ok;
            } else {
                const QString unique = DiskOperations::uniquePath(dst, name);
                ok = !unique.isEmpty() && moveFile(info.absoluteFilePath(), unique, job) && ok;
            }
        } else {
            ok = moveFile(info.absoluteFilePath(), target, job) && ok;
        }
    }
    if (ok && !job.cancelled)
        QDir().rmdir(src); // source folder is empty now
    return ok;
}

} // namespace

CopyManager::CopyManager(QObject *parent) : QObject(parent)
{
}

CopyManager::~CopyManager()
{
    cancelled_ = true;
}

bool CopyManager::start(const QStringList &sources, const QString &destDir, bool isCut, bool clearClipboard, bool merge)
{
    if (running_ || sources.isEmpty())
        return false;

    cancelled_ = false;
    running_ = true;
    moving_ = isCut;
    progress_ = 0;
    currentFile_.clear();
    summary_.clear();
    destDir_ = destDir;
    emit runningChanged();
    emit progressChanged();

    QThread *thread = QThread::create([this, sources, destDir, isCut, clearClipboard, merge]() {
        Job job{cancelled_};
        job.owner = this;
        for (const QString &s : sources) {
            job.total += itemSize(s);
            job.filesTotal += itemFiles(s);
        }
        job.timer.start();

        int pasted = 0;
        for (int i = 0; i < sources.size() && !cancelled_; ++i) {
            const QString &srcPath = sources.at(i);
            QFileInfo srcInfo(srcPath);
            if (!srcInfo.exists())
                continue;
            const QString mergeTarget = destDir + "/" + srcInfo.fileName();
            const bool mergeInto = merge && srcInfo.isDir() && QFileInfo(mergeTarget).isDir();
            const QString destPath = mergeInto ? mergeTarget
                                               : DiskOperations::uniquePath(destDir, srcInfo.fileName());
            if (destPath.isEmpty())
                continue;

            job.currentFile = srcInfo.fileName();
            job.report(true);

            bool ok = false;
            const qint64 size = itemSize(srcPath);
            if (mergeInto) {
                ok = mergeMove(srcPath, destPath, job);
            } else if (isCut) {
                ok = QDir().rename(srcPath, destPath);
                if (ok) {
                    job.done += size;
                    job.filesDone += itemFiles(destPath);
                } else {
                    ok = srcInfo.isDir() ? copyDirChunked(srcPath, destPath, job)
                                         : copyFileChunked(srcPath, destPath, job);
                    if (ok)
                        DiskOperations::remove(srcPath);
                }
            } else {
                ok = srcInfo.isDir() ? copyDirChunked(srcPath, destPath, job)
                                     : copyFileChunked(srcPath, destPath, job);
            }
            if (ok)
                ++pasted;
            else if (cancelled_ && srcInfo.isDir() && !mergeInto)
                DiskOperations::remove(destPath); // drop partial folder
        }

        if (isCut && clearClipboard && pasted > 0) {
            QMetaObject::invokeMethod(qGuiApp, []() { QGuiApplication::clipboard()->clear(); },
                                      Qt::QueuedConnection);
        }
        QMetaObject::invokeMethod(this, "onWorkerFinished", Qt::QueuedConnection, Q_ARG(int, pasted));
    });
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
    return true;
}

void CopyManager::cancel()
{
    cancelled_ = true;
}

void CopyManager::onProgress(double progress, QString file, QString summary)
{
    if (!running_)
        return;
    progress_ = progress;
    currentFile_ = file;
    summary_ = summary;
    emit progressChanged();
}

void CopyManager::onWorkerFinished(int pastedCount)
{
    running_ = false;
    progress_ = 1;
    emit runningChanged();
    emit progressChanged();
    emit finished(destDir_, pastedCount);
}
