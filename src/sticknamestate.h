/* antimicrox Gamepad to KB+M event mapper
 * Copyright (C) 2026 BlueShapes contributors
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef STICKNAMESTATE_H
#define STICKNAMESTATE_H

#include <QMutex>
#include <QString>

/** Thread-safe custom/default names and the corresponding stick label renderer. */
class StickNameState final
{
  public:
    StickNameState() = default;

    QString customName() const;
    QString defaultName() const;

    // Returns true only when a valid custom name changes the stored value.
    bool setCustomName(const QString &name);
    void setDefaultName(const QString &name);
    void clearCustomName();

    // Takes a source snapshot before locking this state, so two sticks are never locked together.
    void copyCustomNameFrom(const StickNameState &source);

    QString partialName(const QString &stickLabel, int realJoyIndex, bool forceFullFormat, bool displayNames) const;

  private:
    static constexpr int MaxCustomNameLength = 20;

    mutable QMutex mutex;
    QString customNameValue;
    QString defaultNameValue;
};

#endif // STICKNAMESTATE_H
