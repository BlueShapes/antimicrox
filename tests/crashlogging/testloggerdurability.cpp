#include "applicationsecuritypolicy.h"
#include "logger.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QMetaObject>
#include <QProcess>
#include <QSemaphore>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>

#include <cstdlib>
#include <atomic>
#include <iostream>
#include <string>
#include <thread>

#ifdef Q_OS_WIN
    #include "windowscrashhandler.h"

    #include <windows.h>
    #include <dbghelp.h>
#endif

namespace
{
constexpr qint64 maximumLogBytes = 8LL * 1024LL * 1024LL;
int failures = 0;

void expect(bool condition, const char *message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

bool writeFile(const QString &path, const QByteArray &contents)
{
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly))
        return false;
    return file.write(contents) == contents.size() && file.flush();
}

QByteArray readFile(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return file.readAll();
}

void testOpeningLogPreservesExistingContents()
{
    QTemporaryDir directory;
    expect(directory.isValid(), "a temporary log directory can be created");
    const QString logPath = directory.filePath("antimicrox.log");
    const QByteArray earlierSession("EARLIER_SESSION\n");
    expect(writeFile(logPath, earlierSession), "an earlier log session can be prepared");

    QTextStream console(stdout);
    Logger *logger = Logger::createInstance(&console, Logger::LOG_INFO);
    Logger::setCurrentLogFile(logPath);

    expect(readFile(logPath).contains(earlierSession), "opening a log appends instead of erasing earlier sessions");
    delete logger;
}

void testOversizedLogKeepsRecentTail()
{
    QTemporaryDir directory;
    expect(directory.isValid(), "a temporary directory for log retention can be created");
    const QString logPath = directory.filePath("oversized.log");

    QByteArray oversized(maximumLogBytes + 4096, 'x');
    oversized.append("\nRECENT_TAIL_MARKER\n");
    expect(writeFile(logPath, oversized), "an oversized log can be prepared");

    QTextStream console(stdout);
    Logger *logger = Logger::createInstance(&console, Logger::LOG_INFO);
    Logger::setCurrentLogFile(logPath);

    const QByteArray retained = readFile(logPath);
    expect(retained.size() <= maximumLogBytes, "an oversized log is reduced to the retention limit");
    expect(retained.contains("RECENT_TAIL_MARKER"), "log retention keeps the most recent diagnostics");
    delete logger;
}

void testActiveLogRemainsBounded()
{
    QTemporaryDir directory;
    expect(directory.isValid(), "a temporary directory for active log retention can be created");
    const QString logPath = directory.filePath("active.log");

    QTextStream console(stdout);
    Logger *logger = Logger::createInstance(&console, Logger::LOG_INFO);
    Logger::setCurrentLogFile(logPath);

    const QString chunk(512 * 1024, QLatin1Char('x'));
    for (int index = 0; index < 18; ++index)
        INFO() << chunk;
    INFO() << "ACTIVE_LOG_RECENT_MARKER";

    const QByteArray retained = readFile(logPath);
    expect(retained.size() <= maximumLogBytes, "an active log remains within the retention limit");
    expect(retained.contains("ACTIVE_LOG_RECENT_MARKER"), "active log retention keeps the latest complete message");
    delete logger;
}

[[noreturn]] void runAbruptExitChild(const QString &logPath)
{
    QTextStream console(stdout);
    Logger *logger = Logger::createInstance(&console, Logger::LOG_INFO);
    Logger::setCurrentLogFile(logPath);

    // With the old asynchronous logger this occupies its worker before the
    // marker is queued. A synchronous logger persists the marker immediately.
    if (logger->thread() != QThread::currentThread())
    {
        QSemaphore workerEntered;
        QMetaObject::invokeMethod(
            logger,
            [&workerEntered] {
                workerEntered.release();
                QThread::msleep(5000);
            },
            Qt::QueuedConnection);
        if (!workerEntered.tryAcquire(1, 2000))
            std::_Exit(88);
    }
    INFO() << "LAST_MESSAGE_BEFORE_ABRUPT_EXIT";

#ifdef Q_OS_WIN
    TerminateProcess(GetCurrentProcess(), 86);
#else
    std::_Exit(86);
#endif
    std::abort();
}

