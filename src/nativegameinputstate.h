#ifndef NATIVEGAMEINPUTSTATE_H
#define NATIVEGAMEINPUTSTATE_H

#include <QByteArray>
#include <QHash>
#include <QString>
#include <functional>

// Protocol policy is independent of IPC and the wall clock so expiry and reconnects
// can be verified without a game, controller, or foreground window changes.
class NativeGameInputState
{
  public:
    enum class Result
    {
        Accepted,
        Ignored,
        Invalid
    };
    static constexpr int MaxFrameBytes = 4096;
    static constexpr int MaxSessions = 64;
    static constexpr quint64 MaxSnapshotAgeMs = 750;
    static constexpr quint64 LeaseMs = 1500;

    Result accept(const QByteArray &frame, quint64 connection, quint32 peerPid, quint64 now,
                  const std::function<bool(quintptr, quint32)> &windowOwner);
    void disconnect(quint64 connection);
    void clear();
    void prune(const std::function<bool(quint32)> &processAlive);
    bool suspended(quint64 now, quintptr foreground, const std::function<bool(quintptr, quint32)> &windowOwner) const;

  private:
    struct Session
    {
        quint64 connection = 0;
        quint32 pid = 0;
        quintptr hwnd = 0;
        quint64 seq = 0;
        quint64 received = 0;
        bool ready = false;
    };
    QHash<QString, Session> sessions;
    QHash<quint64, QString> connectionSessions;
};

#endif
