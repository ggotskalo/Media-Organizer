#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QScreen>
#include <QWindow>

#include "ComponentsMediator.h"
#include "DirProcessor.h"
#include "FavoritFoldersModel.h"
#include "ThumbsDiskCacher.h"
#include "ThumbsModel.h"
#include "ThumbsProvider.h"
#include "ThumbsViewState.h"
#include "Browsing/BrowsingNavigation.h"
#include "ThumbGenerators/VideoThumbnailGeneratorWin32.h"

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
    VideoThumbnailGeneratorWin32Factory factory;

    DirProcessor dirProcessor(thumbsProvider, factory);
    dirProcessor.moveToThread(&dirProcessorThread);

    ThumbsDiskCacher thumbsDiskCacher;
    thumbsDiskCacher.moveToThread(&dirProcessorThread);

    BrowsingNavigation browsingNavigation;
    ThumbsModel thumbsModel;
    FavoritFoldersModel foldersModel;

    ComponentsMediator componentsMediator(*thumbsProvider, dirProcessor, thumbsDiskCacher,
                           browsingNavigation, thumbsModel, foldersModel);
    componentsMediator.registerMetaTypes();
    componentsMediator.makeObjectsConnections();

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
    componentsMediator.finalize();
    dirProcessorThread.terminate();
    dirProcessorThread.wait();

    return res;
}
