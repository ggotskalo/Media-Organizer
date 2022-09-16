#ifndef DISKOPERATIONS_H
#define DISKOPERATIONS_H
#include "QString"

class DiskOperations
{
public:
    DiskOperations();
    static bool remove(QString path);
    static bool moveToParentFolder(QString path);
};

#endif // DISKOPERATIONS_H
