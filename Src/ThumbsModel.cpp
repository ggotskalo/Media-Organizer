#include "ThumbsModel.h"
#include "DiskOperations.h"
#include <QFileInfo>

ThumbsModel::ThumbsModel(QObject *parent) : QAbstractListModel(parent)
    , selectedThumbIndex_(-1)
{    
}

ThumbsModel::~ThumbsModel()
{
}

void ThumbsModel::listReceived(QStringList dirPath, QList<ThumbData> thumbs, bool success)
{
    beginResetModel();
    currentDirPathes_ = dirPath;
    int pos = 0;
    pathToPosition_.clear();
    thumbs_ = thumbs;
    for (const ThumbData& thumb : qAsConst(thumbs_)) {
        pathToPosition_.insert(thumb.filePath, pos++);
    }
    selectedThumbIndex_ = -1;
    endResetModel();
    updateBrokenShortcutsCount();
}

void ThumbsModel::selectItem(int index) {
    if (index < 0 || index >= thumbs_.size()) {
        return;
    }
    selectedThumbIndex_ =  index;
    ThumbData &td = thumbs_[index];
    emit itemSelected(td);
}

void ThumbsModel::setItemAsThumb(int index) {
    if (index < 0 || index >= thumbs_.size()) {
        return;
    }
    ThumbData &td = thumbs_[index];
    emit setItemAsThumbSignal(td);
}

void ThumbsModel::removeItem(int index) {
    if (index < 0 || index >= thumbs_.size()) {
        return;
    }
    const ThumbData item = thumbs_.at(index);
    const bool isShortcut = (item.filePath != item.fileShortcutPath);
    const QString pathToRemove = isShortcut ? item.fileShortcutPath : item.filePath;

    if (DiskOperations::remove(pathToRemove)) {
        beginRemoveRows(QModelIndex(), index, index);
        thumbs_.removeAt(index);
        endRemoveRows();
        updateBrokenShortcutsCount();
    }
}

void ThumbsModel::moveToParent(int index)
{
    if (index < 0 || index >= thumbs_.size()) {
        return;
    }
    QString path = thumbs_.at(index).filePath;
    if (DiskOperations::moveToParentFolder(path)) {
        beginRemoveRows(QModelIndex(), index, index);
        thumbs_.removeAt(index);
        endRemoveRows();
    }
}

void ThumbsModel::showInExplorer(int index)
{
    if (index < 0 || index >= thumbs_.size()) {
        return;
    }
    QString path = thumbs_.at(index).filePath;
    DiskOperations::showInExplorer(path);
}

QHash<int, QByteArray> ThumbsModel::roleNames() const
{
    return { {SourceRole, "source"}, {NameRole, "name"}, {SizeRole, "size"},
             {FolderRole, "isFolder"}, {ShortcutRole, "isShortcut"},
             {BrokenShortcutRole, "isBrokenShortcut"},
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
        case ShortcutRole:
            return td.filePath != td.fileShortcutPath;
        case BrokenShortcutRole:
            return td.isBrokenShortcut;
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

void ThumbsModel::showShortcutInExplorer(int index)
{
    if (index < 0 || index >= thumbs_.size()) {
        return;
    }
    const ThumbData &item = thumbs_.at(index);
    const bool isShortcut = item.filePath != item.fileShortcutPath;
    if (!isShortcut)
        return;

    DiskOperations::showInExplorer(item.fileShortcutPath);
}

void ThumbsModel::updateBrokenShortcutsCount()
{
    int count = 0;
    for (const ThumbData& td : qAsConst(thumbs_)) {
        if (td.isBrokenShortcut)
            ++count;
    }
    if (count != brokenShortcutsCount_) {
        brokenShortcutsCount_ = count;
        emit brokenShortcutsCountChanged();
    }
}

QStringList ThumbsModel::brokenShortcutPaths() const
{
    QStringList paths;
    for (const ThumbData& td : qAsConst(thumbs_)) {
        if (td.isBrokenShortcut)
            paths.append(td.fileShortcutPath);
    }
    return paths;
}

QStringList ThumbsModel::brokenTargetNames() const
{
    QStringList names;
    for (const ThumbData& td : qAsConst(thumbs_)) {
        if (td.isBrokenShortcut)
            names.append(QFileInfo(td.filePath).fileName());
    }
    return names;
}

void ThumbsModel::refresh()
{
    if (!currentDirPathes_.isEmpty())
        emit refreshRequested(currentDirPathes_);
}

void ThumbsModel::removeBrokenShortcuts()
{
    QStringList toRemove;
    for (const ThumbData& td : qAsConst(thumbs_)) {
        if (td.isBrokenShortcut)
            toRemove.append(td.fileShortcutPath);
    }

    for (const QString& path : qAsConst(toRemove))
        QFile::remove(path);

    if (!toRemove.isEmpty()) {
        thumbs_.erase(std::remove_if(thumbs_.begin(), thumbs_.end(),
            [](const ThumbData& td) { return td.isBrokenShortcut; }), thumbs_.end());
        beginResetModel();
        endResetModel();
        updateBrokenShortcutsCount();
    }
}
