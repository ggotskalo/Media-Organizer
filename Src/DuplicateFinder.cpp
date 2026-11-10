#include "DuplicateFinder.h"

#include <QtConcurrent>
#include <QDirIterator>
#include <QFileInfo>
#include <QFile>
#include <QDir>

const QSet<QString> DuplicateFinder::videoExt_ = {
    "mp4", "mpg", "wmv", "mkv", "ts", "avi", "mov", "m2ts", "flv"
};

DuplicateFinder::DuplicateFinder(QObject *parent)
    : QObject(parent)
{
}

DuplicateFinder::~DuplicateFinder()
{
    cancel();
}

void DuplicateFinder::startScan(QStringList searchFolders)
{
    if (running_)
        return;

    searchFolders_ = searchFolders;
    duplicateGroups_.clear();
    duplicateGroupNames_.clear();
    duplicateGroupSizes_.clear();
    cancelled_ = false;
    processedGroups_ = 0;
    replacedCount_ = 0;
    progress_ = 0;
    running_ = true;
    state_ = "scanning";
    emit runningChanged();
    emit stateChanged();

    QtConcurrent::run([this]() { doScan(); });
}

void DuplicateFinder::doScan()
{
    // key = "filename_lowercase|size" -> list of full paths
    QHash<QString, QStringList> fileMap;
    const int totalFolders = searchFolders_.size();

    for (int f = 0; f < totalFolders && !cancelled_; ++f) {
        const QString &folder = searchFolders_[f];

        QMetaObject::invokeMethod(this, [this, folder, f, totalFolders]() {
            progress_ = double(f) / qMax(totalFolders, 1);
            currentFolder_ = folder;
            emit progressChanged();
        }, Qt::QueuedConnection);

        QDirIterator it(folder, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext() && !cancelled_) {
            it.next();
            QFileInfo fi = it.fileInfo();

            if (fi.isShortcut())
                continue;

            QString ext = fi.suffix().toLower();
            if (!videoExt_.contains(ext))
                continue;

            if (fi.size() < 100000000)
                continue;

            //QString key = fi.fileName().toLower() + "|" + QString::number(fi.size());
            QString key = QString::number(fi.size());
            fileMap[key].append(fi.absoluteFilePath());
        }
    }

    if (cancelled_) {
        QMetaObject::invokeMethod(this, [this]() {
            running_ = false;
            state_ = "cancelled";
            emit runningChanged();
            emit stateChanged();
        }, Qt::QueuedConnection);
        return;
    }

    // Filter groups with 2+ unique files. This prevents duplicate
    // identical paths when favorite folders overlap.
    for (auto it = fileMap.constBegin(); it != fileMap.constEnd(); ++it) {
        QStringList uniquePaths;
        QSet<QString> seenPaths;
        for (const QString &path : it.value()) {
            if (!seenPaths.contains(path)) {
                seenPaths.insert(path);
                uniquePaths.append(path);
            }
        }

        if (uniquePaths.size() >= 2) {
            duplicateGroups_.append(uniquePaths);
            QFileInfo fi(uniquePaths.first());
            duplicateGroupNames_.append(fi.fileName());
            duplicateGroupSizes_.append(fi.size());
        }
    }

    const int groupCount = duplicateGroups_.size();

    QMetaObject::invokeMethod(this, [this, groupCount]() {
        progress_ = 1.0;
        emit progressChanged();
        emit scanFinished(groupCount);

        if (groupCount == 0) {
            running_ = false;
            state_ = "finished";
            emit runningChanged();
            emit stateChanged();
            emit finished(0);
            return;
        }

        state_ = "choosing";
        emit stateChanged();
    }, Qt::QueuedConnection);

    if (groupCount == 0)
        return;

    // Present each group to the user
    for (int i = 0; i < groupCount && !cancelled_; ++i) {
        const QStringList &paths = duplicateGroups_[i];
        const QString &name = duplicateGroupNames_[i];
        qint64 sizeBytes = duplicateGroupSizes_[i];

        QString sizeStr;
        if (sizeBytes >= 1073741824)
            sizeStr = QString::number(sizeBytes / 1073741824.0, 'f', 2) + " GB";
        else if (sizeBytes >= 1048576)
            sizeStr = QString::number(sizeBytes / 1048576.0, 'f', 1) + " MB";
        else
            sizeStr = QString::number(sizeBytes / 1024.0, 'f', 0) + " KB";

        accepted_ = false;
        keptPath_.clear();

        QMetaObject::invokeMethod(this, [this, name, sizeStr, paths]() {
            emit duplicateGroupFound(name, sizeStr, paths);
        }, Qt::QueuedConnection);

        {
            QMutexLocker locker(&mutex_);
            waitCondition_.wait(&mutex_);
        }

        if (cancelled_)
            break;

        if (accepted_ && !keptPath_.isEmpty()) {
            for (const QString &path : paths) {
                if (path == keptPath_)
                    continue;

                QString lnkPath = path + ".lnk";
                QFile::remove(path);
                QFile::link(keptPath_, lnkPath);
                ++replacedCount_;
            }
        }

        ++processedGroups_;
        QMetaObject::invokeMethod(this, [this]() {
            emit groupProcessed();
        }, Qt::QueuedConnection);
    }

    const int replaced = replacedCount_;
    QMetaObject::invokeMethod(this, [this, replaced]() {
        running_ = false;
        state_ = "finished";
        emit runningChanged();
        emit stateChanged();
        emit finished(replaced);
    }, Qt::QueuedConnection);
}

void DuplicateFinder::keepFile(QString pathToKeep)
{
    QMutexLocker locker(&mutex_);
    accepted_ = true;
    keptPath_ = pathToKeep;
    waitCondition_.wakeOne();
}

void DuplicateFinder::skipGroup()
{
    QMutexLocker locker(&mutex_);
    accepted_ = false;
    waitCondition_.wakeOne();
}

void DuplicateFinder::cancel()
{
    cancelled_ = true;
    QMutexLocker locker(&mutex_);
    accepted_ = false;
    waitCondition_.wakeOne();
}
