#include "BrowsingHistory.h"
#include <QDir>
#include <QDebug>

BrowsingHistory::BrowsingHistory(QObject *parent) : QObject(parent)
{

}

void BrowsingHistory::itemSelected(QVariant& v, qreal scrollPosition)
{
    if (currentPos_ != history_.end()) {
        currentPos_->scrollPosition = scrollPosition;
    }
    if (currentPos_ == history_.end()) {
        currentPos_ = history_.insert(currentPos_, {v, 0});
    } else if (v != currentPos_->item) {//prevent  doubling
        currentPos_++;
        if (currentPos_ == history_.end() || v != currentPos_->item) {//prevent  doubling
            currentPos_ = history_.insert(currentPos_, {v, 0});
        }
    }
    updateEnabledDirections();
}

void BrowsingHistory::fileSelected(const ThumbData &td, qreal scrollPosition)
{
    if (td.type == ThumbData::Folder) {
        setCurrentPath(td.filePath);
        QVariant v;
        v.setValue(td);
        itemSelected(v, scrollPosition);
    }
}

void BrowsingHistory::favoriteFolderSelected(const FavoriteFolderData &ffd, qreal scrollPosition)
{
    setCurrentPath(ffd.pathes.join("\n"));
    QVariant v;
    v.setValue(ffd);
    itemSelected(v, scrollPosition);
}

void BrowsingHistory::currentClicked()
{
    emit openFolder(currentPath_);
}

void BrowsingHistory::goBack(qreal scrollPosition)
{
    Q_UNUSED(scrollPosition)
    if (currentPos_ == history_.begin()) {
        return;
    }
    if (currentPos_ != history_.end()) {
        currentPos_->scrollPosition = scrollPosition;
        QVariant v = currentPos_->item;
        if (v.canConvert<ThumbData>()) {
            lastSelectedFile_ = v.value<ThumbData>().filePath;
        }
    }
    currentPos_--;
    QVariant v = currentPos_->item;
    if (v.canConvert<ThumbData>()) {
        ThumbData td = v.value<ThumbData>();
        setCurrentPath(td.filePath);
        lastScrollPosition_ = currentPos_->scrollPosition;
        emit goFile(td);
    } else if (v.canConvert<FavoriteFolderData>()) {
        FavoriteFolderData ffd = v.value<FavoriteFolderData>();
        setCurrentPath(ffd.pathes.join("\n"));
        lastScrollPosition_ = currentPos_->scrollPosition;
        emit goFavoriteFolder(ffd);
    }
    updateEnabledDirections();
}

void BrowsingHistory::goForward(qreal scrollPosition)
{
    if (currentPos_ == history_.end() || currentPos_ + 1 == history_.end()) {
        return;
    }
    currentPos_->scrollPosition = scrollPosition;
    currentPos_++;
    QVariant v = currentPos_->item;
    if (v.canConvert<ThumbData>()) {
        ThumbData td = v.value<ThumbData>();
        setCurrentPath(td.filePath);
        lastScrollPosition_ = currentPos_->scrollPosition;
        emit goFile(td);
    } else if (v.canConvert<FavoriteFolderData>()) {
        FavoriteFolderData ffd = v.value<FavoriteFolderData>();
        setCurrentPath(ffd.pathes.join("\n"));
        lastScrollPosition_ = currentPos_->scrollPosition;
        emit goFavoriteFolder(ffd);
    }
    updateEnabledDirections();
}

void BrowsingHistory::goUp(qreal scrollPosition)
{
    if (currentPos_ == history_.end()) {
        return;
    }
    currentPos_->scrollPosition = scrollPosition;
    QVariant v = currentPos_->item;
    if (v.canConvert<ThumbData>()) {
        ThumbData td = v.value<ThumbData>();
        if (td.type == ThumbData::Folder) {
            QDir dir(td.filePath);
            if (dir.cdUp()) {
                td.filePath = dir.absolutePath();
                v.setValue(td);
                history_.insert(currentPos_, {v, scrollPosition});
                setCurrentPath(td.filePath);
                lastScrollPosition_ = 0;
                emit goFile(td);
                updateEnabledDirections();
            }
        }
    }
}

void BrowsingHistory::setCurrentPath(QString path)
{
    currentPath_ = path;
    emit currentPathChanged();
}

void BrowsingHistory::updateEnabledDirections()
{
    QDir dir(currentPath_);
    bool upEnabled = dir.cdUp();
    if (upEnabled_ != upEnabled){
        upEnabled_ = upEnabled;
        emit upEnabledChanged();
    }
    bool backEnabled = (currentPos_ != history_.begin());
    if (backEnabled != backEnabled_) {
        backEnabled_ = backEnabled;
        emit backEnabledChanged();
    }
    bool forwardEnabled = (currentPos_ != history_.end() && (currentPos_ + 1 != history_.end()));
    if (forwardEnabled != forwardEnabled_) {
        forwardEnabled_ = forwardEnabled;
        emit forwardEnabledChanged();
    }
}
