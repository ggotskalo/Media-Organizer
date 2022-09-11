#include "BrowsingBackForward.h"

using namespace Browsing;

BrowsingBackForward::BrowsingBackForward(QObject *parent)
    : BrowsingParentFolder{parent}
{
    connect(this, &BrowsingHistory::historyChanged, this, &BrowsingBackForward::updateEnabledDirections);
}

void BrowsingBackForward::goBack()
{
    if (currentPos_ == history_.begin() || currentPos_ == history_.end()) {
        return;
    }
    currentPos_->scrollPos = scrollPos_;
    currentPos_->selectedPath = selectedPath_;
    currentPos_--;
    emit historyChanged();
    emit restoreState(*currentPos_);
}

void BrowsingBackForward::goForward()
{
    if (currentPos_ == history_.end() || currentPos_ + 1 == history_.end()) {
        return;
    }
    currentPos_->scrollPos = scrollPos_;
    currentPos_->selectedPath = selectedPath_;
    currentPos_++;
    emit historyChanged();
    emit restoreState(*currentPos_);
}

void BrowsingBackForward::addHistoryRecord(const ThumbsViewState &record)
{
    if (currentPos_ != history_.end()) {
        currentPos_->scrollPos = scrollPos_;
        currentPos_->selectedPath = selectedPath_;
    }
    selectedPath_.clear();
    BrowsingParentFolder::addHistoryRecord(record);
}

void BrowsingBackForward::updateEnabledDirections()
{
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
