#ifndef MOVEHELPER_H
#define MOVEHELPER_H

#include <QObject>
#include <QString>
#include <QVariantList>
#include <memory>
#include <atomic>

class CopyManager;

// Prepares "Move to ..." : finds other drives, estimates the size of the item
// and the free space on every drive, then starts the move through CopyManager.
class MoveHelper : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString sourcePath READ sourcePath NOTIFY changed)
    Q_PROPERTY(QString sizeText READ sizeText NOTIFY changed)
    Q_PROPERTY(bool calculating READ calculating NOTIFY changed)
    Q_PROPERTY(QVariantList targets READ targets NOTIFY changed)

public:
    explicit MoveHelper(CopyManager *copyManager, QObject *parent = nullptr);
    ~MoveHelper();

    QString sourcePath() const { return sourcePath_; }
    QString sizeText() const;
    bool calculating() const { return calculating_; }
    QVariantList targets() const { return targets_; }

    // srcRoots[i] -> destRoots[i]: the item lies in srcRoots[i], it can be moved to destRoots[i]
    void prepare(const QString &path, const QStringList &srcRoots, const QStringList &destRoots);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE bool move(int targetIndex);

signals:
    void changed();

private slots:
    void onSizeCalculated(quint64 token, qint64 size);

private:
    void rebuildTargets();

    CopyManager *copyManager_;
    QString sourcePath_;
    QStringList srcRoots_;
    QStringList destRoots_;
    qint64 size_ = 0;
    bool calculating_ = false;
    quint64 token_ = 0;
    std::shared_ptr<std::atomic<bool>> cancelFlag_;
    QVariantList targets_;
};

#endif // MOVEHELPER_H
