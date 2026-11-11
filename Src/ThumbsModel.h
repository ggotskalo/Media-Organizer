#ifndef THUMBSMODEL_H
#define THUMBSMODEL_H
#include <QAbstractListModel>
#include <QSize>

#include "ThumbData.h"
#include "DirProcessor.h"

class ThumbsModel : public QAbstractListModel
 {
     Q_OBJECT
 public:
     enum FileRoles {
         SourceRole = Qt::UserRole + 1,
         NameRole,
         SizeRole,
         FolderRole,
         ShortcutRole,
         PictureRole,
         BrokenShortcutRole,
     };     

     explicit ThumbsModel(QObject *parent = nullptr);
     ~ThumbsModel();

     Q_INVOKABLE void selectItem(int index);
     Q_INVOKABLE void setItemAsThumb(int index);
     Q_INVOKABLE void removeItem(int index);
     Q_INVOKABLE void moveToParent(int index);
     Q_INVOKABLE bool isFolderIndex(int index) const;
     Q_INVOKABLE void goToOriginal(int index);
     Q_INVOKABLE void showInExplorer(int index);
     Q_INVOKABLE void showShortcutInExplorer(int index);
     Q_INVOKABLE bool isPictureIndex(int index) const;
     Q_INVOKABLE QString imagePathAt(int index) const;
     Q_INVOKABLE int picturesCount() const;
     Q_INVOKABLE int pictureOrdinal(int index) const;
     Q_INVOKABLE qint64 imageFileSizeAt(int index) const;
     Q_INVOKABLE qint64 imageFileSizeByPath(const QString& path) const;
     Q_INVOKABLE int imagePixelWidth(const QString& path) const;
     Q_INVOKABLE int nextPictureIndex(int index) const;
     Q_INVOKABLE int prevPictureIndex(int index) const;
     Q_INVOKABLE int firstPictureIndex() const;
     Q_INVOKABLE int lastPictureIndex() const;
     Q_INVOKABLE int pictureIndexByPath(const QString& path) const;
     Q_INVOKABLE QStringList picturePaths() const;
     Q_INVOKABLE bool copyImageToBest(int index);
     Q_INVOKABLE bool copyImageToBestPath(const QString& imagePath);
     Q_INVOKABLE void setFilterText(const QString& filterText);

     Q_INVOKABLE void copyItem(int index);
     Q_INVOKABLE void cutItem(int index);
     Q_INVOKABLE void pasteItems();
     Q_INVOKABLE void pasteLinkItems();
     Q_INVOKABLE bool canPaste() const;
     Q_INVOKABLE void removeBrokenShortcuts();
    Q_INVOKABLE void refresh();
    Q_INVOKABLE QStringList brokenShortcutPaths() const;
    Q_INVOKABLE QStringList brokenTargetNames() const;

     // QAbstractItemModel interface
     QHash<int, QByteArray> roleNames() const override;
     int rowCount(const QModelIndex &parent  = QModelIndex()) const override;
     QVariant data(const QModelIndex &index, int role) const override;
     QList<ThumbData> getThumbsList() const {return thumbs_;}
     int getSelectedThumbIndex() const {return selectedThumbIndex_;}

     Q_PROPERTY(QString filterText READ getFilterText WRITE setFilterText NOTIFY filterTextChanged)
     Q_PROPERTY(bool pasteEnabled READ canPaste NOTIFY canPasteChanged)
     QString getFilterText() const { return filterText_; }

     Q_PROPERTY(int brokenShortcutsCount READ brokenShortcutsCount NOTIFY brokenShortcutsCountChanged)
     int brokenShortcutsCount() const { return brokenShortcutsCount_; }

public slots:
    void showThumb(QString filePath, QString thumbSource, bool success);
    void listReceived(QStringList dirPathes, QList<ThumbData> thumbs, bool success);

signals:
    void itemSelected(ThumbData td);
    void setItemAsThumbSignal(ThumbData td);
    void showItem(int pos, qreal newScrollPosition);
    void clearSelection();
    void filterTextChanged();
    void canPasteChanged();
    void refreshRequested(QStringList dirPathes);
    void brokenShortcutsCountChanged();
    void openFolderRequested(QString folderPath);
    void revealItem(int index);

protected:
     QList<ThumbData> thumbs_;
     QList<ThumbData> filteredThumbs_;
     int selectedThumbIndex_;
     QHash<QString, int> pathToPosition_;
     QString filterText_;
     QStringList currentDirPathes_;
     QString pendingRevealPath_;
     int brokenShortcutsCount_ = 0;
     
     void updateFilter();
     bool matchesFilter(const ThumbData& thumb) const;
     void updateBrokenShortcutsCount();
     void removeFilteredIndex(int index);
     void revealPendingOriginal();
     int realItemIndex(const QString& path) const;
};

#endif // THUMBSMODEL_H
