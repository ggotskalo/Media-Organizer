#include "ThumbsModel.h"
#include "DiskOperations.h"
#include <QFileInfo>
#include <QImageReader>

ThumbsModel::ThumbsModel(QObject *parent) : QAbstractListModel(parent)
    , selectedThumbIndex_(-1)
    , filterText_("")
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
    updateFilter();
    for (const ThumbData& thumb : qAsConst(filteredThumbs_)) {
        pathToPosition_.insert(thumb.filePath, pos++);
    }
    selectedThumbIndex_ = -1;
    endResetModel();
    updateBrokenShortcutsCount();
}

void ThumbsModel::selectItem(int index) {
    if (index < 0 || index >= filteredThumbs_.size()) {
        return;
    }
    selectedThumbIndex_ = index;
    ThumbData &td = filteredThumbs_[index];
    emit itemSelected(td);
}

void ThumbsModel::setItemAsThumb(int index) {
    if (index < 0 || index >= filteredThumbs_.size()) {
        return;
    }
    ThumbData &td = filteredThumbs_[index];
    emit setItemAsThumbSignal(td);
}

void ThumbsModel::removeItem(int index) {
    if (index < 0 || index >= filteredThumbs_.size()) {
        return;
    }
    const ThumbData item = filteredThumbs_.at(index);
    const bool isShortcut = (item.filePath != item.fileShortcutPath);
    const QString pathToRemove = isShortcut ? item.fileShortcutPath : item.filePath;

    if (DiskOperations::remove(pathToRemove)) {
        // Find and remove from original list
        for (int i = 0; i < thumbs_.size(); ++i) {
            if (thumbs_[i].fileShortcutPath == item.fileShortcutPath) {
                thumbs_.removeAt(i);
                break;
            }
        }
        updateFilter();
        beginResetModel();
        endResetModel();
        updateBrokenShortcutsCount();
    }
}

void ThumbsModel::moveToParent(int index)
{
    if (index < 0 || index >= filteredThumbs_.size()) {
        return;
    }
    QString path = filteredThumbs_.at(index).filePath;
    if (DiskOperations::moveToParentFolder(path)) {
        // Find and remove from original list
        for (int i = 0; i < thumbs_.size(); ++i) {
            if (thumbs_[i].filePath == path) {
                thumbs_.removeAt(i);
                break;
            }
        }
        updateFilter();
        beginResetModel();
        endResetModel();
    }
}

bool ThumbsModel::isFolderIndex(int index) const
{
    if (index < 0 || index >= filteredThumbs_.size())
        return false;
    return filteredThumbs_.at(index).type == ThumbData::Folder;
}

void ThumbsModel::showInExplorer(int index)
{
    if (index < 0 || index >= filteredThumbs_.size()) {
        return;
    }
    QString path = filteredThumbs_.at(index).filePath;
    DiskOperations::showInExplorer(path);
}

void ThumbsModel::showShortcutInExplorer(int index)
{
    if (index < 0 || index >= filteredThumbs_.size()) {
        return;
    }
    const ThumbData &item = filteredThumbs_.at(index);
    const bool isShortcut = item.filePath != item.fileShortcutPath;
    if (!isShortcut)
        return;

    DiskOperations::showInExplorer(item.fileShortcutPath);
}

bool ThumbsModel::isPictureIndex(int index) const
{
    if (index < 0 || index >= filteredThumbs_.size())
        return false;
    return filteredThumbs_.at(index).type == ThumbData::Picture;
}

QString ThumbsModel::imagePathAt(int index) const
{
    if (!isPictureIndex(index))
        return {};
    return filteredThumbs_.at(index).filePath;
}

int ThumbsModel::picturesCount() const
{
    int count = 0;
    for (const ThumbData &item : filteredThumbs_) {
        if (item.type == ThumbData::Picture)
            ++count;
    }
    return count;
}

