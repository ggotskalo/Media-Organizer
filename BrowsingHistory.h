#ifndef BROWSINGHISTORY_H
#define BROWSINGHISTORY_H

#include <QObject>
#include <QLinkedList>
#include <QVariant>

#include "ThumbData.h"
#include "FavoriteFolderData.h"

class BrowsingHistory : public QObject
{
    Q_OBJECT

public:
    explicit BrowsingHistory(QObject *parent = nullptr);

    Q_PROPERTY(QString currentPath MEMBER currentPath_ NOTIFY currentPathChanged)
    Q_PROPERTY(bool upEnabled MEMBER upEnabled_ NOTIFY upEnabledChanged)
    Q_PROPERTY(bool backEnabled MEMBER backEnabled_ NOTIFY backEnabledChanged)
    Q_PROPERTY(bool forwardEnabled MEMBER forwardEnabled_ NOTIFY forwardEnabledChanged)

    Q_INVOKABLE void currentClicked();
    Q_INVOKABLE void goBack(qreal scrollPosition);
    Q_INVOKABLE void goForward(qreal scrollPosition);
    Q_INVOKABLE void goUp(qreal scrollPosition);

    qreal getLastScrollPostiton() const {return lastScrollPosition_;}
    void clearLastScrollPostiton() {lastScrollPosition_ = 0;}

    QString getLastSelectedFile() const {return lastSelectedFile_;}
    void clearLastSelectedFile() {lastSelectedFile_.clear();}

    void fileSelected(const ThumbData &td, qreal scrollPosition);
    void favoriteFolderSelected(const FavoriteFolderData &ffd, qreal scrollPosition);

    void setCurrentPath(QString path);

signals:
    void currentPathChanged();
    void openFolder(QString dirPath);
    void goFile(ThumbData td);
    void goFavoriteFolder(FavoriteFolderData ffd);
    void upEnabledChanged();
    void backEnabledChanged();
    void forwardEnabledChanged();

protected:
    void updateEnabledDirections();
    void itemSelected(QVariant& v, qreal scrollPosition);

    struct HistoryRecord {
        QVariant item;
        qreal scrollPosition;
    };

    QString currentPath_;
    QLinkedList<HistoryRecord> history_;
    decltype(history_)::iterator currentPos_ = history_.end();
    bool upEnabled_ = false, backEnabled_ = false, forwardEnabled_ = false;
    qreal lastScrollPosition_ = 0;
    QString lastSelectedFile_;

};

#endif // BROWSINGHISTORY_H
