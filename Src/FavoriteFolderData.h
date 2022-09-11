#ifndef FAVORITEFOLDERDATA_H
#define FAVORITEFOLDERDATA_H
#include <QObject>
#include <QString>
#include <QStringList>

struct FavoriteFolderData {
    QString displayName;
    QStringList pathes;
};

Q_DECLARE_METATYPE(FavoriteFolderData)
#endif // FAVORITEFOLDERDATA_H
