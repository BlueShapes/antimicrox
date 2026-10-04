#include "nativegameinputmonitor.h"
#include "nativegameinputstate.h"
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QtTest>
#ifdef Q_OS_WIN
    #include <windows.h>
#endif

namespace {
const QString session = QStringLiteral("12345678-1234-1234-1234-123456789abc");
QByteArray frame(quint64 seq = 1, quint64 observed = 10000, bool ready = true, quint32 pid = 42,
                 quintptr hwnd = 100, const QString &uuid = session)
{
    return QJsonDocument(QJsonObject{{"protocol", 1}, {"product", "controllable-delta"}, {"type", "state"},
        {"session", uuid}, {"seq", static_cast<double>(seq)}, {"pid", static_cast<double>(pid)},
        {"hwnd", QStringLiteral("0x%1").arg(hwnd, 0, 16)},
        {"observed_uptime_ms", static_cast<double>(observed)}, {"controller_ready", ready}}).toJson(QJsonDocument::Compact);
}
const auto owner = [](quintptr hwnd, quint32 pid) { return (hwnd == 100 || hwnd == 200) && pid == 42; };

class TestMonitor : public NativeGameInputMonitor
{
  public:
    quintptr foreground = 0;
    quint64 now = 10000;
  protected:
    quintptr foregroundWindow() const override { return foreground; }
    quint64 uptimeMs() const override { return now; }
};
}

class NativeGameInputTests : public QObject
{
    Q_OBJECT
  private slots:
    void validation();
    void expiryAndForeground();
    void reconnectAndReplay();
    void boundsAndProcessExit();
    void windowsPipe();
};

void NativeGameInputTests::validation()
{
    NativeGameInputState state;
    using R = NativeGameInputState::Result;
    QCOMPARE(state.accept(frame(), 1, 42, 10000, owner), R::Accepted);
    const QList<QByteArray> invalid = {
        QByteArray("{}"), QByteArray("[]"), QByteArray("bad json"), QByteArray(4096, 'x'),
        frame().replace("\"protocol\":1", "\"protocol\":2"),
        frame().replace("controllable-delta", "controllable"),
        frame().replace("\"state\"", "\"execute\""),
        frame().replace("\"controller_ready\":true", "\"controller_ready\":1"),
        frame().replace("\"seq\":1", "\"seq\":1.5"),
        frame().replace("\"seq\":1", "\"seq\":-1"),
        frame().replace("\"seq\":1", "\"seq\":9007199254740992"),
        frame().replace("0x64", "0x0"), frame().replace("0x64", "0x10000000000000000"),
        frame().replace(session.toUtf8(), "invalid-session"), frame(2, 10000, true, 43), frame(2, 10000, true, 42, 999)
    };
    for (const QByteArray &bad : invalid)
        QCOMPARE(state.accept(bad, 2, 42, 10000, owner), R::Invalid);
    QCOMPARE(state.accept(frame(2, 10001), 1, 42, 10000, owner), R::Ignored);
    QCOMPARE(state.accept(frame(2, 9249), 1, 42, 10000, owner), R::Ignored);
    QCOMPARE(state.accept(frame(2, 9250), 1, 42, 10000, owner), R::Accepted);
}

void NativeGameInputTests::expiryAndForeground()
{
    NativeGameInputState state;
    QCOMPARE(state.accept(frame(), 1, 42, 10000, owner), NativeGameInputState::Result::Accepted);
    QVERIFY(state.suspended(11499, 100, owner)); // Freshness applies on receipt, lease on retention.
    QVERIFY(!state.suspended(11500, 100, owner));
    QVERIFY(!state.suspended(10000, 200, owner));
    QVERIFY(!state.suspended(9999, 100, owner));
    QVERIFY(!state.suspended(10000, 100, [](quintptr, quint32) { return false; }));
    QCOMPARE(state.accept(frame(2, 11000, false), 1, 42, 11000, owner), NativeGameInputState::Result::Accepted);
    QVERIFY(!state.suspended(11000, 100, owner));
    const QString second = QStringLiteral("22345678-1234-1234-1234-123456789abc");
    QCOMPARE(state.accept(frame(1, 11000, true, 42, 200, second), 2, 42, 11000, owner),
             NativeGameInputState::Result::Accepted);
    QVERIFY(!state.suspended(11000, 100, owner));
    QVERIFY(state.suspended(11000, 200, owner));
}

void NativeGameInputTests::reconnectAndReplay()
{
    NativeGameInputState state;
    using R = NativeGameInputState::Result;
    QCOMPARE(state.accept(frame(10), 1, 42, 10000, owner), R::Accepted);
    QCOMPARE(state.accept(frame(10, 11000), 1, 42, 11000, owner), R::Ignored);
    QVERIFY(!state.suspended(11500, 100, owner));
    QCOMPARE(state.accept(frame(11, 12000), 2, 42, 12000, owner), R::Accepted);
    state.disconnect(1); // Previous socket's delayed close cannot clear the replacement's lease.
    QVERIFY(state.suspended(12000, 100, owner));
    state.disconnect(2);
    QVERIFY(!state.suspended(12000, 100, owner));
    QCOMPARE(state.accept(frame(10, 12000), 3, 42, 12000, owner), R::Ignored);
    QCOMPARE(state.accept(frame(12, 12000), 3, 42, 12000, owner), R::Accepted);
    QCOMPARE(state.accept(frame(13, 12000, true, 42, 100, QStringLiteral("22345678-1234-1234-1234-123456789abc")),
                          3, 42, 12000, owner), R::Invalid);
}

