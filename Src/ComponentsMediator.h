#ifndef COMPONENTSMEDIATOR_H
#define COMPONENTSMEDIATOR_H

#include "DirProcessor.h"
#include "FavoritFoldersModel.h"
#include "ThumbsDiskCacher.h"
#include "ThumbsModel.h"
#include "ThumbsProvider.h"
#include "ThumbsViewState.h"
#include "Browsing/BrowsingNavigation.h"

#include <QObject>

class ComponentsMediator : public QObject
{
    Q_OBJECT
public:
    explicit ComponentsMediator(ThumbsProvider& thumbsProvider,
                                DirProcessor& dirProcessor,
                                ThumbsDiskCacher& thumbsDiskCacher,
                                BrowsingNavigation& browsingNavigation,
                                ThumbsModel& thumbsModel,
                                FavoritFoldersModel& foldersModel,
                                QObject *parent = nullptr);

    void registerMetaTypes();
    void makeObjectsConnections();
    void finalize();

protected:
    static QStringList extractDirPathesFromState(ThumbsViewState& state);
    void restoreScrollPosAndSelectedItemFromNavigation(const QList<ThumbData> sortedDirFiles);
    void passCurrentStateToNavigation(const ThumbData& td);

    ThumbsProvider& thumbsProvider;
    DirProcessor& dirProcessor;
    ThumbsDiskCacher& thumbsDiskCacher;
    BrowsingNavigation& browsingNavigation;
    ThumbsModel& thumbsModel;
    FavoritFoldersModel& foldersModel;
};

#endif // COMPONENTSMEDIATOR_H
