#include "ShortcutFixer.h"

#include <QtConcurrent>
#include <QDirIterator>
#include <QFileInfo>
#include <QFile>
#include <QHash>
#include <QSet>

ShortcutFixer::ShortcutFixer(QObject *parent)
    : QObject(parent)
{
}

ShortcutFixer::~ShortcutFixer()
{
    cancel();
}

void ShortcutFixer::startRepair(QStringList shortcutPaths, QStringList targetNames, QStringList searchFolders)
{
    if (running_)
        return;

    shortcutPaths_ = shortcutPaths;
    targetNames_ = targetNames;
    searchFolders_ = searchFolders;
    cancelled_ = false;
    fixAllEnabled_ = false;
    repairedCount_ = 0;
    running_ = true;
    emit runningChanged();

    QtConcurrent::run([this]() { doScan(); });
}

void ShortcutFixer::doScan()
{
    const int totalShortcuts = shortcutPaths_.size();
    const int totalFolders = searchFolders_.size();
    QHash<QString, QStringList> candidatesByName;
    QSet<QString> indexedPaths;

    // Phase 1: scan disks once and build a filename -> paths index.
    for (int f = 0; f < totalFolders && !cancelled_; ++f) {
        const QString &folder = searchFolders_[f];

        double p = double(f) / qMax(totalFolders, 1) * 0.85;
        QMetaObject::invokeMethod(this, [this, folder, p]() {
            progress_ = p;
            currentFolder_ = folder;
            emit scanProgressChanged(folder, p);
        }, Qt::QueuedConnection);

        QDirIterator it(folder, QDir::Files | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (it.hasNext() && !cancelled_) {
            it.next();
            const QString filePath = it.filePath();
            if (indexedPaths.contains(filePath))
                continue;

            indexedPaths.insert(filePath);
            const QString fileNameKey = it.fileName().toLower();
            candidatesByName[fileNameKey].append(filePath);
        }
    }

    // Phase 2: process broken shortcuts using the prepared index.
    for (int i = 0; i < totalShortcuts && !cancelled_; ++i) {
        const QString &shortcutPath = shortcutPaths_[i];
        const QString &targetName = targetNames_[i];

        const QStringList candidates = candidatesByName.value(targetName.toLower());

        for (const QString &candidatePath : candidates) {
            if (cancelled_)
                break;

            if (fixAllEnabled_) {
                QFile::remove(shortcutPath);
                if (QFile::link(candidatePath, shortcutPath))
                    ++repairedCount_;
                break;
            }

            double p = 0.85 + (double(i) / qMax(totalShortcuts, 1)) * 0.15;
            QMetaObject::invokeMethod(this, [this, p]() {
                progress_ = p;
                emit scanProgressChanged(currentFolder_, p);
            }, Qt::QueuedConnection);

            accepted_ = false;
            acceptedNewTarget_.clear();

            QMetaObject::invokeMethod(this, [this, shortcutPath, targetName, candidatePath]() {
                emit matchFound(shortcutPath, targetName, candidatePath);
            }, Qt::QueuedConnection);

            // Pause until user decides
            QMutexLocker locker(&mutex_);
            waitCondition_.wait(&mutex_);

            if (cancelled_)
                break;

            if (accepted_ && !acceptedNewTarget_.isEmpty()) {
                QFile::remove(shortcutPath);
                QFile::link(acceptedNewTarget_, shortcutPath);
                ++repairedCount_;
                break;
            }
            // User skipped — stop searching for this shortcut, move to the next one
            break;
        }
    }

    const int repaired = repairedCount_;
    QMetaObject::invokeMethod(this, [this, repaired]() {
        running_ = false;
        progress_ = 1.0;
        emit runningChanged();
        emit finished(repaired);
        if (repaired > 0)
            emit refreshRequested();
    }, Qt::QueuedConnection);
}

void ShortcutFixer::acceptMatch(const QString &shortcutPath, const QString &newTarget)
{
    Q_UNUSED(shortcutPath)
    QMutexLocker locker(&mutex_);
    accepted_ = true;
    acceptedNewTarget_ = newTarget;
    waitCondition_.wakeOne();
}

void ShortcutFixer::fixAll(const QString &shortcutPath, const QString &newTarget)
{
    Q_UNUSED(shortcutPath)
    fixAllEnabled_ = true;

    QMutexLocker locker(&mutex_);
    accepted_ = true;
    acceptedNewTarget_ = newTarget;
    waitCondition_.wakeOne();
}

void ShortcutFixer::skipMatch()
{
    QMutexLocker locker(&mutex_);
    accepted_ = false;
    waitCondition_.wakeOne();
}

void ShortcutFixer::cancel()
{
    cancelled_ = true;
    fixAllEnabled_ = false;
    QMutexLocker locker(&mutex_);
    accepted_ = false;
    waitCondition_.wakeOne();
}