#ifdef Q_OS_WIN
[[noreturn]] void runMinidumpChild(const QString &dumpDirectory)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    const std::wstring nativeDirectory = dumpDirectory.toStdWString();
    if (!WindowsCrashHandler::install(nativeDirectory.c_str()))
        std::_Exit(87);

    *static_cast<volatile int *>(nullptr) = 1;
    std::abort();
}

[[noreturn]] void runFatalDumpChild(const QString &dumpDirectory)
{
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    const std::wstring nativeDirectory = dumpDirectory.toStdWString();
    if (!WindowsCrashHandler::install(nativeDirectory.c_str()))
        std::_Exit(87);

    QTextStream console(stdout);
    Logger::createInstance(&console, Logger::LOG_DEBUG);
    qInstallMessageHandler(Logger::loggerMessageHandler);
    qFatal("FATAL_DUMP_TEST");
    std::abort();
}

void testUnhandledWindowsExceptionCreatesMinidump()
{
    QTemporaryDir directory;
    expect(directory.isValid(), "a temporary minidump directory can be created");

    QProcess child;
    child.start(QCoreApplication::applicationFilePath(), {QStringLiteral("--minidump-child"), directory.path()});
    expect(child.waitForStarted(5000), "the minidump crash probe starts");
    expect(child.waitForFinished(15000), "the minidump crash probe terminates without a fixed sleep");
    expect(child.exitCode() != 0 || child.exitStatus() == QProcess::CrashExit, "the minidump probe ends abnormally");

    QDir dumpDir(directory.path());
    const QStringList dumps = dumpDir.entryList({QStringLiteral("*.dmp")}, QDir::Files);
    expect(dumps.size() == 1, "one minidump is created for an unhandled exception");
    if (dumps.size() == 1)
    {
        QFile dump(dumpDir.filePath(dumps.constFirst()));
        expect(dump.open(QIODevice::ReadOnly), "the generated minidump can be read");
        expect(dump.size() > 4, "the generated minidump is non-empty");
        const QByteArray contents = dump.readAll();
        expect(contents.startsWith(QByteArrayLiteral("MDMP")), "the generated file has the minidump signature");

        PMINIDUMP_DIRECTORY streamDirectory = nullptr;
        void *stream = nullptr;
        ULONG streamSize = 0;
        const BOOL found = MiniDumpReadDumpStream(const_cast<char *>(contents.constData()), ExceptionStream,
                                                  &streamDirectory, &stream, &streamSize);
        expect(found != FALSE && stream != nullptr && streamSize >= sizeof(MINIDUMP_EXCEPTION_STREAM),
               "the minidump contains an exception stream");
        if (found != FALSE && stream != nullptr && streamSize >= sizeof(MINIDUMP_EXCEPTION_STREAM))
        {
            const auto *exceptionStream = static_cast<const MINIDUMP_EXCEPTION_STREAM *>(stream);
            expect(exceptionStream->ExceptionRecord.ExceptionCode == EXCEPTION_ACCESS_VIOLATION,
                   "the minidump records the access-violation exception code");
        }
    }
}

void testFatalMessageCreatesMinidump()
{
    QTemporaryDir directory;
    expect(directory.isValid(), "a temporary fatal-dump directory can be created");

    QProcess child;
    child.start(QCoreApplication::applicationFilePath(), {QStringLiteral("--fatal-dump-child"), directory.path()});
    expect(child.waitForStarted(5000), "the fatal-dump probe starts");
    expect(child.waitForFinished(15000), "the fatal-dump probe terminates without a fixed sleep");
    expect(child.exitCode() != 0 || child.exitStatus() == QProcess::CrashExit, "the fatal-dump probe ends abnormally");

    QDir dumpDir(directory.path());
    expect(dumpDir.entryList({QStringLiteral("*.dmp")}, QDir::Files).size() == 1,
           "one minidump is created for a Qt fatal message");
}

