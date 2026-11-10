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

    QString statusText() const { return statusText_; }
    bool isBusy() const { return isBusy_; }

public slots:
    void setStatus(const QString& text);
    void clearStatus();

signals:
    void statusTextChanged();
    void isBusyChanged();

private:
    QString statusText_;
    bool isBusy_;
};

#endif // STATUSMANAGER_H
