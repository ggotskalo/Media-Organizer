#ifndef DUPLICATEFINDER_H
#define DUPLICATEFINDER_H

#include <QObject>
#include <QStringList>
#include <QMutex>
#include <QWaitCondition>
#include <QSet>
#include <atomic>

class DuplicateFinder : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString currentFolder READ currentFolder NOTIFY progressChanged)
    Q_PROPERTY(QString state READ state NOTIFY stateChanged)
    Q_PROPERTY(int totalGroups READ totalGroups NOTIFY scanFinished)
    Q_PROPERTY(int processedGroups READ processedGroups NOTIFY groupProcessed)
    Q_PROPERTY(int replacedCount READ replacedCount NOTIFY groupProcessed)

public:
    explicit DuplicateFinder(QObject *parent = nullptr);
    ~DuplicateFinder();

    bool isRunning() const { return running_; }
    double progress() const { return progress_; }
    QString currentFolder() const { return currentFolder_; }
    QString state() const { return state_; }
    int totalGroups() const { return duplicateGroups_.size(); }
    int processedGroups() const { return processedGroups_; }
    int replacedCount() const { return replacedCount_; }

public slots:
    void startScan(QStringList searchFolders);
    void keepFile(QString pathToKeep);
    void skipGroup();
    void cancel();

signals:
    void runningChanged();
    void progressChanged();
    void stateChanged();
    void scanFinished(int groupCount);
    void duplicateGroupFound(QString fileName, QString fileSize, QStringList paths);
    void groupProcessed();
    void finished(int replacedCount);

private:
    void doScan();

    QStringList searchFolders_;
    QList<QStringList> duplicateGroups_;
    QStringList duplicateGroupNames_;
    QList<qint64> duplicateGroupSizes_;

    QMutex mutex_;
    QWaitCondition waitCondition_;

    bool running_ = false;
    std::atomic<bool> cancelled_{false};
    bool accepted_ = false;
    QString keptPath_;
    double progress_ = 0;
    QString currentFolder_;
    QString state_;
    int processedGroups_ = 0;
    int replacedCount_ = 0;

    static const QSet<QString> videoExt_;
};

#endif // DUPLICATEFINDER_H