void testCrashDumpRetentionIsBounded()
{
    QTemporaryDir directory;
    expect(directory.isValid(), "a temporary crash retention directory can be created");
    for (int index = 0; index < 7; ++index)
    {
        const QString name = QStringLiteral("antimicrox-crash-retention-%1.dmp").arg(index);
        expect(writeFile(directory.filePath(name), QByteArrayLiteral("old dump")), "an old crash dump can be prepared");
    }
    expect(writeFile(directory.filePath("unrelated.dmp"), QByteArrayLiteral("unrelated")),
           "an unrelated dump can be prepared");

    const std::wstring nativeDirectory = directory.path().toStdWString();
    expect(WindowsCrashHandler::install(nativeDirectory.c_str()), "crash handling can be installed for retention");

    QDir dumpDir(directory.path());
    expect(dumpDir.entryList({QStringLiteral("antimicrox-crash-*.dmp")}, QDir::Files).size() <= 4,
           "startup retains room for one new dump within the five-dump limit");
    expect(QFile::exists(directory.filePath("unrelated.dmp")), "crash retention does not delete unrelated dumps");
}
#endif

void testAbruptExitDoesNotLoseLastCompletedMessage()
{
    QTemporaryDir directory;
    expect(directory.isValid(), "a temporary directory for abrupt-exit logging can be created");
    const QString logPath = directory.filePath("abrupt.log");

    QProcess child;
    child.start(QCoreApplication::applicationFilePath(), {QStringLiteral("--abrupt-exit-child"), logPath});
    expect(child.waitForStarted(5000), "the abrupt-exit probe starts");
    expect(child.waitForFinished(10000), "the abrupt-exit probe terminates without a fixed sleep");
    expect(child.exitCode() != 0 || child.exitStatus() == QProcess::CrashExit,
           "the abrupt-exit probe ends abnormally");
    expect(readFile(logPath).contains("LAST_MESSAGE_BEFORE_ABRUPT_EXIT"),
           "a completed log call survives immediate process termination");
}

void testLoggingDuringShutdownIsSafelyDropped()
{
    QTextStream console(stdout);
    Logger *logger = Logger::createInstance(&console, Logger::LOG_INFO);
    QSemaphore readyToDelete;
    QSemaphore loggerDeleted;
    std::atomic_bool loggingThrew = false;

    std::thread producer([&] {
        INFO() << "BEFORE_LOGGER_SHUTDOWN";
        readyToDelete.release();
        loggerDeleted.acquire();
        try
        {
            INFO() << "AFTER_LOGGER_SHUTDOWN";
        }
        catch (...)
        {
            loggingThrew = true;
        }
    });

    readyToDelete.acquire();
    delete logger;
    loggerDeleted.release();
    producer.join();
    expect(!loggingThrew.load(), "logging started during shutdown is safely dropped");
}
} // namespace

const ApplicationSecurityPolicy &ApplicationSecurityPolicy::current() noexcept
{
    static const ApplicationSecurityPolicy policy(ElevationState::NotElevated, false);
    return policy;
}

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    if (application.arguments().size() == 3 && application.arguments().at(1) == QStringLiteral("--abrupt-exit-child"))
        runAbruptExitChild(application.arguments().at(2));
#ifdef Q_OS_WIN
    if (application.arguments().size() == 3 && application.arguments().at(1) == QStringLiteral("--minidump-child"))
        runMinidumpChild(application.arguments().at(2));
    if (application.arguments().size() == 3 && application.arguments().at(1) == QStringLiteral("--fatal-dump-child"))
        runFatalDumpChild(application.arguments().at(2));
#endif

    testOpeningLogPreservesExistingContents();
    testOversizedLogKeepsRecentTail();
    testActiveLogRemainsBounded();
    testAbruptExitDoesNotLoseLastCompletedMessage();
    testLoggingDuringShutdownIsSafelyDropped();
#ifdef Q_OS_WIN
    testUnhandledWindowsExceptionCreatesMinidump();
    testFatalMessageCreatesMinidump();
    testCrashDumpRetentionIsBounded();
#endif

    if (failures != 0)
    {
        std::cerr << failures << " crash logging test(s) failed\n";
        return 1;
    }

    std::cout << "All crash logging tests passed\n";
    return 0;
}
