#include "DiskOperations.h"
#include <QDir>
DiskOperations::DiskOperations()
{

}

bool DiskOperations::remove(QString path)
{
    bool success = false;
    QFileInfo fileInfo(path);
    if (fileInfo.isFile()) {
        QFile file(path);
        success = file.remove();
    } else if (fileInfo.isDir()) {
        QDir dir(path);
        success = dir.removeRecursively();
    }
    return success;
}

bool DiskOperations::moveToParentFolder(QString path)
{
    bool success = false;
    QFileInfo fileInfo(path);
    QString folderPath;
    if (fileInfo.isFile()) {
        folderPath = fileInfo.path();
    } else if (fileInfo.isDir()) {
        folderPath = path;
    }
    QDir dir(folderPath);
    if (dir.cdUp()) {
        QFile file(path);
        QString newName = dir.absoluteFilePath(fileInfo.fileName());
        success = file.rename(newName);

    }
    return success;
}
