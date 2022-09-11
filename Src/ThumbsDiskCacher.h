#ifndef THUMBSDISKCACHER_H
#define THUMBSDISKCACHER_H

#include <QObject>
#include <QImage>
#include <QHash>

#include "ThumbData.h"
class ThumbsDiskCacher : public QObject
{
    Q_OBJECT
public:
    explicit ThumbsDiskCacher(QObject *parent = nullptr);

public slots:
    //emits thumbLoaded, allThumbsLoaded
    void loadThumbs(QStringList dirPathes, QList<ThumbData> sortedFiles, bool lazy);
    void saveThumbs();
    void storeThumbInCache(QString path, QImage thumb);
    QImage getThumb(QString path);
    bool haveThumb(QString path);

    //save "newThumb" to parent folder thumbs file
    void setCurrentFolderThumb(QImage newThumb);

signals:
    void thumbLoaded(QString path, QImage thumb);
    void allThumbsLoaded(QStringList loadedPathes);

protected:
    void loadThumbsInternal(QString dirPath, bool forFolder, bool *success = nullptr);

    QHash<QString/*path*/, QImage> thumbs_;
    QList<ThumbData> currentSortedFiles_;
    QStringList currentDirPathes_;

    bool modified_ = false;           
    bool lazy_ = false;

    const QString thumbsFile_ = ".thumbs";
    const char* format_ = "jpg";
    const int quality_ = 80;
    const qint8 version_ = 1;

};

#endif // THUMBSDISKCACHER_H