void NativeGameInputTests::boundsAndProcessExit()
{
    NativeGameInputState state;
    for (int i = 0; i < NativeGameInputState::MaxSessions; ++i)
    {
        const QString uuid = QStringLiteral("%1-1234-1234-1234-123456789abc").arg(i, 8, 16, QLatin1Char('0'));
        QCOMPARE(state.accept(frame(1, 10000, true, 42, 100, uuid), i + 1, 42, 10000, owner),
                 NativeGameInputState::Result::Accepted);
    }
    QCOMPARE(state.accept(frame(), 100, 42, 10000, owner), NativeGameInputState::Result::Invalid);
    state.prune([](quint32) { return false; });
    QVERIFY(!state.suspended(10000, 100, owner));
    QCOMPARE(state.accept(frame(), 100, 42, 10000, owner), NativeGameInputState::Result::Accepted);
    state.clear();
    QVERIFY(!state.suspended(10000, 100, owner));
}

void NativeGameInputTests::windowsPipe()
{
#ifdef Q_OS_WIN
    const HWND hwnd = CreateWindowExW(0, L"STATIC", L"AntiMicroX IPC test", WS_OVERLAPPED, 0, 0, 1, 1,
                                      nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
    QVERIFY(hwnd != nullptr); // Hidden native window; desktop focus never changes.
    TestMonitor monitor;
    monitor.foreground = reinterpret_cast<quintptr>(hwnd);
    QSignalSpy suspensionSignals(&monitor, &NativeGameInputMonitor::suspensionChanged);
    monitor.setEnabled(true);
    QVERIFY(monitor.isListening());
    QLocalSocket socket;
    socket.connectToServer(monitor.serverName());
    QTRY_COMPARE_WITH_TIMEOUT(socket.state(), QLocalSocket::ConnectedState, 1000);
    const QByteArray message = frame(1, 10000, true, GetCurrentProcessId(), monitor.foreground) + '\n';
    socket.write(message.left(30));
    socket.flush();
    QTest::qWait(30);
    QVERIFY(!monitor.isSuspended());
    socket.write(message.mid(30));
    socket.flush();
    QTRY_VERIFY_WITH_TIMEOUT(monitor.isSuspended(), 1000); // Confirms Qt descriptor is a pipe HANDLE with OS peer PID.
    monitor.foreground = 0;
    QTRY_VERIFY_WITH_TIMEOUT(!monitor.isSuspended(), 1000);
    monitor.foreground = reinterpret_cast<quintptr>(hwnd);
    QTRY_VERIFY_WITH_TIMEOUT(monitor.isSuspended(), 1000);
    monitor.now = 11500;
    QTRY_VERIFY_WITH_TIMEOUT(!monitor.isSuspended(), 1000);
    socket.write(frame(2, 11500, false, GetCurrentProcessId(), monitor.foreground) + '\n' +
                 frame(3, 11500, true, GetCurrentProcessId(), monitor.foreground) + '\n');
    socket.flush();
    QTRY_VERIFY_WITH_TIMEOUT(monitor.isSuspended(), 1000);
    socket.disconnectFromServer();
    QTRY_VERIFY_WITH_TIMEOUT(!monitor.isSuspended(), 1000);
    QLocalSocket forged;
    forged.connectToServer(monitor.serverName());
    QTRY_COMPARE_WITH_TIMEOUT(forged.state(), QLocalSocket::ConnectedState, 1000);
    forged.write(frame(4, 11500, true, GetCurrentProcessId() + 1, monitor.foreground) + '\n');
    forged.flush();
    QTRY_COMPARE_WITH_TIMEOUT(forged.state(), QLocalSocket::UnconnectedState, 1000);
    QVERIFY(!monitor.isSuspended());
    QLocalSocket oversized;
    oversized.connectToServer(monitor.serverName());
    QTRY_COMPARE_WITH_TIMEOUT(oversized.state(), QLocalSocket::ConnectedState, 1000);
    oversized.write(QByteArray(4096, 'x')); // A never-terminated frame cannot grow the receive buffer.
    oversized.flush();
    QTRY_COMPARE_WITH_TIMEOUT(oversized.state(), QLocalSocket::UnconnectedState, 1000);
    QLocalSocket silent;
    silent.connectToServer(monitor.serverName());
    QTRY_COMPARE_WITH_TIMEOUT(silent.state(), QLocalSocket::ConnectedState, 1000);
    QTest::qWait(20);
    monitor.now += 500;
    QTRY_COMPARE_WITH_TIMEOUT(silent.state(), QLocalSocket::UnconnectedState, 1000);
    monitor.close();
    QVERIFY(!monitor.isListening());
    QVERIFY(!monitor.isSuspended());
    QVERIFY(suspensionSignals.count() >= 6);
    DestroyWindow(hwnd);
#else
    QSKIP("Windows named pipe integration");
#endif
}

QTEST_GUILESS_MAIN(NativeGameInputTests)
#include "testnativegameinput.moc"
