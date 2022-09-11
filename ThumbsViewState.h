#ifndef THUMBSVIEWSTATE_H
#define THUMBSVIEWSTATE_H

#include "ThumbData.h"
#include <QVariant>
class ThumbsViewState
{
public:
    bool operator!=(const ThumbsViewState& other) const {return currentItem != other.currentItem;}

    QVariant currentItem;
    qreal scrollPos = 0;
    QString selectedPath;
    QList<ThumbData> siblingFolders;
    int currentFolderIndex;
};

Q_DECLARE_METATYPE(ThumbsViewState)
#endif // THUMBSVIEWSTATE_H
