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
     };     

     explicit ThumbsModel(QObject *parent = nullptr);
     ~ThumbsModel();

     Q_INVOKABLE void selectItem(int index);
     Q_INVOKABLE void setItemAsThumb(int index);
     Q_INVOKABLE void removeItem(int index);
     Q_INVOKABLE void moveToParent(int index);
     Q_INVOKABLE void showInExplorer(int index);

     // QAbstractItemModel interface
     QHash<int, QByteArray> roleNames() const override;
     int rowCount(const QModelIndex &parent  = QModelIndex()) const override;
     QVariant data(const QModelIndex &index, int role) const override;
     QList<ThumbData> getThumbsList() const {return thumbs_;}
     int getSelectedThumbIndex() const {return selectedThumbIndex_;}

public slots:
    void showThumb(QString filePath, QString thumbSource, bool success);
    void listReceived(QStringList dirPathes, QList<ThumbData> thumbs, bool success);

signals:
    void itemSelected(ThumbData td);
    void setItemAsThumbSignal(ThumbData td);
    void showItem(int pos, qreal newScrollPosition);
    void clearSelection();

protected:
     QList<ThumbData> thumbs_;
     int selectedThumbIndex_;
     QHash<QString, int> pathToPosition_;
};

#endif // THUMBSMODEL_H
