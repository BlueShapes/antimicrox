#include "nativegameinputmonitor.h"

#include <QDebug>
#include <QScopedValueRollback>
#ifdef Q_OS_WIN
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
    #include <sddl.h>

namespace {
QString processSid(HANDLE process)
{
    HANDLE token = nullptr;
    if (!OpenProcessToken(process, TOKEN_QUERY, &token))
        return {};
    DWORD bytes = 0;
    GetTokenInformation(token, TokenUser, nullptr, 0, &bytes);
    QByteArray storage(static_cast<int>(bytes), '\0');
    QString result;
    if (bytes && GetTokenInformation(token, TokenUser, storage.data(), bytes, &bytes))
    {
        LPWSTR sid = nullptr;
        if (ConvertSidToStringSidW(reinterpret_cast<TOKEN_USER *>(storage.data())->User.Sid, &sid))
        {
            result = QString::fromWCharArray(sid);
            LocalFree(sid);
        }
    }
    CloseHandle(token);
    return result;
}

bool alive(HANDLE process)
{
    return process && WaitForSingleObject(process, 0) == WAIT_TIMEOUT;
}
}
#endif

NativeGameInputMonitor::NativeGameInputMonitor(QObject *parent) : QObject(parent)
{
    server.setSocketOptions(QLocalServer::UserAccessOption);
    server.setMaxPendingConnections(8);
    timer.setInterval(100);
    connect(&server, &QLocalServer::newConnection, this, &NativeGameInputMonitor::acceptConnections);
    connect(&timer, &QTimer::timeout, this, &NativeGameInputMonitor::recalculate);
}

NativeGameInputMonitor::~NativeGameInputMonitor() { close(); }

QString NativeGameInputMonitor::endpointName()
{
#ifdef Q_OS_WIN
    const QString sid = processSid(GetCurrentProcess());
    DWORD session = 0;
    if (!sid.isEmpty() && ProcessIdToSessionId(GetCurrentProcessId(), &session))
        return QStringLiteral("antimicroxDeltaControllableV1-%1-%2").arg(sid).arg(session);
#endif
    return {};
}

void NativeGameInputMonitor::setEnabled(bool enabled)
{
    if (!enabled)
    {
        close();
        return;
    }
#ifdef Q_OS_WIN
    if (server.isListening())
        return;
    const QString name = endpointName();
    if (name.isEmpty() || !server.listen(name))
    {
        qWarning() << "Controllable Delta integration unavailable:" << server.errorString();
        return;
    }
    timer.start();
#endif
}

void NativeGameInputMonitor::close()
{
    timer.stop();
    server.close();
    for (QLocalSocket *socket : connections.keys())
        removeConnection(socket);
    state.clear();
    if (m_suspended)
    {
        m_suspended = false;
        emit suspensionChanged(false);
    }
}

void NativeGameInputMonitor::acceptConnections()
{
    while (server.hasPendingConnections())
    {
        QLocalSocket *socket = server.nextPendingConnection();
        if (!socket)
            break;
#ifdef Q_OS_WIN
        ULONG pid = 0;
        DWORD peerSession = 0, ourSession = 0;
        // This is a native pipe HANDLE on Windows, verified by the focused IPC test.
        const HANDLE pipe = reinterpret_cast<HANDLE>(socket->socketDescriptor());
        HANDLE process = nullptr;
        const QString ourSid = processSid(GetCurrentProcess());
        const bool peerValid = connections.size() < 8 && GetNamedPipeClientProcessId(pipe, &pid) && pid &&
            ProcessIdToSessionId(pid, &peerSession) && ProcessIdToSessionId(GetCurrentProcessId(), &ourSession) &&
            peerSession == ourSession &&
            (process = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid)) &&
            alive(process) && !ourSid.isEmpty() && processSid(process) == ourSid;
        if (peerValid)
        {
            socket->setReadBufferSize(8192);
            connections.insert(socket, {++nextConnection, static_cast<quint32>(pid), uptimeMs(), false, {}, process});
            connect(socket, &QLocalSocket::readyRead, this, [this, socket] { readConnection(socket); });
            connect(socket, &QLocalSocket::disconnected, this, [this, socket] { removeConnection(socket); });
            readConnection(socket);
            continue;
        }
        if (process)
            CloseHandle(process);
#endif
        socket->abort();
        socket->deleteLater();
    }
}

