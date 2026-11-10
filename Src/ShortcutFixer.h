#ifndef SHORTCUTFIXER_H
#define SHORTCUTFIXER_H

#include <QObject>
#include <QStringList>
#include <QMutex>
#include <QWaitCondition>
#include <atomic>

class ShortcutFixer : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(double progress READ progress NOTIFY scanProgressChanged)
    Q_PROPERTY(QString currentFolder READ currentFolder NOTIFY scanProgressChanged)

public:
    explicit ShortcutFixer(QObject *parent = nullptr);
    ~ShortcutFixer();

    bool isRunning() const { return running_; }
    double progress() const { return progress_; }
    QString currentFolder() const { return currentFolder_; }

public slots:
    void startRepair(QStringList shortcutPaths, QStringList targetNames, QStringList searchFolders);
    void acceptMatch(const QString &shortcutPath, const QString &newTarget);
    void fixAll(const QString &shortcutPath, const QString &newTarget);
    void skipMatch();
    void cancel();

signals:
    void scanProgressChanged(QString currentFolder, double progress);
    void matchFound(QString shortcutPath, QString targetName, QString candidatePath);
    void finished(int repairedCount);
    void runningChanged();
    void refreshRequested();

private:
    void doScan();

    QStringList shortcutPaths_;
    QStringList targetNames_;
    QStringList searchFolders_;

    QMutex mutex_;
    QWaitCondition waitCondition_;

    bool running_ = false;
    std::atomic<bool> cancelled_{false};
    std::atomic<bool> fixAllEnabled_{false};
    bool accepted_ = false;
    QString acceptedNewTarget_;
    double progress_ = 0;
    QString currentFolder_;
    int repairedCount_ = 0;
};

#endif // SHORTCUTFIXER_H
