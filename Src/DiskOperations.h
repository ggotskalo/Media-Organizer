#ifndef DISKOPERATIONS_H
#define DISKOPERATIONS_H
#include <QString>
#include <QStringList>

class DiskOperations
{
public:
    DiskOperations();
    static bool remove(QString path);
    static bool moveToParentFolder(QString path);
    static bool showInExplorer(QString path);

    static void copyToClipboard(QStringList paths);
    static void cutToClipboard(QStringList paths);
    static QStringList pasteFromClipboard(const QString& destDir);
    static QStringList pasteLinkFromClipboard(const QString& destDir);
    static bool clipboardHasFiles();

private:
    static void setClipboardFiles(const QStringList& paths, bool isCut);
    static bool copyDirectoryRecursively(const QString& srcPath, const QString& destPath);
    static QString generateUniquePath(const QString& destDir, const QString& fileName);
};

#endif // DISKOPERATIONS_H
