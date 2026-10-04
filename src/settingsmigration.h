/* AntiMicroX-Delta settings migration
 * Copyright (C) 2026 BlueShapes contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef SETTINGSMIGRATION_H
#define SETTINGSMIGRATION_H

#include <QString>
#include <QStringList>

namespace SettingsMigration {
enum class Result
{
    DestinationExists,
    NoSource,
    Copied,
    CopyFailed
};

struct Outcome
{
    Result result;
    QString sourcePath;
};

Outcome copyFirstAvailable(const QStringList &sourcePaths, const QString &destinationPath);
} // namespace SettingsMigration

#endif // SETTINGSMIGRATION_H
