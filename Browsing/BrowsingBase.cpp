#include "BrowsingBase.h"
#include "../ThumbData.h"
#include "../FavoriteFolderData.h"

using namespace Browsing;

BrowsingBase::BrowsingBase(QObject *parent)
    : QObject{parent}
{

}

void BrowsingBase::openCurrentFolder()
{
    QString currentFolderPath;
    const QVariant& currentItem = currentState_.currentItem;
    if (currentItem.canConvert<ThumbData>()) {
        ThumbData td = currentItem.value<ThumbData>();
        if (td.type == ThumbData::Folder) {
            currentFolderPath = td.filePath;
        }
    } else if (currentItem.canConvert<FavoriteFolderData>()) {
        FavoriteFolderData ffd = currentItem.value<FavoriteFolderData>();
        if (ffd.pathes.size() == 1) {
            currentFolderPath = ffd.pathes.first();
        }
    }
    if (!currentFolderPath.isEmpty()) {
        emit openFolder(currentFolderPath);
    }

}

void BrowsingBase::setCurrentState(const ThumbsViewState& state)
{
    currentState_ = state;
    emit currentStateChanged(currentState_);
}

ThumbsViewState BrowsingBase::getCurrentState() const
{
    return currentState_;
}

QString BrowsingBase::getCurrentPath() const
{
    QString ret;
    const QVariant& currentItem = currentState_.currentItem;
    if (currentItem.canConvert<ThumbData>()) {
        ThumbData td = currentItem.value<ThumbData>();
        if (td.type == ThumbData::Folder) {
            ret = td.filePath;
        }
    } else if (currentItem.canConvert<FavoriteFolderData>()) {
        FavoriteFolderData ffd = currentItem.value<FavoriteFolderData>();
            ret = ffd.pathes.join("\n");
    }
    return ret;
}
