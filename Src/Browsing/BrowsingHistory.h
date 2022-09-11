#ifndef BROWSINGHISTORY_H
#define BROWSINGHISTORY_H

#include <QLinkedList>
#include <QVariant>

#include "BrowsingBase.h"

namespace Browsing {

class BrowsingHistory : public BrowsingBase
{
    Q_OBJECT
public:
    explicit BrowsingHistory(QObject *parent = nullptr);

    void setCurrentState(const ThumbsViewState& state) override;

signals:
    void historyChanged();
    void restoreState(ThumbsViewState state);


protected:
    virtual void addHistoryRecord(const ThumbsViewState &record);

    QLinkedList<ThumbsViewState> history_;
    decltype(history_)::iterator currentPos_ = history_.end();

};

} // namespace Browsing

#endif // BROWSINGHISTORY_H
