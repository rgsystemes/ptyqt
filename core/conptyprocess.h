#ifndef CONPTYPROCESS_H
#define CONPTYPROCESS_H

#include "iptyprocess.h"
#include <windows.h>
#include <process.h>
#include <QLibrary>
#include <stdio.h>
#include <QMutex>
#include <QTimer>
#include <QThread>
#include <QStringList>
#include <QCoreApplication>

#include "../lumwrap_win.h"
GPA_WRAP(
    Kernel32.dll,
    DeleteProcThreadAttributeList,
    (LPPROC_THREAD_ATTRIBUTE_LIST lpAttributeList),
    (lpAttributeList),
    WINAPI,
    void,
    void()
);

//Taken from the RS5 Windows SDK, but redefined here in case we're targeting <= 17733
//Just for compile, ConPty doesn't work with Windows SDK < 17733
#ifndef PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE
#define PROC_THREAD_ATTRIBUTE_PSEUDOCONSOLE \
  ProcThreadAttributeValue(22, FALSE, TRUE, FALSE)

typedef VOID* HPCON;

#define TOO_OLD_WINSDK
#endif

template <typename T>
std::vector<T> vectorFromString(const std::basic_string<T> &str)
{
    return std::vector<T>(str.begin(), str.end());
}

//ConPTY available only on Windows 10 releazed after 1903 (19H1) Windows release
class WindowsContext
{
public:
    typedef HRESULT (WINAPI *CreatePseudoConsolePtr)(
            COORD size,         // ConPty Dimensions
            HANDLE hInput,      // ConPty Input
            HANDLE hOutput,	    // ConPty Output
            DWORD dwFlags,      // ConPty Flags
            HPCON* phPC);       // ConPty Reference

    typedef HRESULT (WINAPI *ResizePseudoConsolePtr)(HPCON hPC, COORD size);

    typedef VOID (WINAPI *ClosePseudoConsolePtr)(HPCON hPC);

    WindowsContext()
        : createPseudoConsole(nullptr)
        , resizePseudoConsole(nullptr)
        , closePseudoConsole(nullptr)
    {

    }

    bool init()
    {
        //already initialized
        if (createPseudoConsole)
            return true;

        //try to load symbols from library
        //if it fails -> we can't use ConPty API
        HANDLE kernel32Handle = LoadLibraryExW(L"kernel32.dll", 0, 0);

        if (kernel32Handle != nullptr)
        {
            createPseudoConsole = (CreatePseudoConsolePtr)GetProcAddress((HMODULE)kernel32Handle, "CreatePseudoConsole");
            resizePseudoConsole = (ResizePseudoConsolePtr)GetProcAddress((HMODULE)kernel32Handle, "ResizePseudoConsole");
            closePseudoConsole = (ClosePseudoConsolePtr)GetProcAddress((HMODULE)kernel32Handle, "ClosePseudoConsole");
            if (createPseudoConsole == NULL || resizePseudoConsole == NULL || closePseudoConsole == NULL)
            {
                m_lastError = QString("WindowsContext/ConPty error: %1").arg("Invalid on load API functions");
                return false;
            }
        }
        else
        {
            m_lastError = QString("WindowsContext/ConPty error: %1").arg("Unable to load kernel32");
            return false;
        }

        return true;
    }

    QString lastError()
    {
        return m_lastError;
    }

public:
    //vars
    CreatePseudoConsolePtr createPseudoConsole;
    ResizePseudoConsolePtr resizePseudoConsole;
    ClosePseudoConsolePtr closePseudoConsole;

private:
    QString m_lastError;
};

class PtyBuffer : public QIODevice
{
    friend class ConPtyProcess;
    Q_OBJECT
public:

    PtyBuffer() {  }
    ~PtyBuffer() { }

    //just empty realization, we need only 'readyRead' signal of this class
    qint64 readData(char *data, qint64 maxlen) { Q_UNUSED(data); Q_UNUSED(maxlen); return 0; }
    qint64 writeData(const char *data, qint64 len) { Q_UNUSED(data); Q_UNUSED(len); return 0; }

    bool   isSequential() { return true; }
    qint64 bytesAvailable() { return m_readBuffer.size(); }
    qint64 size() { return m_readBuffer.size(); }

    void emitReadyRead();

private slots:
    void onReadyRead();

public:
    QByteArray m_readBuffer;
};

class ConPtyProcessThread : public QThread
{
    Q_OBJECT
public:
    ConPtyProcessThread(HANDLE hPipeIn, QMutex * bufferMutexPointer, PtyBuffer * bufferPointer, PROCESS_INFORMATION piClient, STARTUPINFOEX startupInfo, QObject * parent)
        : QThread(parent),
          m_hPipeIn(hPipeIn),
          m_bufferMutexPointer(bufferMutexPointer),
          m_bufferPointer(bufferPointer),
          m_piClient(piClient),
          m_startupInfo(startupInfo),
          m_isInterruptionRequested(false)
    {
    }

    void run()
    {
        forever
        {
            //buffers
            const DWORD BUFF_SIZE{ 512 };
            char szBuffer[BUFF_SIZE]{};

            //DWORD dwBytesWritten{};
            DWORD dwBytesRead{};
            BOOL fRead{ FALSE };

            // Read from the pipe
            fRead = ReadFile(m_hPipeIn, szBuffer, BUFF_SIZE, &dwBytesRead, NULL);

            {
                QMutexLocker locker(m_bufferMutexPointer);
                m_bufferPointer->m_readBuffer.append(szBuffer, dwBytesRead);
                m_bufferPointer->emitReadyRead();
            }

            if (m_isInterruptionRequested)
                break;

            QCoreApplication::processEvents();
        }

        // Now safe to clean-up client app's process-info & thread
        CloseHandle(m_piClient.hThread);
        CloseHandle(m_piClient.hProcess);

        // Cleanup attribute list
        DeleteProcThreadAttributeListWrap(m_startupInfo.lpAttributeList);
        //free(startupInfo.lpAttributeList);
    }

private slots:
    void onInterruptionRequested()
    {
        m_isInterruptionRequested = true;
        msleep(200);
    }

private:
    HANDLE m_hPipeIn;
    QMutex * m_bufferMutexPointer;
    PtyBuffer * m_bufferPointer;
    PROCESS_INFORMATION m_piClient;
    STARTUPINFOEX m_startupInfo;
    bool m_isInterruptionRequested;
};

class ConPtyProcess : public IPtyProcess
{
    Q_OBJECT
public:
    ConPtyProcess();
    ~ConPtyProcess();

    bool startProcess(const QString &shellPath, QStringList environment, qint16 cols, qint16 rows);
    bool resize(qint16 cols, qint16 rows);
    bool kill();
    PtyType type() const;
    QString dumpDebugInfo();
    virtual QIODevice *notifier();
    virtual QByteArray readAll();
    virtual qint64 write(const QByteArray &byteArray);
    bool isAvailable();
    void moveToThread(QThread *targetThread);

signals:
    void requestInterruption();

private:
    HRESULT createPseudoConsoleAndPipes(HPCON* phPC, HANDLE* phPipeIn, HANDLE* phPipeOut, qint16 cols, qint16 rows);
    HRESULT initializeStartupInfoAttachedToPseudoConsole(STARTUPINFOEX* pStartupInfo, HPCON hPC);

private:
    WindowsContext m_winContext;
    HPCON m_ptyHandler;
    HANDLE m_hPipeIn, m_hPipeOut;

    QThread *m_readThread;
    QMutex m_bufferMutex;
    PtyBuffer m_buffer;

};

#endif // CONPTYPROCESS_H
