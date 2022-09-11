#include "BrowsingHistory.h"

using namespace Browsing;

BrowsingHistory::BrowsingHistory(QObject *parent)
    : BrowsingBase{parent}
{
    connect(this, &BrowsingHistory::historyChanged, [this]{
        if (currentPos_ != history_.end())
            BrowsingBase::setCurrentState(*currentPos_);
    });
}

void BrowsingHistory::setCurrentState(const ThumbsViewState &state)
{
    addHistoryRecord(state);
}

void BrowsingHistory::addHistoryRecord(const ThumbsViewState &record)
{
    if (currentPos_ == history_.end()) {
        currentPos_ = history_.insert(currentPos_, record);
        emit historyChanged();
    } else if (record != *currentPos_) {//prevent doubling with current
        currentPos_++;
        if (currentPos_ == history_.end() || record != *currentPos_) {//prevent doubling with next
            currentPos_ = history_.insert(currentPos_, record);
        }
        emit historyChanged();
    }
}


