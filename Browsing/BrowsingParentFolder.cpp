#include <QDir>

#include "BrowsingParentFolder.h"
#include "../ThumbData.h"
#include "../FavoriteFolderData.h"

using namespace Browsing;

BrowsingParentFolder::BrowsingParentFolder(QObject *parent)
    : BrowsingHistory{parent}
{
    connect(this, &BrowsingHistory::currentStateChanged, this, &BrowsingParentFolder::updateEnabledDirections);
}

void BrowsingParentFolder::goParent()
{
    QString parentFolderPath = getParentFolder(currentState_);
    ThumbData td;
    td.filePath = parentFolderPath;
    td.type = ThumbData::Folder;
    QVariant v;
    v.setValue(td);
    ThumbsViewState state{v};
    addHistoryRecord(state);
    emit restoreState(state);
}

QString BrowsingParentFolder::getParentFolder(const ThumbsViewState& state) const
{
    const QVariant& currentItem = state.currentItem;
    QString currentFolderPath;
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
        QDir dir(currentFolderPath);
        if (dir.cdUp()) {
            return dir.absolutePath();
        }
    }
    return {};
}

void BrowsingParentFolder::updateEnabledDirections()
{
    bool upEnabled = !getParentFolder(currentState_).isEmpty();
    if (upEnabled_ != upEnabled){
        upEnabled_ = upEnabled;
        emit upEnabledChanged();
    }
}

