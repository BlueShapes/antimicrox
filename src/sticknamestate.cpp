/* antimicrox Gamepad to KB+M event mapper
 * Copyright (C) 2026 BlueShapes contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "sticknamestate.h"

#include <QMutexLocker>

QString StickNameState::customName() const
{
    QMutexLocker locker(&mutex);
    return customNameValue;
}

QString StickNameState::defaultName() const
{
    QMutexLocker locker(&mutex);
    return defaultNameValue;
}

bool StickNameState::setCustomName(const QString &name)
{
    if (name.length() > MaxCustomNameLength)
        return false;

    QMutexLocker locker(&mutex);
    if (name == customNameValue)
        return false;

    customNameValue = name;
    return true;
}

void StickNameState::setDefaultName(const QString &name)
{
    QMutexLocker locker(&mutex);
    defaultNameValue = name;
}

void StickNameState::clearCustomName()
{
    QMutexLocker locker(&mutex);
    customNameValue.clear();
}

void StickNameState::copyCustomNameFrom(const StickNameState &source)
{
    const QString snapshot = source.customName();
    QMutexLocker locker(&mutex);
    customNameValue = snapshot;
}

QString StickNameState::partialName(const QString &stickLabel, int realJoyIndex, bool forceFullFormat,
                                    bool displayNames) const
{
    QString custom;
    QString defaultName;
    {
        QMutexLocker locker(&mutex);
        custom = customNameValue;
        defaultName = defaultNameValue;
    }

    QString label;
    if (!custom.isEmpty() && displayNames)
    {
        if (forceFullFormat)
            label.append(stickLabel).append(QLatin1Char(' '));

        label.append(custom);
    } else if (!defaultName.isEmpty())
    {
        if (forceFullFormat)
            label.append(stickLabel).append(QLatin1Char(' '));

        label.append(defaultName);
    } else
    {
        label.append(stickLabel).append(QLatin1Char(' '));
        label.append(QString::number(realJoyIndex));
    }

    return label;
}
