#include "FavoritFoldersModel.h"
#include <QStandardPaths>

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
}

void FavoritFoldersModel::renameFolder(int pos, QString newName)
{
    if (pos < 0 || pos >= folders_.size()) {
        return;
    }
    folders_[pos].displayName = newName;
    emit dataChanged(index(pos), index(pos));
    saveSettings();
}

void FavoritFoldersModel::uniteWithFolder(int pos, QUrl folder)
{
    if (pos < 0 || pos >= folders_.size()) {
        return;
    }
    QDir dir(folder.toLocalFile());
    if (!folders_[pos].pathes.contains(dir.absolutePath())) {
        folders_[pos].pathes += dir.absolutePath();
        emit dataChanged(index(pos), index(pos));
        saveSettings();
    }
    itemClicked(pos);
}

QHash<int, QByteArray> FavoritFoldersModel::roleNames() const
{
    return {{DisplayName, "name"}};
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
    }
    return QVariant();
}
