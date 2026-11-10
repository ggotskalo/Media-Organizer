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
         BrokenShortcutRole,
     };

     explicit ThumbsModel(QObject *parent = nullptr);
     ~ThumbsModel();

     Q_INVOKABLE void selectItem(int index);
     Q_INVOKABLE void setItemAsThumb(int index);
     Q_INVOKABLE void removeItem(int index);
     Q_INVOKABLE void moveToParent(int index);
     Q_INVOKABLE void showInExplorer(int index);
     Q_INVOKABLE void showShortcutInExplorer(int index);
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
    void refreshRequested(QStringList dirPathes);
    void brokenShortcutsCountChanged();

protected:
     QList<ThumbData> thumbs_;
     int selectedThumbIndex_;
     QHash<QString, int> pathToPosition_;
     QStringList currentDirPathes_;
     int brokenShortcutsCount_ = 0;

     void updateBrokenShortcutsCount();
};

#endif // THUMBSMODEL_H
