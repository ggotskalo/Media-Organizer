#ifndef STATUSMANAGER_H
#define STATUSMANAGER_H

#include <QObject>
#include <QString>

class StatusManager : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(bool isBusy READ isBusy NOTIFY isBusyChanged)

public:
    explicit StatusManager(QObject *parent = nullptr);

    QString statusText() const { return priorityText_.isEmpty() ? statusText_ : priorityText_; }
    bool isBusy() const { return isBusy_ || !priorityText_.isEmpty(); }

public slots:
    void setStatus(const QString& text);
    void clearStatus();
    // shown above the normal status, not overwritten by it (e.g. file search progress)
    void setPriorityStatus(const QString& text);
    void clearPriorityStatus();

signals:
    void statusTextChanged();
    void isBusyChanged();

private:
    QString statusText_;
    QString priorityText_;
    bool isBusy_;
};

#endif // STATUSMANAGER_H