int ThumbsModel::pictureOrdinal(int index) const
{
    if (!isPictureIndex(index))
        return 0;

    int ordinal = 0;
    for (int i = 0; i <= index && i < filteredThumbs_.size(); ++i) {
        if (filteredThumbs_.at(i).type == ThumbData::Picture)
            ++ordinal;
    }
    return ordinal;
}

qint64 ThumbsModel::imageFileSizeAt(int index) const
{
    if (!isPictureIndex(index))
        return 0;
    return filteredThumbs_.at(index).size;
}

qint64 ThumbsModel::imageFileSizeByPath(const QString& path) const
{
    if (path.isEmpty())
        return 0;

    for (const ThumbData& item : filteredThumbs_) {
        if (item.type == ThumbData::Picture &&
            QString::compare(item.filePath, path, Qt::CaseInsensitive) == 0) {
            return item.size;
        }
    }
    QFileInfo fi(path);
    return fi.exists() ? fi.size() : 0;
}

int ThumbsModel::imagePixelWidth(const QString& path) const
{
    if (path.isEmpty())
        return 0;

    QImageReader reader(path);
    reader.setAutoTransform(true);
    const int width = reader.size().width();
    return width > 0 ? width : 0;
}

int ThumbsModel::nextPictureIndex(int index) const
{
    if (filteredThumbs_.isEmpty() || index < 0 || index >= filteredThumbs_.size())
        return -1;

    for (int i = index + 1; i < filteredThumbs_.size(); ++i) {
        if (filteredThumbs_.at(i).type == ThumbData::Picture)
            return i;
    }
    for (int i = 0; i < index; ++i) {
        if (filteredThumbs_.at(i).type == ThumbData::Picture)
            return i;
    }
    return index;
}

int ThumbsModel::prevPictureIndex(int index) const
{
    if (filteredThumbs_.isEmpty() || index < 0 || index >= filteredThumbs_.size())
        return -1;

    for (int i = index - 1; i >= 0; --i) {
        if (filteredThumbs_.at(i).type == ThumbData::Picture)
            return i;
    }
    for (int i = filteredThumbs_.size() - 1; i > index; --i) {
        if (filteredThumbs_.at(i).type == ThumbData::Picture)
            return i;
    }
    return index;
}

int ThumbsModel::firstPictureIndex() const
{
    for (int i = 0; i < filteredThumbs_.size(); ++i) {
        if (filteredThumbs_.at(i).type == ThumbData::Picture)
            return i;
    }
    return -1;
}

int ThumbsModel::lastPictureIndex() const
{
    for (int i = filteredThumbs_.size() - 1; i >= 0; --i) {
        if (filteredThumbs_.at(i).type == ThumbData::Picture)
            return i;
    }
    return -1;
}

int ThumbsModel::pictureIndexByPath(const QString& path) const
{
    if (path.isEmpty())
        return -1;

    for (int i = 0; i < filteredThumbs_.size(); ++i) {
        const ThumbData& item = filteredThumbs_.at(i);
        if (item.type == ThumbData::Picture &&
            QString::compare(item.filePath, path, Qt::CaseInsensitive) == 0) {
            return i;
        }
    }
    return -1;
}

QStringList ThumbsModel::picturePaths() const
{
    QStringList paths;
    for (const ThumbData& item : filteredThumbs_) {
        if (item.type == ThumbData::Picture)
            paths.append(item.filePath);
    }
    return paths;
}

bool ThumbsModel::copyImageToBest(int index)
{
    if (!isPictureIndex(index))
        return false;
    const QString imagePath = imagePathAt(index);
    if (imagePath.isEmpty())
        return false;
    return DiskOperations::copyToBest(imagePath);
}

bool ThumbsModel::copyImageToBestPath(const QString& imagePath)
{
    if (imagePath.isEmpty())
        return false;
    return DiskOperations::copyToBest(imagePath);
}

void ThumbsModel::copyItem(int index)
{
    if (index < 0 || index >= filteredThumbs_.size())
        return;
    DiskOperations::copyToClipboard({filteredThumbs_.at(index).filePath});
}

