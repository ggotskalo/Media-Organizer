#include "StatusManager.h"

StatusManager::StatusManager(QObject *parent)
    : QObject(parent)
    , statusText_("")
    , isBusy_(false)
{
}

void StatusManager::setStatus(const QString& text)
{
    if (statusText_ != text) {
        statusText_ = text;
        isBusy_ = !text.isEmpty();
        if (priorityText_.isEmpty()) {
            emit statusTextChanged();
            emit isBusyChanged();
        }
    }
}

void StatusManager::setPriorityStatus(const QString& text)
{
    if (priorityText_ != text) {
        priorityText_ = text;
        emit statusTextChanged();
        emit isBusyChanged();
    }
}

void StatusManager::clearPriorityStatus()
{
    setPriorityStatus(QString());
}

void StatusManager::clearStatus()
{
    if (!statusText_.isEmpty() || isBusy_) {
        statusText_ = "";
        isBusy_ = false;
        emit statusTextChanged();
        emit isBusyChanged();
    }
}
