#ifndef COPYMANAGER_H
#define COPYMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>
#include <atomic>

// Copies/moves files from the clipboard on a worker thread and reports progress.
class CopyManager : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString currentFile READ currentFile NOTIFY progressChanged)
    Q_PROPERTY(bool moving READ isMoving NOTIFY runningChanged)
    Q_PROPERTY(QString summary READ summary NOTIFY progressChanged)

public:
    explicit CopyManager(QObject *parent = nullptr);
    ~CopyManager();

    bool isRunning() const { return running_; }
    double progress() const { return progress_; }
    QString currentFile() const { return currentFile_; }
    QString summary() const { return summary_; }
    bool isMoving() const { return moving_; }

    // Returns false if nothing was started.
    bool start(const QStringList &sources, const QString &destDir, bool isCut, bool clearClipboard = true, bool merge = false);

public slots:
    void cancel();

signals:
    void runningChanged();
    void progressChanged();
    void finished(QString destDir, int pastedCount);

private slots:
    void onProgress(double progress, QString file, QString summary);
    void onWorkerFinished(int pastedCount);

private:
    bool running_ = false;
    bool moving_ = false;
    double progress_ = 0;
    QString currentFile_;
    QString summary_;
    QString destDir_;
    std::atomic<bool> cancelled_{false};
};

#endif // COPYMANAGER_H
