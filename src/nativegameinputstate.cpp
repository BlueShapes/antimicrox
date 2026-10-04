#include "nativegameinputstate.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <cmath>
#include <limits>

namespace {
bool integer(const QJsonValue &value, quint64 &result)
{
    if (!value.isDouble())
        return false;
    const double number = value.toDouble();
    if (!std::isfinite(number) || number < 0 || number > 9007199254740991.0 || std::floor(number) != number)
        return false;
    result = static_cast<quint64>(number);
    return true;
}
} // namespace

NativeGameInputState::Result NativeGameInputState::accept(const QByteArray &frame, quint64 connection, quint32 peerPid,
                                                          quint64 now,
                                                          const std::function<bool(quintptr, quint32)> &windowOwner)
{
    if (frame.isEmpty() || frame.size() >= MaxFrameBytes || connection == 0 || peerPid == 0)
        return Result::Invalid;
    QJsonParseError error;
    const QJsonDocument document = QJsonDocument::fromJson(frame, &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        return Result::Invalid;
    const QJsonObject object = document.object();
    quint64 protocol = 0, pid = 0, seq = 0, observed = 0;
    if (!integer(object.value("protocol"), protocol) || protocol != 1 ||
        object.value("product") != QJsonValue(QStringLiteral("controllable-delta")) ||
        object.value("type") != QJsonValue(QStringLiteral("state")) || !integer(object.value("pid"), pid) ||
        pid != peerPid || !integer(object.value("seq"), seq) || !integer(object.value("observed_uptime_ms"), observed) ||
        !object.value("controller_ready").isBool() || !object.value("session").isString() ||
        !object.value("hwnd").isString())
        return Result::Invalid;

    static const QRegularExpression uuid(
        QStringLiteral("^[0-9a-fA-F]{8}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{4}-[0-9a-fA-F]{12}$"));
    static const QRegularExpression handle(QStringLiteral("^0x[0-9a-fA-F]{1,16}$"));
    const QString session = object.value("session").toString().toLower();
    const QString hwndString = object.value("hwnd").toString();
    if (!uuid.match(session).hasMatch() || !handle.match(hwndString).hasMatch())
        return Result::Invalid;
    bool converted = false;
    const quint64 hwndValue = hwndString.mid(2).toULongLong(&converted, 16);
    if (!converted || hwndValue == 0 || hwndValue > std::numeric_limits<quintptr>::max() ||
        !windowOwner(static_cast<quintptr>(hwndValue), peerPid))
        return Result::Invalid;
    if (connectionSessions.contains(connection) && connectionSessions.value(connection) != session)
        return Result::Invalid;

    auto previous = sessions.constFind(session);
    if (previous != sessions.cend() && previous->pid != peerPid)
        return Result::Invalid;
    // A duplicate/replayed update never renews a lease, including after reconnect.
    if (observed > now || now - observed > MaxSnapshotAgeMs || (previous != sessions.cend() && seq <= previous->seq))
        return Result::Ignored;
    if (previous == sessions.cend() && sessions.size() >= MaxSessions)
        return Result::Invalid;

    sessions.insert(session, {connection, peerPid, static_cast<quintptr>(hwndValue), seq, now,
                              object.value("controller_ready").toBool()});
    connectionSessions.insert(connection, session);
    return Result::Accepted;
}

void NativeGameInputState::disconnect(quint64 connection)
{
    const QString session = connectionSessions.take(connection);
    auto entry = sessions.find(session);
    if (entry != sessions.end() && entry->connection == connection)
    {
        entry->connection = 0;
        entry->ready = false;
    }
}

void NativeGameInputState::clear()
{
    sessions.clear();
    connectionSessions.clear();
}

void NativeGameInputState::prune(const std::function<bool(quint32)> &processAlive)
{
    for (auto it = sessions.begin(); it != sessions.end();)
    {
        if (!processAlive(it->pid))
        {
            const QString session = it.key();
            for (auto connection = connectionSessions.begin(); connection != connectionSessions.end();)
                connection = connection.value() == session ? connectionSessions.erase(connection) : ++connection;
            it = sessions.erase(it);
        } else
            ++it;
    }
}

bool NativeGameInputState::suspended(quint64 now, quintptr foreground,
                                     const std::function<bool(quintptr, quint32)> &windowOwner) const
{
    if (foreground == 0)
        return false;
    for (const Session &session : sessions)
        if (session.connection != 0 && session.ready && session.hwnd == foreground && now >= session.received &&
            now - session.received < LeaseMs && windowOwner(session.hwnd, session.pid))
            return true;
    return false;
}
