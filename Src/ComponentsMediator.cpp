#include "ComponentsMediator.h"

#include <algorithm>
#include <QDesktopServices>

ComponentsMediator::ComponentsMediator(ThumbsProvider& thumbsProvider2,
                                       DirProcessor& dirProcessor2,
                                       ThumbsDiskCacher& thumbsDiskCacher2,
                                       BrowsingNavigation& browsingNavigation2,
                                       ThumbsModel& thumbsModel2,
                                       FavoritFoldersModel& foldersModel2,
                                       QObject *parent)
    : QObject{parent},
      thumbsProvider(thumbsProvider2),
      dirProcessor(dirProcessor2),
      thumbsDiskCacher(thumbsDiskCacher2),
      browsingNavigation(browsingNavigation2),
      thumbsModel(thumbsModel2),
      foldersModel(foldersModel2)
{

}

void ComponentsMediator::registerMetaTypes()
{
    qRegisterMetaType<QList<ThumbData>>();
    QMetaType::registerComparators<ThumbData>();
    qRegisterMetaType<ThumbsViewState>();
    qRegisterMetaType<FavoriteFolderData>();
}

void ComponentsMediator::makeObjectsConnections()
{
    //browsingHistory->dirProcessor
    QObject::connect(&browsingNavigation, &BrowsingNavigation::openFolder, [&](QString dirPath){
         QDesktopServices::openUrl(QUrl::fromLocalFile(dirPath));
    });
    QObject::connect(&browsingNavigation, &BrowsingNavigation::restoreState, &dirProcessor,
                     [this](ThumbsViewState state){
        QStringList pathes = extractDirPathesFromState(state);
        dirProcessor.getUnitedDirFilesAsync(pathes);
    });

    //dirProcessor->thumbsModel
    QObject::connect(&dirProcessor, &DirProcessor::fileProcessed, &thumbsModel, &ThumbsModel::showThumb, Qt::QueuedConnection);

    QObject::connect(&dirProcessor, &DirProcessor::getDirFilesAsyncResponce, &thumbsModel, [this](QStringList dirPathes, QList<ThumbData> sortedDirFiles, bool success, bool allHaveThumbs){
        thumbsModel.listReceived(dirPathes, sortedDirFiles, success);
        restoreScrollPosAndSelectedItemFromNavigation(sortedDirFiles);
    });

    //thumbsModel->dirProcessor
    QObject::connect(&thumbsModel, &ThumbsModel::itemSelected, &dirProcessor,
        [&](ThumbData td) {
            //save selected path to history, need for restoring selected item
            browsingNavigation.setSelectedPath(td.filePath);
            if (td.type == ThumbData::Folder) {
                passCurrentStateToNavigation(td);
                dirProcessor.getUnitedDirFilesAsync({td.filePath});
            } else {
                QDesktopServices::openUrl(QUrl::fromLocalFile(td.filePath));
            }
    });

    QObject::connect(&thumbsModel, &ThumbsModel::itemSelected, &dirProcessor,
        [&](ThumbData td) {
            //save selected path to history, need for restoring selected item
            browsingNavigation.setSelectedPath(td.filePath);
            if (td.type == ThumbData::Folder) {
                passCurrentStateToNavigation(td);
                dirProcessor.getUnitedDirFilesAsync({td.filePath});
            } else {
                QDesktopServices::openUrl(QUrl::fromLocalFile(td.filePath));
            }
    });

    //foldersModel->dirProcessor
    QObject::connect(&foldersModel, &FavoritFoldersModel::itemSelected, &dirProcessor,
        [&](FavoriteFolderData ffd) {
            QVariant v;
            v.setValue(ffd);
            ThumbsViewState state{v};
            browsingNavigation.setCurrentState(state);
            dirProcessor.getUnitedDirFilesAsync(ffd.pathes);
        }
    );

    //=====Thumbs disk caching
    //dirProcessor->thumbsDiskCacher
    QObject::connect(&dirProcessor, &DirProcessor::fileProcessed, &thumbsDiskCacher, [&](QString filePath, QString thumbSource, bool success){
        if (success) {
            QImage image = thumbsProvider.getImage(thumbSource);
            if (!image.isNull()) {
                thumbsDiskCacher.storeThumbInCache(filePath, image);
            }
        }
    });
    QObject::connect(&dirProcessor, &DirProcessor::allFilesProcessed, &thumbsDiskCacher, &ThumbsDiskCacher::saveThumbs);
    QObject::connect(&dirProcessor, &DirProcessor::getDirFilesAsyncResponce, &thumbsDiskCacher, [&](QStringList dirPathes, QList<ThumbData> sortedDirFiles, bool success, bool allHaveThumbs){
            if (success) {
                thumbsDiskCacher.loadThumbs(dirPathes, sortedDirFiles, allHaveThumbs);
            }
    });

    //thumbsDiskCacher->thumbsProvider
    QObject::connect(&thumbsDiskCacher, &ThumbsDiskCacher::thumbLoaded, &thumbsProvider, [&](QString path, QImage thumb) {
        thumbsProvider.addImage(path, thumb);
        QMetaObject::invokeMethod(&thumbsModel, [&, path](){
            thumbsModel.showThumb(path, path, true);
        });
    });

    //thumbsDiskCacher->dirProcessor
    QObject::connect(&thumbsDiskCacher, &ThumbsDiskCacher::allThumbsLoaded, &dirProcessor, &DirProcessor::startGeneratingThumbnails);

    //thumbsProvider->thumbsDiskCacher
    QObject::connect(&thumbsProvider, &ThumbsProvider::thumbRequired, &thumbsDiskCacher, [&](const QString path){
        QImage thumb = thumbsDiskCacher.getThumb(path);
        if (!thumb.isNull()) {
            thumbsProvider.addImage(path, thumb);
            QMetaObject::invokeMethod(&thumbsModel, [&, path](){
                thumbsModel.showThumb(path, path, true);
            });
        } else {
            dirProcessor.generateThumbnail(path);
        }
    });

    //thumbsModel->thumbsDiskCacher
    QObject::connect(&thumbsModel, &ThumbsModel::setItemAsThumbSignal, &thumbsDiskCacher, [&](ThumbData td){
            QImage newThumb = thumbsProvider.getImage(td.thumbSource);
            if (newThumb.isNull()) {
                newThumb = thumbsDiskCacher.getThumb(td.filePath);
            }
            if (!newThumb.isNull()) {
                thumbsDiskCacher.setCurrentFolderThumb(newThumb);
            }
    });
}

