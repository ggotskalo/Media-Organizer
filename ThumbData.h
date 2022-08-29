#ifndef THUMBDATA_H
#define THUMBDATA_H

#include <QString>
#include <QObject>

struct ThumbData {
    enum Type {
        Video,
        Picture,
        Folder,
        Unsupported
    };
    bool operator==(const ThumbData& td) const {return td.filePath == filePath;}
    bool operator<(const ThumbData& td) const {return td.filePath < filePath;}
    QString displayName;
    QString filePath;
    Type type;
    QString thumbSource;
    qint64 size = 0;
    bool noThumb = false;
};

Q_DECLARE_METATYPE(ThumbData)
#endif // THUMBDATA_H
