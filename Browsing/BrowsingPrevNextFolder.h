#ifndef BROWSING_BROWSINGPREVNEXTFOLDER_H
#define BROWSING_BROWSINGPREVNEXTFOLDER_H

#include "BrowsingBackForward.h"

namespace Browsing {
//Navigation through sibling folders
class BrowsingPrevNextFolder : public BrowsingBackForward
{
    Q_OBJECT
public:
    explicit BrowsingPrevNextFolder(QObject *parent = nullptr);

    Q_PROPERTY(bool prevFolderEnabled MEMBER prevFolderEnabled_ NOTIFY prevFolderEnabledChanged)
    Q_PROPERTY(bool nextFolderEnabled MEMBER nextFolderEnabled_ NOTIFY nextFolderEnabledChanged)

    Q_INVOKABLE void goNextFolder();
    Q_INVOKABLE void goPrevFolder();

signals:
    void prevFolderEnabledChanged();
    void nextFolderEnabledChanged();

protected:
    inline bool canPrev();
    inline bool canNext();
    void updateEnabledDirections();
    bool prevFolderEnabled_ = false, nextFolderEnabled_ = false;
};

} // namespace Browsing

#endif // BROWSING_BROWSINGPREVNEXTFOLDER_H
