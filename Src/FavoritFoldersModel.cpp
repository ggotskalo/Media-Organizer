#include <QStorageInfo>
#include "FavoritFoldersModel.h"
#include <QStandardPaths>
#include <QDir>

namespace {
int indexOfPath(const QStringList &paths, const QString &path)
{
    for (int i = 0; i < paths.size(); ++i) {
        if (QString::compare(paths.at(i), path, Qt::CaseInsensitive) == 0)
            return i;
    }
    return -1;
}
}

FavoritFoldersModel::FavoritFoldersModel(QObject *parent)
    : QAbstractListModel(parent)
{
    QString iniPath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "\\favoritFolders.ini";
    settings_ = new QSettings(iniPath, QSettings::IniFormat);
    loadSettings();
    if (folders_.isEmpty()) {
        folders_.append({"Pictures + Video",
                        {QStandardPaths::writableLocation(QStandardPaths::PicturesLocation),
                         QStandardPaths::writableLocation(QStandardPaths::MoviesLocation)
                        }
                       });
        folders_.append({"Downloads",
                        {QStandardPaths::writableLocation(QStandardPaths::DownloadLocation)}
                       });
        folders_.append({"Desktop",
                        {QStandardPaths::writableLocation(QStandardPaths::DesktopLocation)}
                       });
        saveSettings();
    }    
}

void FavoritFoldersModel::loadSettings()
{
    int size = settings_->beginReadArray("folders");
    for (int i = 0; i < size; ++i) {
        settings_->setArrayIndex(i);
        folders_.append({settings_->value("name").toString(), settings_->value("pathes").toStringList()});
    }
    settings_->endArray();
}

void FavoritFoldersModel::saveSettings() {
    settings_->clear();
    settings_->beginWriteArray("folders");
    int index = 0;
    for (const FavoriteFolderData& folder : qAsConst(folders_)) {
        settings_->setArrayIndex(index++);
        settings_->setValue("name", folder.displayName);
        settings_->setValue("pathes", folder.pathes);
    }
    settings_->endArray();
    settings_->sync();
}

FavoritFoldersModel::~FavoritFoldersModel()
{
    delete settings_;
}

void FavoritFoldersModel::itemClicked(int index) {
    if (index < 0 || index >= folders_.size()) {
        return;
    }
    FavoriteFolderData &ffd = folders_[index];
    emit itemSelected(ffd);
}

void FavoritFoldersModel::addFolder(QUrl folder) {

    QDir dir(folder.toLocalFile());
    beginInsertRows(QModelIndex(), folders_.size(), folders_.size());
    QString name = dir.dirName();
    folders_.append({name.isEmpty() ? dir.absolutePath() : name, {dir.absolutePath()}});
    endInsertRows();
    saveSettings();
    emit foldersChanged();
}

void FavoritFoldersModel::removeFolder(int index)
{
    if (index < 0 || index >= folders_.size()) {
        return;
    }
    beginRemoveRows(QModelIndex(), index, index);
    folders_.removeAt(index);
    endRemoveRows();
    saveSettings();
    emit foldersChanged();
}

void FavoritFoldersModel::renameFolder(int pos, QString newName)
{
    if (pos < 0 || pos >= folders_.size()) {
        return;
    }
    folders_[pos].displayName = newName;
    emit dataChanged(index(pos), index(pos));
    saveSettings();
    emit foldersChanged();
}

void FavoritFoldersModel::uniteWithFolder(int pos, QUrl folder)
{
    addPathToFolder(pos, folder);
}

QStringList FavoritFoldersModel::getFolderPaths(int index) const
{
    if (index < 0 || index >= folders_.size())
        return {};
    return folders_.at(index).pathes;
}

bool FavoritFoldersModel::addPathToFolder(int pos, QUrl folder)
{
    if (pos < 0 || pos >= folders_.size() || !folder.isValid())
        return false;

    const QString localPath = folder.toLocalFile();
    if (localPath.isEmpty())
        return false;

    QDir dir(localPath);
    const QString normalizedPath = dir.absolutePath();
    if (normalizedPath.isEmpty())
        return false;

    if (indexOfPath(folders_[pos].pathes, normalizedPath) >= 0) {
        itemClicked(pos);
        return false;
    }

    folders_[pos].pathes.append(normalizedPath);
    emit dataChanged(index(pos), index(pos));
    saveSettings();
    emit foldersChanged();
    itemClicked(pos);
    return true;
}

bool FavoritFoldersModel::removePathFromFolder(int pos, QString path)
{
    if (pos < 0 || pos >= folders_.size() || path.isEmpty())
        return false;

    if (folders_[pos].pathes.size() <= 1)
        return false;

    const QString normalizedPath = QDir(path).absolutePath();
    const int pathIndex = indexOfPath(folders_[pos].pathes, normalizedPath);
    if (pathIndex < 0)
        return false;

    folders_[pos].pathes.removeAt(pathIndex);
    emit dataChanged(index(pos), index(pos));
    saveSettings();
    emit foldersChanged();
    itemClicked(pos);
    return true;
}

QStringList FavoritFoldersModel::getAllFolderPaths() const
{
    QStringList result;
    for (const FavoriteFolderData& ffd : qAsConst(folders_)) {
        result.append(ffd.pathes);
    }
    return result;
}

void FavoritFoldersModel::refreshFreeSpace()
{
    if (folders_.isEmpty())
        return;
    emit dataChanged(index(0), index(folders_.size() - 1), {FreeSpace});
}

QHash<int, QByteArray> FavoritFoldersModel::roleNames() const
{
    return {{DisplayName, "name"}, {FreeSpace, "freeSpace"}};
}

int FavoritFoldersModel::rowCount(const QModelIndex &parent) const
{
    return folders_.size();
}

QVariant FavoritFoldersModel::data(const QModelIndex &index, int role) const
{
    if (index.row() < 0 || index.row() > folders_.count())
        return QVariant();
    switch (role) {
        case DisplayName:
            return folders_.at(index.row()).displayName;
        case FreeSpace: {
            // only for single-path favorites
            const QStringList& pathes = folders_.at(index.row()).pathes;
            if (pathes.size() != 1)
                return QString();
            const QStorageInfo storage(pathes.first());
            if (!storage.isValid() || !storage.isReady() || storage.bytesAvailable() < 0)
                return QString();
            const double gb = storage.bytesAvailable() / (1024.0 * 1024.0 * 1024.0);
            if (gb >= 1024.0)
                return tr("%1 TB free").arg(gb / 1024.0, 0, 'f', 2);
            return tr("%1 GB free").arg(gb, 0, 'f', gb >= 100 ? 0 : 1);
        }
    }
    return QVariant();
}
