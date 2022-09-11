#include "BrowsingPrevNextFolder.h"

using namespace Browsing;

BrowsingPrevNextFolder::BrowsingPrevNextFolder(QObject *parent)
    : BrowsingBackForward{parent}
{
    connect(this, &BrowsingHistory::currentStateChanged, this, &BrowsingPrevNextFolder::updateEnabledDirections);
}

void BrowsingPrevNextFolder::goNextFolder()
{
    if (canNext()) {
        ThumbsViewState& state=*currentPos_;
        state.currentItem.setValue(currentPos_->siblingFolders.at(++state.currentFolderIndex));
        //don't add to history
        BrowsingBase::setCurrentState(state);
        emit restoreState(state);
    }
}

void BrowsingPrevNextFolder::goPrevFolder()
{
    if (canPrev()) {
        ThumbsViewState& state=*currentPos_;
        state.currentItem.setValue(currentPos_->siblingFolders.at(--state.currentFolderIndex));
        //don't add to history
        BrowsingBase::setCurrentState(state);
        emit restoreState(state);
    }
}

bool BrowsingPrevNextFolder::canPrev()
{
    return (currentPos_ != history_.end()
            && currentPos_->currentFolderIndex > 0);
}

bool BrowsingPrevNextFolder::canNext()
{
    return (currentPos_ != history_.end()
            && currentPos_->currentFolderIndex >= 0
            && currentPos_->currentFolderIndex+1 < currentPos_->siblingFolders.size());
}

void BrowsingPrevNextFolder::updateEnabledDirections()
{
    bool prevFolderEnabled = canPrev();
    if (prevFolderEnabled != prevFolderEnabled_) {
        prevFolderEnabled_ = prevFolderEnabled;
        emit prevFolderEnabledChanged();
    }
    bool nextFolderEnabled = canNext();
    if (nextFolderEnabled != nextFolderEnabled_) {
        nextFolderEnabled_ = nextFolderEnabled;
        emit nextFolderEnabledChanged();
    }
}