void ThumbsModel::cutItem(int index)
{
    if (index < 0 || index >= filteredThumbs_.size())
        return;
    DiskOperations::cutToClipboard({filteredThumbs_.at(index).filePath});
}

void ThumbsModel::pasteItems()
{
    if (currentDirPathes_.size() != 1)
        return;
    QStringList pasted = DiskOperations::pasteFromClipboard(currentDirPathes_.first());
    if (!pasted.isEmpty())
        emit refreshRequested(currentDirPathes_);
}

void ThumbsModel::pasteLinkItems()
{
    if (currentDirPathes_.size() != 1)
        return;
    QStringList created = DiskOperations::pasteLinkFromClipboard(currentDirPathes_.first());
    if (!created.isEmpty())
        emit refreshRequested(currentDirPathes_);
}

bool ThumbsModel::canPaste() const
{
    return currentDirPathes_.size() == 1 && DiskOperations::clipboardHasFiles();
}

QHash<int, QByteArray> ThumbsModel::roleNames() const
{
    return { {SourceRole, "source"}, {NameRole, "name"}, {SizeRole, "size"},
             {FolderRole, "isFolder"}, {ShortcutRole, "isShortcut"},
             {PictureRole, "isPicture"}, {BrokenShortcutRole, "isBrokenShortcut"},
           };
}


int ThumbsModel::rowCount(const QModelIndex &parent) const
{
    return filteredThumbs_.count();    
}

QVariant ThumbsModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() >= filteredThumbs_.count())
        return QVariant();
    const ThumbData& td = filteredThumbs_.at(index.row());
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
        case PictureRole:
            return td.type == ThumbData::Picture;
        case BrokenShortcutRole:
            return td.isBrokenShortcut;
    }
    return QVariant();
}

void ThumbsModel::showThumb(QString filePath, QString thumbSource, bool success)
{
    // Update in original list
    for (int i = 0; i < thumbs_.size(); ++i) {
        if (thumbs_[i].filePath == filePath) {
            if (thumbs_[i].thumbSource == thumbSource) {
                //force image update
                thumbs_[i].thumbSource = "";
            }
            thumbs_[i].thumbSource = thumbSource;
            break;
        }
    }
    
    // Update in filtered list and emit dataChanged
    auto iter = pathToPosition_.constFind(filePath);
    if (iter != pathToPosition_.constEnd()) {
        int pos = *iter;
        if (pos >= 0 && pos < filteredThumbs_.size()) {
            if (filteredThumbs_[pos].thumbSource == thumbSource) {
                //force image update
                filteredThumbs_[pos].thumbSource = "";
                emit dataChanged(index(pos), index(pos));
            }
            filteredThumbs_[pos].thumbSource = thumbSource;
            emit dataChanged(index(pos), index(pos));
        }
    }
}

void ThumbsModel::setFilterText(const QString& filterText)
{
    if (filterText_ != filterText) {
        filterText_ = filterText;
        beginResetModel();
        updateFilter();
        pathToPosition_.clear();
        int pos = 0;
        for (const ThumbData& thumb : qAsConst(filteredThumbs_)) {
            pathToPosition_.insert(thumb.filePath, pos++);
        }
        selectedThumbIndex_ = -1;
        endResetModel();
        emit filterTextChanged();
    }
}

void ThumbsModel::updateFilter()
{
    filteredThumbs_.clear();
    if (filterText_.isEmpty()) {
        filteredThumbs_ = thumbs_;
    } else {
        QString lowerFilter = filterText_.toLower();
        for (const ThumbData& thumb : qAsConst(thumbs_)) {
            if (matchesFilter(thumb)) {
                filteredThumbs_.append(thumb);
            }
        }
    }
}

bool ThumbsModel::matchesFilter(const ThumbData& thumb) const
{
    if (filterText_.isEmpty()) {
        return true;
    }
    QString lowerFilter = filterText_.toLower();
    QString lowerName = thumb.displayName.toLower();
    return lowerName.contains(lowerFilter);
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
        updateFilter();
        endResetModel();
        updateBrokenShortcutsCount();
    }
}
