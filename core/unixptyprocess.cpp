#include "unixptyprocess.h"
#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
#include <QStandardPaths>
#else
#include <QDir>
#endif // QT_VERSION >= 5.0.0

#include <termios.h>
#include <errno.h>
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_FREEBSD)
#include <utmpx.h>
#endif
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <stdlib.h>
#include <QFileInfo>
#include <QCoreApplication>

UnixPtyProcess::UnixPtyProcess()
    : IPtyProcess()
    , m_readMasterNotify(0)
{
#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
    m_shellProcess.setWorkingDirectory(QStandardPaths::writableLocation(QStandardPaths::HomeLocation));
#else
    m_shellProcess.setWorkingDirectory(QDir::homePath());
#endif // QT_VERSION >= 5.0.0
}

UnixPtyProcess::~UnixPtyProcess()
{
    kill();
}

bool UnixPtyProcess::startProcess(const QString &shellPath, QStringList environment, qint16 cols, qint16 rows)
{
    if (!isAvailable())
    {
        m_lastError = QString("UnixPty Error: unavailable");
        return false;
    }

    if (m_shellProcess.state() == QProcess::Running)
        return false;

    QFileInfo fi(shellPath);
    if (fi.isRelative() || !QFile::exists(shellPath))
    {
        //todo add auto-find executable in PATH env var
        m_lastError = QString("UnixPty Error: shell file path must be absolute");
        return false;
    }

    m_shellPath = shellPath;
    m_size = QPair<qint16, qint16>(cols, rows);

    int rc = 0;

    m_shellProcess.m_handleMaster = ::posix_openpt(O_RDWR | O_NOCTTY);
    if (m_shellProcess.m_handleMaster <= 0)
    {
        m_lastError = QString("UnixPty Error: unable to open master -> %1").arg(strerror(errno));
        kill();
        return false;
    }

    m_shellProcess.m_handleSlaveName = ptsname(m_shellProcess.m_handleMaster);
    if ( m_shellProcess.m_handleSlaveName.isEmpty())
    {
        m_lastError = QString("UnixPty Error: unable to get slave name -> %1").arg(strerror(errno));
        kill();
        return false;
    }

    rc = grantpt(m_shellProcess.m_handleMaster);
    if (rc != 0)
    {
        m_lastError = QString("UnixPty Error: unable to change perms for slave -> %1").arg(strerror(errno));
        kill();
        return false;
    }

    rc = unlockpt(m_shellProcess.m_handleMaster);
    if (rc != 0)
    {
        m_lastError = QString("UnixPty Error: unable to unlock slave -> %1").arg(strerror(errno));
        kill();
        return false;
    }

    m_shellProcess.m_handleSlave = ::open(m_shellProcess.m_handleSlaveName.toLatin1().data(), O_RDWR | O_NOCTTY);
    if (m_shellProcess.m_handleSlave < 0)
    {
        m_lastError = QString("UnixPty Error: unable to open slave -> %1").arg(strerror(errno));
        kill();
        return false;
    }

    rc = fcntl(m_shellProcess.m_handleMaster, F_SETFD, FD_CLOEXEC);
    if (rc == -1)
    {
        m_lastError = QString("UnixPty Error: unable to set flags for master -> %1").arg(strerror(errno));
        kill();
        return false;
    }

    rc = fcntl(m_shellProcess.m_handleSlave, F_SETFD, FD_CLOEXEC);
    if (rc == -1)
    {
        m_lastError = QString("UnixPty Error: unable to set flags for slave -> %1").arg(strerror(errno));
        kill();
        return false;
    }

    struct ::termios ttmode;
    rc = tcgetattr(m_shellProcess.m_handleMaster, &ttmode);
    if (rc != 0)
    {
        m_lastError = QString("UnixPty Error: termios fail -> %1").arg(strerror(errno));
        kill();
        return false;
    }

    ttmode.c_iflag = ICRNL | IXON | IXANY | IMAXBEL | BRKINT;
#if defined(IUTF8)
    ttmode.c_iflag |= IUTF8;
#endif

    ttmode.c_oflag = OPOST | ONLCR;
    ttmode.c_cflag = CREAD | CS8 | HUPCL;
    ttmode.c_lflag = ICANON | ISIG | IEXTEN | ECHO | ECHOE | ECHOK | ECHOKE | ECHOCTL;

    ttmode.c_cc[VEOF] = 4;
    ttmode.c_cc[VEOL] = -1;
    ttmode.c_cc[VEOL2] = -1;
    ttmode.c_cc[VERASE] = 0x7f;
    ttmode.c_cc[VWERASE] = 23;
    ttmode.c_cc[VKILL] = 21;
    ttmode.c_cc[VREPRINT] = 18;
    ttmode.c_cc[VINTR] = 3;
    ttmode.c_cc[VQUIT] = 0x1c;
    ttmode.c_cc[VSUSP] = 26;
    ttmode.c_cc[VSTART] = 17;
    ttmode.c_cc[VSTOP] = 19;
    ttmode.c_cc[VLNEXT] = 22;
    ttmode.c_cc[VDISCARD] = 15;
    ttmode.c_cc[VMIN] = 1;
    ttmode.c_cc[VTIME] = 0;

#if (__APPLE__)
    ttmode.c_cc[VDSUSP] = 25;
    ttmode.c_cc[VSTATUS] = 20;
#endif

    cfsetispeed(&ttmode, B38400);
    cfsetospeed(&ttmode, B38400);

    rc = tcsetattr(m_shellProcess.m_handleMaster, TCSANOW, &ttmode);
    if (rc != 0)
    {
        m_lastError = QString("UnixPty Error: unabble to set associated params -> %1").arg(strerror(errno));
        kill();
        return false;
    }

    m_readMasterNotify = new QSocketNotifier(m_shellProcess.m_handleMaster, QSocketNotifier::Read, &m_shellProcess);
    m_readMasterNotify->setEnabled(true);
    m_readMasterNotify->moveToThread(m_shellProcess.thread());
#if (QT_VERSION >= QT_VERSION_CHECK(5, 0, 0))
    QObject::connect(m_readMasterNotify, &QSocketNotifier::activated, [this]()
    {
        readFromMaster();
    });
#else
    QObject::connect(m_readMasterNotify, SIGNAL(activated(int)), this, SLOT(onSocketActivated(int)));
#endif

    QStringList defaultVars;

    defaultVars.append("TERM=xterm-256color");
    defaultVars.append("ITERM_PROFILE=Default");
    defaultVars.append("XPC_FLAGS=0x0");
    defaultVars.append("XPC_SERVICE_NAME=0");
    defaultVars.append("LANG=en_US.UTF-8");
    defaultVars.append("LC_ALL=en_US.UTF-8");
    defaultVars.append("LC_CTYPE=UTF-8");
    defaultVars.append("COMMAND_MODE=unix2003");
    defaultVars.append("COLORTERM=truecolor");

    Q_UNUSED(environment);
    QProcessEnvironment envFormat;
    foreach (QString line, defaultVars)
    {
        envFormat.insert(line.split("=").first(), line.split("=").last());
    }
    m_shellProcess.setProcessEnvironment(envFormat);
    m_shellProcess.setReadChannel(QProcess::StandardOutput);
    m_shellProcess.start(m_shellPath, QStringList());
    m_shellProcess.waitForStarted();

    #if (QT_VERSION >= QT_VERSION_CHECK(5, 3, 0))
    m_pid = m_shellProcess.processId();
#else
    m_pid = m_shellProcess.pid();
#endif // QT_VERSION >= 5.3.0

    if (m_pid > 0)
        writeUtmpxRecord();

    resize(cols, rows);

    // The shell has its own copies of the slave now. Closing ours makes read() on the master
    // fail with EIO once the shell (and whatever it started) has exited, instead of blocking
    if (m_shellProcess.state() == QProcess::Running)
    {
        ::close(m_shellProcess.m_handleSlave);
        m_shellProcess.m_handleSlave = -1;
    }

    return true;
}

