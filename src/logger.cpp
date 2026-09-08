/* antimicrox Gamepad to KB+M event mapper
 * Copyright (C) 2015 Travis Nickles <nickles.travis@gmail.com>
 * Copyright (C) 2020 Jagoda Górska <juliagoda.pl@protonmail>
 * Copyright (C) 2021 Paweł Kotiuk
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.

 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.

 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "logger.h"

#include "applicationsecuritypolicy.h"

#if defined(Q_OS_WIN)
    #include "windowscrashhandler.h"
#endif

#include <QDebug>
#include <QFileInfo>
#include <QSaveFile>
#include <QTime>

#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    #include <QStringConverter>
#endif

Logger *Logger::instance = nullptr;
QReadWriteLock Logger::instanceLock;

namespace
{
constexpr qint64 maximumLogBytes = 8LL * 1024LL * 1024LL;

bool isUtf8ContinuationByte(char value)
{
    return (static_cast<unsigned char>(value) & 0xc0U) == 0x80U;
}

bool retainRecentLogContents(const QString &filename)
{
    QFile source(filename);
    if (!source.exists() || source.size() <= maximumLogBytes || !source.open(QIODevice::ReadOnly))
        return true;

    if (!source.seek(source.size() - maximumLogBytes))
        return false;

    QByteArray recentContents = source.read(maximumLogBytes);
    if (recentContents.isEmpty() && source.size() != 0)
        return false;
    source.close();

    const qsizetype firstNewline = recentContents.indexOf('\n');
    if (firstNewline >= 0 && firstNewline + 1 < recentContents.size())
    {
        recentContents.remove(0, firstNewline + 1);
    }
    else
    {
        qsizetype firstCharacter = 0;
        while (firstCharacter < recentContents.size() && isUtf8ContinuationByte(recentContents.at(firstCharacter)))
            ++firstCharacter;
        recentContents.remove(0, firstCharacter);
    }

    QSaveFile replacement(filename);
    if (!replacement.open(QIODevice::WriteOnly))
        return false;
    if (replacement.write(recentContents) != recentContents.size())
        return false;
    return replacement.commit();
}

bool refersToSameFile(const QString &first, const QString &second)
{
    return QFileInfo(first).absoluteFilePath() == QFileInfo(second).absoluteFilePath();
}
} // namespace

Logger::Logger(QTextStream *stream, LogLevel output_lvl, QObject *parent)
    : QObject(parent)
    , outputFile(std::make_unique<QFile>())
    , outputStream(stream)
    , outputLevel(output_lvl)
{
}

Logger::~Logger()
{
    QWriteLocker lifecycleLocker(&instanceLock);
    if (instance == this)
        instance = nullptr;
    closeLogger();
}

void Logger::setLogLevel(LogLevel level)
{
    QReadLocker lifecycleLocker(&instanceLock);
    Logger *current = instance;
    Q_ASSERT(current != nullptr);
    if (current == nullptr)
        return;
    QMutexLocker locker(&current->logMutex);
    current->outputLevel = level;
}

Logger::LogLevel Logger::getCurrentLogLevel()
{
    QMutexLocker locker(&logMutex);
    return outputLevel;
}

Logger::LogLevel Logger::currentLogLevel()
{
    QReadLocker lifecycleLocker(&instanceLock);
    if (instance == nullptr)
        return LOG_NONE;
    return instance->getCurrentLogLevel();
}

void Logger::setCurrentStream(QTextStream *stream)
{
    QReadLocker lifecycleLocker(&instanceLock);
    Logger *current = instance;
    Q_ASSERT(current != nullptr);
    if (current == nullptr)
        return;
    QMutexLocker locker(&current->logMutex);
    if (current->outputStream != nullptr)
        current->outputStream->flush();
    current->outputStream = stream;
}

QTextStream *Logger::getCurrentStream()
{
    QReadLocker lifecycleLocker(&instanceLock);
    Logger *current = instance;
    Q_ASSERT(current != nullptr);
    if (current == nullptr)
        return nullptr;
    QMutexLocker locker(&current->logMutex);
    return current->outputStream;
}

void Logger::submitMessage(const QString &message, LogLevel level, uint lineno, const QString &filename)
{
    QReadLocker lifecycleLocker(&instanceLock);
    if (instance != nullptr)
        instance->logMessage(message, level, lineno, filename);
}

void Logger::closeLogger(bool closeStream)
{
    QMutexLocker locker(&logMutex);
    if (outputStream != nullptr)
        outputStream->flush();
    if (closeStream && outputFile != nullptr && outputFile->isOpen())
        outputFile->close();
}

void Logger::logMessage(const QString &message, const Logger::LogLevel level, const uint lineno, const QString &filename)
{
    const static QMap<Logger::LogLevel, QString> typeNames = {
        {LogLevel::LOG_DEBUG, "🐞DEBUG"},  {LogLevel::LOG_VERBOSE, "⚪VERBOSE"}, {LogLevel::LOG_INFO, "🟢INFO"},
        {LogLevel::LOG_WARNING, "❗WARN"}, {LogLevel::LOG_ERROR, "❌ERROR"},     {LogLevel::LOG_NONE, "NONE"}};

    QMutexLocker locker(&logMutex);
    if (outputStream == nullptr || outputLevel == LOG_NONE || level > outputLevel)
        return;

    const bool extendedLogs = outputLevel == LOG_DEBUG;
    if (extendedLogs)
        *outputStream << QString("[%1] ").arg(QTime::currentTime().toString("hh:mm:ss.zzz"));

    QString finalMessage = message;
    finalMessage.replace("\n", "\n\t\t\t");
    *outputStream << typeNames[level] << "\t" << finalMessage;

    if (extendedLogs)
    {
        static int filenameOffset = -1;
        if (filenameOffset < 0)
            filenameOffset = filename.lastIndexOf("/src/");
        if (lineno != 0)
            *outputStream << " (file " << filename.mid(filenameOffset) << ":" << lineno << ")";
    }

    *outputStream << "\n";
    outputStream->flush();

    if (outputFile != nullptr && outputStream == &outFileStream && outputFile->size() > maximumLogBytes)
    {
        outputFile->close();
        retainRecentLogContents(outputFile->fileName());
        const bool reopened = outputFile->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text);
        if (!reopened)
            outputStream = nullptr;
    }
}

void Logger::setCurrentLogFile(QString filename)
{
    if (filename.isEmpty())
        return;
    if (!ApplicationSecurityPolicy::current().allowsUserControlledLogFile())
    {
        qWarning() << "File logging is disabled while AntiMicroX is running elevated.";
        return;
    }

    bool openFailed = false;
    {
        QReadLocker lifecycleLocker(&instanceLock);
        Logger *current = instance;
        Q_ASSERT(current != nullptr);
        if (current == nullptr)
            return;

        QMutexLocker locker(&current->logMutex);
        if (current->outputFile != nullptr && current->outputFile->isOpen()
            && refersToSameFile(current->outputFile->fileName(), filename))
        {
            return;
        }

        const bool retentionSucceeded = retainRecentLogContents(filename);

        std::unique_ptr<QFile> replacement = std::make_unique<QFile>(filename);
        if (!retentionSucceeded || !replacement->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        {
            openFailed = true;
        }
        else
        {
            current->outFileStream.flush();
            if (current->outputFile != nullptr && current->outputFile->isOpen())
                current->outputFile->close();

            current->outputFile = std::move(replacement);
            current->outFileStream.setDevice(current->outputFile.get());
#if defined(Q_OS_WIN)
    #if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
            current->outFileStream.setEncoding(QStringConverter::Utf8);
    #else
            current->outFileStream.setCodec("UTF-8");
    #endif
#endif
            current->outputStream = &current->outFileStream;
        }
    }

    if (openFailed)
        qCritical() << "Couldn't open log file: " << filename;
}

bool Logger::isWritingToFile()
{
    QMutexLocker locker(&logMutex);
    return outputFile != nullptr && outputFile->isOpen();
}

bool Logger::isFileLoggingEnabled()
{
    QReadLocker lifecycleLocker(&instanceLock);
    return instance != nullptr && instance->isWritingToFile();
}

void Logger::loggerMessageHandler(QtMsgType type, const QMessageLogContext &context, const QString &msg)
{
    const Logger::LogLevel level = Logger::currentLogLevel();

    switch (type)
    {
    case QtDebugMsg:
        if (level >= Logger::LOG_DEBUG || level == Logger::LOG_MAX)
            LogHelper(LogLevel::LOG_DEBUG, context.line, context.file, msg);
        break;
    case QtInfoMsg:
        if (level >= Logger::LOG_INFO)
            LogHelper(LogLevel::LOG_INFO, context.line, context.file, msg);
        break;
    case QtWarningMsg:
        if (level >= Logger::LOG_WARNING)
            LogHelper(LogLevel::LOG_WARNING, context.line, context.file, msg);
        break;
    case QtCriticalMsg:
        if (level >= Logger::LOG_ERROR)
            LogHelper(LogLevel::LOG_ERROR, context.line, context.file, msg);
        break;
    case QtFatalMsg:
        if (level >= Logger::LOG_ERROR)
            LogHelper(LogLevel::LOG_ERROR, context.line, context.file, msg);
#if defined(Q_OS_WIN)
        WindowsCrashHandler::writeDump();
#endif
        abort();
    default:
        break;
    }
}

Logger *Logger::createInstance(QTextStream *stream, LogLevel outputLevel, QObject *parent)
{
    Logger *replacement = new Logger(stream, outputLevel, parent);
    Logger *previous = nullptr;
    {
        QWriteLocker lifecycleLocker(&instanceLock);
        previous = instance;
        instance = replacement;
    }
    delete previous;
    return replacement;
}

bool Logger::isDebugEnabled()
{
    QReadLocker lifecycleLocker(&instanceLock);
    if (instance == nullptr)
        return false;
    QMutexLocker locker(&instance->logMutex);
    return instance->outputLevel == LogLevel::LOG_DEBUG;
}

QString Logger::getCurrentLogFile()
{
    QReadLocker lifecycleLocker(&instanceLock);
    Logger *current = instance;
    Q_ASSERT(current != nullptr);
    if (current == nullptr)
        return "";
    QMutexLocker locker(&current->logMutex);
    if (current->outputFile != nullptr && current->outputFile->isOpen())
        return current->outputFile->fileName();
    return "";
}
