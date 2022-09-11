#ifndef BROWSINGBASE_H
#define BROWSINGBASE_H

#include <QObject>
#include "../ThumbsViewState.h"

namespace Browsing {

class BrowsingBase : public QObject
{
    Q_OBJECT

public:
    explicit BrowsingBase(QObject *parent = nullptr);
    virtual ~BrowsingBase() = default;

    Q_PROPERTY(QString currentPath READ getCurrentPath NOTIFY currentStateChanged)
    Q_INVOKABLE void openCurrentFolder();
    virtual void setCurrentState(const ThumbsViewState& state);
    virtual ThumbsViewState getCurrentState() const;

signals:
    void currentStateChanged(ThumbsViewState state);
    void openFolder(QString path);

protected:
    QString getCurrentPath() const;
    ThumbsViewState currentState_;

};

} // namespace Browsing

#endif // BROWSINGBASE_H
