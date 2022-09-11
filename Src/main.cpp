#include <algorithm>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QScreen>
#include <QThreadPool>
#include <QWindow>

#include "DirProcessor.h"
#include "FavoritFoldersModel.h"
#include "ThumbsDiskCacher.h"
#include "ThumbsModel.h"
#include "ThumbsProvider.h"
#include "ThumbsViewState.h"
#include "Browsing/BrowsingNavigation.h"

void makeObjectsConnections(ThumbsProvider& thumbsProvider,
                            DirProcessor& dirProcessor,
                            ThumbsDiskCacher& thumbsDiskCacher,
                            BrowsingNavigation& browsingNavigation,
                            ThumbsModel& thumbsModel,
                            FavoritFoldersModel& foldersModel);

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif

    QGuiApplication app(argc, argv);
    app.setOrganizationName("MediaOrganizer");
    app.setOrganizationDomain("MediaOrganizer.com");
    app.setApplicationName("Media Organizer");    

    QThread dirProcessorThread;
    dirProcessorThread.start();

    //don't delete, the QQmlEngine takes ownership of provider
    ThumbsProvider* thumbsProvider = new ThumbsProvider;

    DirProcessor dirProcessor(thumbsProvider);
    dirProcessor.moveToThread(&dirProcessorThread);

    ThumbsDiskCacher thumbsDiskCacher;
    thumbsDiskCacher.moveToThread(&dirProcessorThread);

    BrowsingNavigation browsingNavigation;
    ThumbsModel thumbsModel;
    FavoritFoldersModel foldersModel;

    makeObjectsConnections(*thumbsProvider, dirProcessor, thumbsDiskCacher,
                           browsingNavigation, thumbsModel, foldersModel);

    QQmlApplicationEngine engine;
    const QUrl url(QStringLiteral("qrc:/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);

    engine.rootContext()->setContextProperty("thumbsModel", &thumbsModel);
    engine.rootContext()->setContextProperty("foldersModel", &foldersModel);
    engine.rootContext()->setContextProperty("browsing", &browsingNavigation);
    engine.addImageProvider("thumb", thumbsProvider);

    engine.load(url);

    //pass winId to video thumbnails generator
    WId wid = 0;
    const QObject* m_rootObject = engine.rootObjects().constFirst();
    if(m_rootObject) {
        const QWindow *window = qobject_cast<const QWindow *>(m_rootObject);
        if(window) {
            wid = window->winId();
        }
    }
    VideoThumbnailGeneratorWin32::init((HWND)wid);

    //high dpi monitors support, pass biggest scale factor to dirProcessor for thumb scaling
    double scale = 0;
    foreach (const QScreen* screen, app.screens()){
        double curScale = screen->devicePixelRatio();
        if (curScale > scale) scale = curScale;
    }
    dirProcessor.setScale(scale);

    //running
    int res = app.exec();

    //stopping

    //save unsaved generated thumbs
    QMetaObject::invokeMethod(&thumbsDiskCacher, [&](){
        thumbsDiskCacher.saveThumbs();
    }, Qt::BlockingQueuedConnection);

    //cancel working thumbnail generators
    QMetaObject::invokeMethod(&dirProcessor, [&](){
        dirProcessor.cancel();
    }, Qt::BlockingQueuedConnection);
    dirProcessor.waitFinished();

    dirProcessorThread.terminate();
    dirProcessorThread.wait();

    return res;
}

void makeObjectsConnections(ThumbsProvider& thumbsProvider,
                            DirProcessor& dirProcessor,
                            ThumbsDiskCacher& thumbsDiskCacher,
                            BrowsingNavigation& browsingNavigation,
                            ThumbsModel& thumbsModel,
                            FavoritFoldersModel& foldersModel)
{
    //browsingHistory->dirProcessor
    QObject::connect(&browsingNavigation, &BrowsingNavigation::openFolder, [&](QString dirPath){
         QDesktopServices::openUrl(QUrl::fromLocalFile(dirPath));
    });    
    QObject::connect(&browsingNavigation, &BrowsingNavigation::restoreState, &dirProcessor,
                     [&](ThumbsViewState state){
        const QVariant& currentItem = state.currentItem;
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
        dirProcessor.getUnitedDirFilesAsync(pathes);
    });

    //dirProcessor->thumbsModel
    QObject::connect(&dirProcessor, &DirProcessor::fileProcessed, &thumbsModel, &ThumbsModel::showThumb, Qt::QueuedConnection );
    qRegisterMetaType<QList<ThumbData>>();
    QMetaType::registerComparators<ThumbData>();
    QObject::connect(&dirProcessor, &DirProcessor::getDirFilesAsyncResponce, &thumbsModel, [&](QStringList dirPathes, QList<ThumbData> sortedDirFiles, bool success, bool allHaveThumbs){
        thumbsModel.listReceived(dirPathes, sortedDirFiles, success);
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
    });
    //thumbsModel->dirProcessor
    qRegisterMetaType<ThumbsViewState>();
    QObject::connect(&thumbsModel, &ThumbsModel::itemSelected, &dirProcessor,
        [&](ThumbData td) {
            browsingNavigation.setSelectedPath(td.filePath);
            if (td.type == ThumbData::Folder) {

                int currentItemIndex = thumbsModel.getSelectedThumbIndex();
                auto parentFolderItems = thumbsModel.getThumbsList();
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
                dirProcessor.getUnitedDirFilesAsync({td.filePath});
            } else {
                QDesktopServices::openUrl(QUrl::fromLocalFile(td.filePath));
            }
    });


    qRegisterMetaType<FavoriteFolderData>();
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

    //=====Thumbs disk caching support
    //thumbs saving
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

    //thumbs loading
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
