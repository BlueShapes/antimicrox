/* antimicrox Gamepad to KB+M event mapper
 * Copyright (C) 2026
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "inputreleaselatch.h"

#include <QtGlobal>

int InputReleaseLatch::calibrateAxisValue(int rawValue, double gain, double offset)
{
    return static_cast<int>(rawValue * gain + offset);
}

void InputReleaseLatch::reset(const QVector<bool> &buttons, const QVector<int> &hats, const QVector<int> &axes,
                              const QVector<bool> &axesInDeadZone, const QVector<StickPair> &stickPairs)
{
    m_latchedButtons = buttons;
    m_latchedHats.resize(hats.size());
    for (int i = 0; i < hats.size(); ++i)
        m_latchedHats[i] = hats.at(i) != 0;

    m_axisValues = axes;
    m_latchedAxes.resize(axes.size());

    m_stickPairs.clear();
    m_latchedStickPairs.clear();
    for (const StickPair &pair : stickPairs)
    {
        if (pair.xAxis < 0 || pair.yAxis < 0 || pair.xAxis >= m_axisValues.size() || pair.yAxis >= m_axisValues.size())
            continue;

        m_stickPairs.append(pair);
        m_latchedStickPairs.append(!stickPairIsNeutral(pair));
    }

    QVector<bool> axesInStick(axes.size(), false);
    for (const StickPair &pair : m_stickPairs)
    {
        axesInStick[pair.xAxis] = true;
        axesInStick[pair.yAxis] = true;
    }

    for (int i = 0; i < axes.size(); ++i)
    {
        // A paired stick is neutral by its radial deadzone. Applying the
        // individual axis deadzone as well can leave a neutral diagonal
        // latched even though the whole stick is already inside its deadzone.
        m_latchedAxes[i] = !axesInStick.at(i) && (i < axesInDeadZone.size()) && !axesInDeadZone.at(i);
    }
}

bool InputReleaseLatch::allowButtonEvent(int index, bool pressed)
{
    if (index < 0 || index >= m_latchedButtons.size() || !m_latchedButtons.at(index))
        return true;

    if (!pressed)
        m_latchedButtons[index] = false;
    return false;
}

bool InputReleaseLatch::allowHatEvent(int index, int direction)
{
    if (index < 0 || index >= m_latchedHats.size() || !m_latchedHats.at(index))
        return true;

    if (direction == 0)
        m_latchedHats[index] = false;
    return false;
}

bool InputReleaseLatch::allowAxisEvent(int index, int value, bool inDeadZone)
{
    if (index < 0 || index >= m_axisValues.size())
        return true;

    const bool wasLatched = isAxisLatched(index);
    m_axisValues[index] = value;

    if (m_latchedAxes.at(index) && inDeadZone)
        m_latchedAxes[index] = false;

    for (int i = 0; i < m_stickPairs.size(); ++i)
    {
        if (m_latchedStickPairs.at(i) && stickPairIsNeutral(m_stickPairs.at(i)))
            m_latchedStickPairs[i] = false;
    }

    return !wasLatched;
}

bool InputReleaseLatch::isButtonLatched(int index) const
{
    return index >= 0 && index < m_latchedButtons.size() && m_latchedButtons.at(index);
}

bool InputReleaseLatch::isHatLatched(int index) const
{
    return index >= 0 && index < m_latchedHats.size() && m_latchedHats.at(index);
}

bool InputReleaseLatch::isAxisLatched(int index) const
{
    if (index < 0 || index >= m_latchedAxes.size())
        return false;

    if (m_latchedAxes.at(index))
        return true;

    for (int i = 0; i < m_stickPairs.size(); ++i)
    {
        const StickPair &pair = m_stickPairs.at(i);
        if (m_latchedStickPairs.at(i) && (pair.xAxis == index || pair.yAxis == index))
            return true;
    }

    return false;
}

bool InputReleaseLatch::isStickLatched(int xAxis, int yAxis) const
{
    for (int i = 0; i < m_stickPairs.size(); ++i)
    {
        const StickPair &pair = m_stickPairs.at(i);
        if (m_latchedStickPairs.at(i) && pair.xAxis == xAxis && pair.yAxis == yAxis)
            return true;
    }

    return false;
}

void InputReleaseLatch::clear()
{
    m_latchedButtons.clear();
    m_latchedHats.clear();
    m_latchedAxes.clear();
    m_axisValues.clear();
    m_latchedStickPairs.clear();
    m_stickPairs.clear();
}

bool InputReleaseLatch::stickPairIsNeutral(const StickPair &pair) const
{
    const qint64 x = m_axisValues.at(pair.xAxis);
    const qint64 y = m_axisValues.at(pair.yAxis);
    const qint64 deadZone = qMax(0, pair.deadZone);
    return (x * x + y * y) <= (deadZone * deadZone);
}