void NativeGameInputMonitor::readConnection(QLocalSocket *socket)
{
    auto it = connections.find(socket);
    if (it == connections.end())
        return;
    int frames = 0, bytes = 0;
    while (socket->bytesAvailable() > 0)
    {
        const QByteArray chunk = socket->read(NativeGameInputState::MaxFrameBytes);
        bytes += chunk.size();
        if (bytes > 16384)
        {
            removeConnection(socket);
            return;
        }
        it->buffer.append(chunk);
        int end;
        while ((end = it->buffer.indexOf('\n')) >= 0)
        {
            if (++frames > 32 || end >= NativeGameInputState::MaxFrameBytes)
            {
                removeConnection(socket);
                return;
            }
            const QByteArray frame = it->buffer.left(end);
            it->buffer.remove(0, end + 1);
            const auto result = state.accept(frame, it->id, it->pid, uptimeMs(), &NativeGameInputMonitor::windowOwner);
            if (result == NativeGameInputState::Result::Invalid)
            {
                removeConnection(socket);
                return;
            }
            if (result == NativeGameInputState::Result::Accepted)
                it->initialized = true;
        }
        if (it->buffer.size() >= NativeGameInputState::MaxFrameBytes)
        {
            removeConnection(socket);
            return;
        }
    }
    recalculate();
}

void NativeGameInputMonitor::removeConnection(QLocalSocket *socket)
{
    auto it = connections.find(socket);
    if (it == connections.end())
        return;
    state.disconnect(it->id);
#ifdef Q_OS_WIN
    CloseHandle(it->process);
#endif
    connections.erase(it);
    socket->disconnect(this);
    socket->abort();
    socket->deleteLater();
    if (!recalculating)
        recalculate();
}

void NativeGameInputMonitor::recalculate()
{
    if (recalculating)
        return;
    QScopedValueRollback<bool> guard(recalculating, true);
    const quint64 now = uptimeMs();
#ifdef Q_OS_WIN
    for (QLocalSocket *socket : connections.keys())
    {
        const Connection &connection = connections[socket];
        if ((!connection.initialized && now >= connection.connected && now - connection.connected >= 500) ||
            !alive(connection.process))
        {
            removeConnection(socket);
        }
    }
    state.prune([](quint32 pid) {
        HANDLE process = OpenProcess(SYNCHRONIZE | PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
        const bool result = alive(process);
        if (process)
            CloseHandle(process);
        return result;
    });
#endif
    const bool suspended = state.suspended(now, foregroundWindow(), &NativeGameInputMonitor::windowOwner);
    if (suspended != m_suspended)
    {
        m_suspended = suspended;
        emit suspensionChanged(suspended);
    }
}

bool NativeGameInputMonitor::windowOwner(quintptr window, quint32 pid)
{
#ifdef Q_OS_WIN
    const HWND hwnd = reinterpret_cast<HWND>(window);
    DWORD owner = 0;
    return IsWindow(hwnd) && GetWindowThreadProcessId(hwnd, &owner) && owner == pid;
#else
    Q_UNUSED(window)
    Q_UNUSED(pid)
    return false;
#endif
}

quintptr NativeGameInputMonitor::foregroundWindow() const
{
#ifdef Q_OS_WIN
    return reinterpret_cast<quintptr>(GetForegroundWindow());
#else
    return 0;
#endif
}

quint64 NativeGameInputMonitor::uptimeMs() const
{
#ifdef Q_OS_WIN
    return GetTickCount64();
#else
    return 0;
#endif
}
