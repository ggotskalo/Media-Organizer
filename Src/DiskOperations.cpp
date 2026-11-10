#include "DiskOperations.h"
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QGuiApplication>
#include <QClipboard>
#include <QMimeData>
#include <QUrl>
#ifdef Q_OS_WIN
#include <windows.h>
#include <shlobj.h>
#endif

DiskOperations::DiskOperations()
{

}

bool DiskOperations::remove(QString path)
{
    QFileInfo fileInfo(path);

    // Try file removal first (important for broken .lnk on Windows,
    // where QFileInfo::isFile() may return false).
    QFile file(path);
    if (file.remove())
        return true;

    if (fileInfo.isDir()) {
        QDir dir(path);
        return dir.removeRecursively();
    }

    return false;
}

bool DiskOperations::moveToParentFolder(QString path)
{
    QFileInfo fileInfo(path);
    QDir parentDir(fileInfo.path());
    if (!parentDir.cdUp())
        return false;

    QString newPath = parentDir.absoluteFilePath(fileInfo.fileName());
    if (QFileInfo::exists(newPath))
        return false;

    return QDir().rename(path, newPath);
}

bool DiskOperations::showInExplorer(QString path)
{
       QFileInfo info(path);
       QString existingPath;

       if (info.exists()) {
           existingPath = info.absoluteFilePath();
       } else {
           const QString parentDir = info.absolutePath();
           if (parentDir.isEmpty() || !QFileInfo(parentDir).exists())
               return false;
           existingPath = parentDir;
       }

#ifdef Q_OS_WIN
       const QString nativePath = QDir::toNativeSeparators(existingPath);
       PIDLIST_ABSOLUTE pidl = ILCreateFromPathW(reinterpret_cast<LPCWSTR>(nativePath.utf16()));
       if (pidl) {
           const HRESULT hr = SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0);
           ILFree(pidl);
           if (SUCCEEDED(hr))
               return true;
       }
#endif

       // Fallback when shell API fails.
       return QProcess::startDetached("explorer.exe", {QDir::toNativeSeparators(existingPath)});
}

void DiskOperations::setClipboardFiles(const QStringList& paths, bool isCut)
{
    QMimeData* mimeData = new QMimeData();
    QList<QUrl> urls;
    for (const QString& path : paths)
        urls.append(QUrl::fromLocalFile(path));
    mimeData->setUrls(urls);

    // DROPEFFECT_COPY = 1, DROPEFFECT_MOVE = 2
    QByteArray dropEffect(4, '\0');
    dropEffect[0] = isCut ? 2 : 1;
    mimeData->setData("Preferred DropEffect", dropEffect);

    QGuiApplication::clipboard()->setMimeData(mimeData);
}

void DiskOperations::copyToClipboard(QStringList paths)
{
    setClipboardFiles(paths, false);
}

void DiskOperations::cutToClipboard(QStringList paths)
{
    setClipboardFiles(paths, true);
}

bool DiskOperations::clipboardHasFiles()
{
    const QMimeData* mimeData = QGuiApplication::clipboard()->mimeData();
    return mimeData && mimeData->hasUrls();
}

bool DiskOperations::copyDirectoryRecursively(const QString& srcPath, const QString& destPath)
{
    QDir srcDir(srcPath);
    if (!srcDir.exists())
        return false;

    QDir().mkpath(destPath);

    for (const QFileInfo& info : srcDir.entryInfoList(QDir::Files | QDir::Dirs | QDir::NoDotAndDotDot)) {
        QString destItemPath = destPath + "/" + info.fileName();
        if (info.isDir()) {
            if (!copyDirectoryRecursively(info.absoluteFilePath(), destItemPath))
                return false;
        } else {
            if (!QFile::copy(info.absoluteFilePath(), destItemPath))
                return false;
        }
    }
    return true;
}

QString DiskOperations::generateUniquePath(const QString& destDir, const QString& fileName)
{
    QDir dir(destDir);
    QString path = dir.absoluteFilePath(fileName);
    if (!QFileInfo::exists(path))
        return path;

    QFileInfo fi(fileName);
    QString baseName = fi.completeBaseName();
    QString suffix = fi.suffix();

    for (int i = 2; i < 1000; ++i) {
        QString newName = suffix.isEmpty()
            ? QString("%1 (%2)").arg(baseName).arg(i)
            : QString("%1 (%2).%3").arg(baseName).arg(i).arg(suffix);
        path = dir.absoluteFilePath(newName);
        if (!QFileInfo::exists(path))
            return path;
    }
    return {};
}

QStringList DiskOperations::pasteFromClipboard(const QString& destDir)
{
    QStringList pastedPaths;
    const QClipboard* clipboard = QGuiApplication::clipboard();
    const QMimeData* mimeData = clipboard->mimeData();
    if (!mimeData || !mimeData->hasUrls())
        return pastedPaths;

    bool isCut = false;
    QByteArray dropEffect = mimeData->data("Preferred DropEffect");
    if (dropEffect.size() >= 1)
        isCut = (dropEffect.at(0) & 2) != 0;

    for (const QUrl& url : mimeData->urls()) {
        if (!url.isLocalFile())
            continue;

        QString srcPath = url.toLocalFile();
        QFileInfo srcInfo(srcPath);
        if (!srcInfo.exists())
            continue;

        QString destPath = generateUniquePath(destDir, srcInfo.fileName());
        if (destPath.isEmpty())
            continue;

        bool success = false;
        if (isCut) {
            success = QDir().rename(srcPath, destPath);
            if (!success) {
                if (srcInfo.isDir())
                    success = copyDirectoryRecursively(srcPath, destPath);
                else
                    success = QFile::copy(srcPath, destPath);
                if (success)
                    remove(srcPath);
            }
        } else {
            if (srcInfo.isDir())
                success = copyDirectoryRecursively(srcPath, destPath);
            else
                success = QFile::copy(srcPath, destPath);
        }

        if (success)
            pastedPaths.append(destPath);
    }

    if (isCut && !pastedPaths.isEmpty())
        QGuiApplication::clipboard()->clear();

    return pastedPaths;
}

QStringList DiskOperations::pasteLinkFromClipboard(const QString& destDir)
{
    QStringList createdLinks;
    const QMimeData* mimeData = QGuiApplication::clipboard()->mimeData();
    if (!mimeData || !mimeData->hasUrls())
        return createdLinks;

    for (const QUrl& url : mimeData->urls()) {
        if (!url.isLocalFile())
            continue;

        QString srcPath = url.toLocalFile();
        QFileInfo srcInfo(srcPath);
        if (!srcInfo.exists())
            continue;

        QString linkName = srcInfo.fileName() + ".lnk";
        QString linkPath = generateUniquePath(destDir, linkName);
        if (linkPath.isEmpty())
            continue;

        if (QFile::link(srcPath, linkPath))
            createdLinks.append(linkPath);
    }

    return createdLinks;
}
