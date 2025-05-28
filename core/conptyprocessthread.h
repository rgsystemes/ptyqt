#ifndef CONPTYPROCESSTHREAD_H
#define CONPTYPROCESSTHREAD_H

#include <QObject>
#include <QThread>
#include <QMutexLocker>
#include <QCoreApplication>
#include "..\lumwrap_win.h"

GPA_WRAP(
    Kernel32.dll,
    DeleteProcThreadAttributeList,
    (LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList),
    (lpAttributeList),
    WINAPI,
    void,
    void()
);

#endif // CONPTYPROCESSTHREAD_H
