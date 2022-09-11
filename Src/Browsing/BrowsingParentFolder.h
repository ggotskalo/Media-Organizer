#ifndef BROWSINGPARENTFOLDER_H
#define BROWSINGPARENTFOLDER_H

#include <QObject>
#include "BrowsingHistory.h"

namespace Browsing {

class BrowsingParentFolder : public BrowsingHistory
{
    Q_OBJECT
public:
    explicit BrowsingParentFolder(QObject *parent = nullptr);
    Q_PROPERTY(bool upEnabled MEMBER upEnabled_ NOTIFY upEnabledChanged)
    Q_INVOKABLE void goParent();

signals:
    void upEnabledChanged();

protected:
    bool upEnabled_ = false;
    QString getParentFolder(const ThumbsViewState& state) const;
    bool isParentFolderEnabled(const ThumbsViewState& state);
    void updateEnabledDirections();

};

} // namespace Browsing

#endif // BROWSINGPARENTFOLDER_H