#if (QT_VERSION < QT_VERSION_CHECK(5, 0, 0))
void UnixPtyProcess::onSocketActivated(int socket)
{
    Q_UNUSED(socket)

    readFromMaster();
}
#endif // QT_VERSION < 5.0.0

void UnixPtyProcess::readFromMaster()
{
    // The master is blocking: read once per activation, the notifier fires again while data is pending
    char buffer[4096];
    ssize_t len = ::read(m_shellProcess.m_handleMaster, buffer, sizeof(buffer));
    if (len <= 0)
    {
        // EOF, or EIO once every slave fd is closed (the shell has exited): stop the notifier,
        // it would fire again and again
        if (len == 0 || (errno != EINTR && errno != EAGAIN))
            m_readMasterNotify->setEnabled(false);
        return;
    }

    m_shellReadBuffer.append(buffer, len);
    m_shellProcess.emitReadyRead();
}

void UnixPtyProcess::writeUtmpxRecord()
{
#if !defined(Q_OS_ANDROID) && !defined(Q_OS_FREEBSD)
    // on Android imposible to put record to the 'utmp' file
    // Written by the parent: the child process modifier runs between fork and exec,
    // where neither Qt nor the utmpx functions are safe to call
    struct utmpx utmpxInfo;
    memset(&utmpxInfo, 0, sizeof(utmpxInfo));

    strncpy(utmpxInfo.ut_user, qgetenv("USER").constData(), sizeof(utmpxInfo.ut_user));

    QString device(m_shellProcess.m_handleSlaveName);
    if (device.startsWith("/dev/"))
        device = device.mid(5);

    const QByteArray deviceName = device.toLatin1();
    const char *d = deviceName.constData();

    strncpy(utmpxInfo.ut_line, d, sizeof(utmpxInfo.ut_line));

    if (strlen(d) >= sizeof(utmpxInfo.ut_id))
        strncpy(utmpxInfo.ut_id, d + strlen(d) - sizeof(utmpxInfo.ut_id), sizeof(utmpxInfo.ut_id));

    struct timeval tv;
    gettimeofday(&tv, 0);
    utmpxInfo.ut_tv.tv_sec = tv.tv_sec;
    utmpxInfo.ut_tv.tv_usec = tv.tv_usec;

    utmpxInfo.ut_type = USER_PROCESS;
    utmpxInfo.ut_pid = static_cast<pid_t>(m_pid);

    utmpxname(_PATH_UTMPX);
    setutxent();
    pututxline(&utmpxInfo);
    endutxent();
#endif
}

