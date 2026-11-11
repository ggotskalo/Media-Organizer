#ifndef FAVORITFOLDERSMODEL_H
#define FAVORITFOLDERSMODEL_H

#include <QAbstractListModel>
#include <QSettings>

#include "ThumbData.h"
#include "FavoriteFolderData.h"
#include "DirProcessor.h"

class FavoritFoldersModel : public QAbstractListModel
{
    Q_OBJECT
public:
    enum FolderRoles {
        DisplayName = Qt::UserRole + 1,
        FreeSpace,
    };

    explicit FavoritFoldersModel(QObject *parent = nullptr);
    ~FavoritFoldersModel();

    // QAbstractItemModel interface
    QHash<int, QByteArray> roleNames() const override;
    int rowCount(const QModelIndex &parent  = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;

    Q_INVOKABLE void itemClicked(int index);
    Q_INVOKABLE void refreshFreeSpace();
    Q_INVOKABLE void addFolder(QUrl folder);
    Q_INVOKABLE void removeFolder(int index);
    Q_INVOKABLE void renameFolder(int index, QString newName);
    Q_INVOKABLE void uniteWithFolder(int index, QUrl folder);
    Q_INVOKABLE QStringList getFolderPaths(int index) const;
    Q_INVOKABLE bool addPathToFolder(int index, QUrl folder);
    Q_INVOKABLE bool removePathFromFolder(int index, QString path);
    
    QList<FavoriteFolderData> getAllFolders() const { return folders_; }
    Q_INVOKABLE QStringList getAllFolderPaths() const;

signals:
    void itemSelected(FavoriteFolderData ffd);
    void foldersChanged(); // Emitted when folders list changes

protected:
    void loadSettings();
    void saveSettings();

    QSettings *settings_;
    QList<FavoriteFolderData> folders_;
};

#endif // FAVORITFOLDERSMODEL_H
