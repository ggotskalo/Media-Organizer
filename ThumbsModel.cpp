#include "ThumbsModel.h"

ThumbsModel::ThumbsModel(QObject *parent) : QAbstractListModel(parent)
{    
}

ThumbsModel::~ThumbsModel()
{
}

void ThumbsModel::listReceived(QStringList dirPath, QList<ThumbData> thumbs, bool success)
{
    beginResetModel();
    int pos = 0;
    pathToPosition_.clear();
    thumbs_ = thumbs;
    for (const ThumbData& thumb : qAsConst(thumbs_)) {
        pathToPosition_.insert(thumb.filePath, pos++);
    }
    endResetModel();
}

void ThumbsModel::selectItem(int index, qreal scrollPosition) {
    if (index<0 || index>=thumbs_.size()) {
        return;
    }
    ThumbData &td = thumbs_[index];
    emit itemSelected(td, scrollPosition);
}

void ThumbsModel::setItemAsThumb(int index) {
    if (index<0 || index>=thumbs_.size()) {
        return;
    }
    ThumbData &td = thumbs_[index];
    emit setItemAsThumbSignal(td);
}

QHash<int, QByteArray> ThumbsModel::roleNames() const
{
    return { {SourceRole, "source"}, {NameRole, "name"}, {SizeRole, "size"},
             {FolderRole, "isFolder"},
           };
}


int ThumbsModel::rowCount(const QModelIndex &parent) const
{
    return thumbs_.count();
}

QVariant ThumbsModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() > thumbs_.count())
        return QVariant();
    const ThumbData& td = thumbs_.at(index.row());
    switch (role) {
        case SourceRole:
            return td.noThumb? "" : td.thumbSource;
        case NameRole:
            return td.displayName;
        case SizeRole:
            return td.size;
        case FolderRole:
            return td.type == ThumbData::Folder;
    }
    return QVariant();
}

void ThumbsModel::showThumb(QString filePath, QString thumbSource, bool success)
{
    auto iter = pathToPosition_.constFind(filePath);
    if (iter != pathToPosition_.constEnd()) {
        int pos = *iter;
        if (pos >=0 && pos < thumbs_.size()) {
            if (thumbs_[pos].thumbSource == thumbSource) {
                //force image update
                thumbs_[pos].thumbSource = "";
                emit dataChanged(index(pos), index(pos));
            }
            thumbs_[pos].thumbSource = thumbSource;
            emit dataChanged(index(pos), index(pos));
        }
    }
}