bool UnixPtyProcess::resize(qint16 cols, qint16 rows)
{
    struct winsize winp;
    winp.ws_col = cols;
    winp.ws_row = rows;
    winp.ws_xpixel = 0;
    winp.ws_ypixel = 0;

    // Setting the size on the master applies it to the terminal (the slave is closed once the shell has started)
    bool res = (ioctl(m_shellProcess.m_handleMaster, TIOCSWINSZ, &winp) != -1);

    if (res)
    {
        m_size = QPair<qint16, qint16>(cols, rows);
    }

    return res;
}

bool UnixPtyProcess::kill()
{
    m_shellProcess.m_handleSlaveName = QString();
    if (m_shellProcess.m_handleSlave >= 0)
    {
        ::close(m_shellProcess.m_handleSlave);
        m_shellProcess.m_handleSlave = -1;
    }
    if (m_shellProcess.m_handleMaster >= 0)
    {
        ::close(m_shellProcess.m_handleMaster);
        m_shellProcess.m_handleMaster = -1;
    }

    if (m_shellProcess.state() == QProcess::Running)
    {
        m_readMasterNotify->disconnect();
        m_readMasterNotify->deleteLater();

        m_shellProcess.terminate();
        m_shellProcess.waitForFinished(1000);

        if (m_shellProcess.state() == QProcess::Running)
        {
            m_shellProcess.kill();
            m_shellProcess.waitForFinished(1000);
        }

        return (m_shellProcess.state() == QProcess::NotRunning);
    }
    return false;
}

IPtyProcess::PtyType UnixPtyProcess::type() const
{
    return IPtyProcess::UnixPty;
}

QString UnixPtyProcess::dumpDebugInfo()
{
#ifdef PTYQT_DEBUG
    return QString("PID: %1, In: %2, Out: %3, Type: %4, Cols: %5, Rows: %6, IsRunning: %7, Shell: %8, SlaveName: %9")
            .arg(m_pid).arg(m_shellProcess.m_handleMaster).arg(m_shellProcess.m_handleSlave).arg(type())
            .arg(m_size.first).arg(m_size.second).arg(m_shellProcess.state() == QProcess::Running)
            .arg(m_shellPath).arg(m_shellProcess.m_handleSlaveName);
#else
    return QString("Nothing...");
#endif
}

QIODevice *UnixPtyProcess::notifier()
{
    return &m_shellProcess;
}

QByteArray UnixPtyProcess::readAll()
{
    QByteArray tmpBuffer =  m_shellReadBuffer;
    m_shellReadBuffer.clear();
    return tmpBuffer;
}

qint64 UnixPtyProcess::write(const QByteArray &byteArray)
{
    int result = ::write(m_shellProcess.m_handleMaster, byteArray.constData(), byteArray.size());
    Q_UNUSED(result)

    return byteArray.size();
}

bool UnixPtyProcess::isAvailable()
{
	//todo check something more if required
    return true;
}

void UnixPtyProcess::moveToThread(QThread *targetThread)
{
    m_shellProcess.moveToThread(targetThread);
}

void ShellProcess::setupPtyChildProcess()
{
    // Runs between fork and exec: only async-signal-safe calls here
    dup2(m_handleSlave, STDIN_FILENO);
    dup2(m_handleSlave, STDOUT_FILENO);
    dup2(m_handleSlave, STDERR_FILENO);

    pid_t sid = setsid();
    ioctl(m_handleSlave, TIOCSCTTY, 0);
    tcsetpgrp(m_handleSlave, sid);

    setuid(0);
}
