#include "ptyqt.h"
#include <utility>

#ifdef Q_OS_WIN
#include "winptyprocess.h"
#include "conptyprocess.h"
#endif

#ifdef Q_OS_UNIX
#include "unixptyprocess.h"
#endif

IPtyProcess *PtyQt::createPtyProcess(IPtyProcess::PtyType ptyType)
{
    switch (ptyType)
    {
#ifdef Q_OS_WIN
#if __cplusplus >= 201103L
    case IPtyProcess::PtyType::WinPty:
#else
    case IPtyProcess::WinPty:
#endif
        return new WinPtyProcess();
        break;
#if __cplusplus >= 201103L
    case IPtyProcess::PtyType::ConPty:
#else
    case IPtyProcess::ConPty:
#endif
        return new ConPtyProcess();
        break;
#endif
#ifdef Q_OS_UNIX
#if __cplusplus >= 201103L
    case IPtyProcess::PtyType::UnixPty:
#else
    case IPtyProcess::UnixPty:
#endif
        return new UnixPtyProcess();
        break;
#endif
#if __cplusplus >= 201103L
    case IPtyProcess::PtyType::AutoPty:
#else
    case IPtyProcess::AutoPty:
#endif
    default:
        break;
    }

#ifdef Q_OS_WIN
    if (ConPtyProcess().isAvailable())
        return new ConPtyProcess();
    else
        return new WinPtyProcess();
#endif
#ifdef Q_OS_UNIX
    return new UnixPtyProcess();
#endif
}
