#include "DiskOperations.h"
#include <QDir>
#include <QFileInfo>
#include <QProcess>
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

