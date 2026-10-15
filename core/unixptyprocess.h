#ifndef UNIXPTYPROCESS_H
#define UNIXPTYPROCESS_H

#include "iptyprocess.h"
#include <QProcess>
#include <QSocketNotifier>


// support for build with MUSL on Alpine Linux
#ifndef _PATH_UTMPX
#include <sys/time.h>
# define _PATH_UTMPX	"/var/log/utmp"
#endif

class ShellProcess : public QProcess
{
    friend class UnixPtyProcess;
    Q_OBJECT
public:
    ShellProcess( )
        : QProcess( )
        , m_handleMaster(-1)
        , m_handleSlave(-1)
    {
        setProcessChannelMode(QProcess::SeparateChannels);
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
        // QProcess::setupChildProcess() has been replaced by a modifier in Qt 6
        setChildProcessModifier([this]() { setupPtyChildProcess(); });
#endif // QT_VERSION >= 6.0.0
    }

    void emitReadyRead()
    {
        emit readyRead();
    }

protected:
    void setupPtyChildProcess();
#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)) && (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
    virtual void setupChildProcess() Q_DECL_OVERRIDE { setupPtyChildProcess(); }
#elif (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
    virtual void setupChildProcess() { setupPtyChildProcess(); }
#endif

private:
    int m_handleMaster, m_handleSlave;
    QString m_handleSlaveName;
};

class UnixPtyProcess : public IPtyProcess
{
    Q_OBJECT
public:
    UnixPtyProcess();
    virtual ~UnixPtyProcess();

    virtual bool startProcess(const QString &shellPath, QStringList environment, qint16 cols, qint16 rows);
    virtual bool resize(qint16 cols, qint16 rows);
    virtual bool kill();
    virtual PtyType type() const;
    virtual QString dumpDebugInfo();
    virtual QIODevice *notifier();
    virtual QByteArray readAll();
    virtual qint64 write(const QByteArray &byteArray);
    virtual bool isAvailable();
    void moveToThread(QThread *targetThread);

#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
private slots:
    void onSocketActivated(int socket);
#endif // QT_VERSION < 5.0.0

private:
    void readFromMaster();
    void writeUtmpxRecord();

    ShellProcess m_shellProcess;
    QSocketNotifier *m_readMasterNotify;
    QByteArray m_shellReadBuffer;

};

#endif // UNIXPTYPROCESS_H