void ComponentsMediator::finalize()
{
    //save unsaved generated thumbs
    QMetaObject::invokeMethod(&thumbsDiskCacher, [&](){
        thumbsDiskCacher.saveThumbs();
    }, Qt::BlockingQueuedConnection);

    //cancel working thumbnail generators
    QMetaObject::invokeMethod(&dirProcessor, [&](){
        dirProcessor.cancel();
    }, Qt::BlockingQueuedConnection);
    dirProcessor.waitFinished();

}

QStringList ComponentsMediator::extractDirPathesFromState(ThumbsViewState& state)
{
    QVariant &currentItem = state.currentItem;
    QStringList pathes;
    if (currentItem.canConvert<ThumbData>()) {
        ThumbData td = currentItem.value<ThumbData>();
        if (td.type == ThumbData::Folder) {
            pathes.append(td.filePath);
        }
    } else if (currentItem.canConvert<FavoriteFolderData>()) {
        FavoriteFolderData ffd = currentItem.value<FavoriteFolderData>();
        pathes = ffd.pathes;
    }
    return pathes;
}

void ComponentsMediator::restoreScrollPosAndSelectedItemFromNavigation(const QList<ThumbData> sortedDirFiles)
{
    ThumbsViewState state = browsingNavigation.getCurrentState();
    qreal newScrollPosition = state.scrollPos;
    QString lastSelectedFile = state.selectedPath;
    if (newScrollPosition !=0 || !lastSelectedFile.isEmpty()) {
        auto foundIt = std::find_if(sortedDirFiles.cbegin(), sortedDirFiles.cend(), [lastSelectedFile](const ThumbData& td){
            return td.filePath == lastSelectedFile;
        });
        int pos = foundIt == sortedDirFiles.cend() ? -1 : foundIt - sortedDirFiles.cbegin();
        emit thumbsModel.showItem(pos, newScrollPosition);
        browsingNavigation.setSelectedPath(lastSelectedFile);
    } else {
        emit thumbsModel.clearSelection();
    }
}

void ComponentsMediator::passCurrentStateToNavigation(const ThumbData& td)
{
    int currentItemIndex = thumbsModel.getSelectedThumbIndex();
    auto parentFolderItems = thumbsModel.getThumbsList();
    //remove all non folder items, need for navigation through sibling folders
    if (currentItemIndex > 0 && currentItemIndex < parentFolderItems.size()) {
        const ThumbData currentItem = parentFolderItems.at(currentItemIndex);
        auto newEnd = std::remove_if(parentFolderItems.begin(), parentFolderItems.end(),
                                     [](const ThumbData& td){return td.type != ThumbData::Folder || td.noThumb;});
        parentFolderItems.erase(newEnd, parentFolderItems.end());
        auto selectedPosIterator = std::find(parentFolderItems.cbegin(), parentFolderItems.cend(), currentItem);
        if (selectedPosIterator != parentFolderItems.cend()) {
            currentItemIndex = selectedPosIterator - parentFolderItems.cbegin();
        } else {
            currentItemIndex = -1;
        }
    }
    QVariant v;
    v.setValue(td);
    ThumbsViewState state{v, 0, "", parentFolderItems, currentItemIndex};
    browsingNavigation.setCurrentState(state);
}
