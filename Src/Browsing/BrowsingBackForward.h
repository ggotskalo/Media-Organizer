#ifndef BROWSINGBACKFORWARD_H
#define BROWSINGBACKFORWARD_H

#include <QObject>
#include "BrowsingParentFolder.h"

namespace Browsing {

class BrowsingBackForward : public BrowsingParentFolder
{
    Q_OBJECT
public:
    explicit BrowsingBackForward(QObject *parent = nullptr);

    Q_PROPERTY(bool backEnabled MEMBER backEnabled_ NOTIFY backEnabledChanged)
    Q_PROPERTY(bool forwardEnabled MEMBER forwardEnabled_ NOTIFY forwardEnabledChanged)

    Q_INVOKABLE void setScrollPos(qreal pos) {scrollPos_ = pos;}
    Q_INVOKABLE void setSelectedPath(QString path) {selectedPath_ = path;}
    Q_INVOKABLE void goBack();
    Q_INVOKABLE void goForward();

signals:
    void backEnabledChanged();
    void forwardEnabledChanged();

protected:
    void addHistoryRecord(const ThumbsViewState &record) override;
    void updateEnabledDirections();

    bool backEnabled_ = false, forwardEnabled_ = false;
    qreal scrollPos_ = 0;
    QString selectedPath_;
};

} // namespace Browsing

#endif // BROWSINGBACKFORWARD_H
