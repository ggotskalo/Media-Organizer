#include <algorithm>
#include <QDesktopServices>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QScreen>
#include <QThreadPool>
#include <QWindow>

#include "BrowsingHistory.h"
#include "DirProcessor.h"
#include "FavoritFoldersModel.h"
#include "ThumbsDiskCacher.h"
#include "ThumbsModel.h"
#include "ThumbsProvider.h"

void makeObjectsConnections(ThumbsProvider& thumbsProvider,
                            DirProcessor& dirProcessor,
                            ThumbsDiskCacher& thumbsDiskCacher,
                            BrowsingHistory& browsingHistory,
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

    BrowsingHistory browsingHistory;
    ThumbsModel thumbsModel;
    FavoritFoldersModel foldersModel;

    makeObjectsConnections(*thumbsProvider, dirProcessor, thumbsDiskCacher,
                           browsingHistory, thumbsModel, foldersModel);

    QQmlApplicationEngine engine;
    const QUrl url(QStringLiteral("qrc:/Qml/main.qml"));
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreated,
                     &app, [url](QObject *obj, const QUrl &objUrl) {
        if (!obj && url == objUrl)
            QCoreApplication::exit(-1);
    }, Qt::QueuedConnection);

    engine.rootContext()->setContextProperty("thumbsModel", &thumbsModel);
    engine.rootContext()->setContextProperty("foldersModel", &foldersModel);
    engine.rootContext()->setContextProperty("browsingHistory", &browsingHistory);
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
                            BrowsingHistory& browsingHistory,
                            ThumbsModel& thumbsModel,
                            FavoritFoldersModel& foldersModel)
{
    //browsingHistory->dirProcessor
    QObject::connect(&browsingHistory, &BrowsingHistory::openFolder, [&](QString dirPath){
         QDesktopServices::openUrl(QUrl::fromLocalFile(dirPath));
    });
    QObject::connect(&browsingHistory, &BrowsingHistory::goFile, &dirProcessor, [&](ThumbData td){
        dirProcessor.getUnitedDirFilesAsync({td.filePath});
    });
    QObject::connect(&browsingHistory, &BrowsingHistory::goFavoriteFolder, &dirProcessor, [&](FavoriteFolderData ffd){
        dirProcessor.getUnitedDirFilesAsync(ffd.pathes);
    });

    //dirProcessor->thumbsModel
    QObject::connect(&dirProcessor, &DirProcessor::fileProcessed, &thumbsModel, &ThumbsModel::showThumb, Qt::QueuedConnection );
    qRegisterMetaType<QList<ThumbData>>();
    QMetaType::registerComparators<ThumbData>();
    QObject::connect(&dirProcessor, &DirProcessor::getDirFilesAsyncResponce, &thumbsModel, [&](QStringList dirPathes, QList<ThumbData> sortedDirFiles, bool success, bool allHaveThumbs){
        thumbsModel.listReceived(dirPathes, sortedDirFiles, success);
        qreal newScrollPosition = browsingHistory.getLastScrollPostiton();
        QString lastSelectedFile = browsingHistory.getLastSelectedFile();
        if (newScrollPosition !=0 || !lastSelectedFile.isEmpty()) {
            auto foundIt = std::find_if(sortedDirFiles.cbegin(), sortedDirFiles.cend(), [lastSelectedFile](const ThumbData& td){
                return td.filePath == lastSelectedFile;
            });
            int pos = foundIt == sortedDirFiles.cend() ? -1 : foundIt - sortedDirFiles.cbegin();
            emit thumbsModel.showItem(pos, newScrollPosition);
            browsingHistory.clearLastScrollPostiton();
            browsingHistory.clearLastSelectedFile();
        } else {
            emit thumbsModel.clearSelection();
        }
    });
    //thumbsModel->dirProcessor
    QObject::connect(&thumbsModel, &ThumbsModel::itemSelected, &dirProcessor,
        [&](ThumbData td, qreal scrollPosition) {
            if (td.type == ThumbData::Folder) {
                browsingHistory.fileSelected(td, scrollPosition);
                dirProcessor.getUnitedDirFilesAsync({td.filePath});
            } else {
                QDesktopServices::openUrl(QUrl::fromLocalFile(td.filePath));
            }
    });


    qRegisterMetaType<FavoriteFolderData>();
    //foldersModel->dirProcessor
    QObject::connect(&foldersModel, &FavoritFoldersModel::itemSelected, &dirProcessor,
        [&](FavoriteFolderData ffd, qreal scrollPosition) {
            browsingHistory.favoriteFolderSelected(ffd, scrollPosition);
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
