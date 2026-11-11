#ifndef FILESEARCHER_H
#define FILESEARCHER_H

#include <QObject>
#include <QHash>
#include <QMutex>
#include <QObject>
#include <QStringList>
#include <QVariantList>
#include <QVector>
#include <atomic>
#include <memory>

class StatusManager;

struct SearchEntry {
    QString path;   // full path, forward slashes
    QString lower;  // lower-cased file name, used for matching
    qint64 size = 0;
};

// Searches file names of all favorite folders. Every scanned folder is kept in
// an in-memory cache, so repeated searches do not touch the disks again unless
// "rescan" is requested.
class FileSearcher : public QObject
{
    Q_OBJECT

    Q_PROPERTY(bool running READ isRunning NOTIFY runningChanged)
    Q_PROPERTY(double progress READ progress NOTIFY progressChanged)
    Q_PROPERTY(QString currentPath READ currentPath NOTIFY progressChanged)
    Q_PROPERTY(QString query READ query NOTIFY queryChanged)
    Q_PROPERTY(int found READ found NOTIFY foundChanged)

public:
    explicit FileSearcher(StatusManager *status, QObject *parent = nullptr);
    ~FileSearcher();

    bool isRunning() const { return running_; }
    double progress() const { return progress_; }
    QString currentPath() const { return currentPath_; }
    QString query() const { return query_; }
    int found() const { return found_; }

    Q_INVOKABLE void start(const QString &text, bool rescan, const QStringList &folders);
    Q_INVOKABLE void cancel();

signals:
    void runningChanged();
    void progressChanged();
    void queryChanged();
    void foundChanged();
    void searchStarted(QString query);
    void hitsFound(QStringList paths);

private slots:
    void onHits(quint64 token, QStringList paths, QVariantList sizes);
    void onProgress(quint64 token, double progress, QString path);
    void onFinished(quint64 token, bool cancelled);

private:
    static QStringList uniqueRoots(const QStringList &folders);
    void updateStatus();

    StatusManager *status_;
    int found_ = 0;

    // cache: normalized root path -> all files below it
    QHash<QString, std::shared_ptr<QVector<SearchEntry>>> cache_;
    QMutex cacheMutex_;

    std::shared_ptr<std::atomic<bool>> cancelFlag_;
    quint64 token_ = 0;
    bool running_ = false;
    double progress_ = 0;
    QString currentPath_;
    QString query_;
};

#endif // FILESEARCHER_H
