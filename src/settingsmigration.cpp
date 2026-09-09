/* AntiMicroX-Delta settings migration
 * Copyright (C) 2026 BlueShapes contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "settingsmigration.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryFile>

#ifdef Q_OS_WIN
    #include <io.h>
#elif defined(Q_OS_UNIX)
    #include <unistd.h>
#endif

namespace
{
bool durableFlush(QTemporaryFile &file)
{
    if (!file.flush())
        return false;
#ifdef Q_OS_WIN
    return _commit(file.handle()) == 0;
#elif defined(Q_OS_UNIX)
    return fsync(file.handle()) == 0;
#else
    return true;
#endif
}
} // namespace

namespace SettingsMigration
{
Outcome copyFirstAvailable(const QStringList &sourcePaths, const QString &destinationPath)
{
    if (QFileInfo::exists(destinationPath))
        return {Result::DestinationExists, {}};

    for (const QString &sourcePath : sourcePaths)
    {
        const QFileInfo source(sourcePath);
        if (!source.exists() || !source.isFile() || source.isSymLink())
            continue;

        const QString canonicalSource = source.canonicalFilePath();
        if (canonicalSource.isEmpty())
            continue;

        const QString destinationDirectory = QFileInfo(destinationPath).absolutePath();
        if (!QDir().mkpath(destinationDirectory))
            return {Result::CopyFailed, canonicalSource};

        QFile input(canonicalSource);
        QTemporaryFile output(QDir(destinationDirectory).filePath(QStringLiteral(".antimicrox-delta-settings-XXXXXX.tmp")));
        output.setAutoRemove(true);
        if (!input.open(QIODevice::ReadOnly) || !output.open())
            return {Result::CopyFailed, canonicalSource};

        while (!input.atEnd())
        {
            const QByteArray chunk = input.read(64 * 1024);
            if (chunk.isEmpty() && input.error() != QFileDevice::NoError)
                return {Result::CopyFailed, canonicalSource};
            if (!chunk.isEmpty() && output.write(chunk) != chunk.size())
                return {Result::CopyFailed, canonicalSource};
        }
        if (!durableFlush(output))
            return {Result::CopyFailed, canonicalSource};
        output.close();

        if (QFileInfo::exists(destinationPath))
            return {Result::DestinationExists, {}};
        if (!output.rename(destinationPath))
            return QFileInfo::exists(destinationPath) ? Outcome{Result::DestinationExists, {}}
                                                      : Outcome{Result::CopyFailed, canonicalSource};
        output.setAutoRemove(false);
        return {Result::Copied, canonicalSource};
    }

    return {Result::NoSource, {}};
}
} // namespace SettingsMigration
