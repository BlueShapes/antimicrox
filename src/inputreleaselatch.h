/* antimicrox Gamepad to KB+M event mapper
 * Copyright (C) 2026
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#ifndef INPUTRELEASELATCH_H
#define INPUTRELEASELATCH_H

#include <QVector>

/**
 * @brief Suppresses mapped controls that were already active when input was
 *        resumed, until each control has returned to its neutral state.
 *
 * This class is deliberately independent of SDL so its edge handling can be
 * tested without a physical controller.
 */
class InputReleaseLatch
{
  public:
    struct StickPair
    {
        int xAxis = -1;
        int yAxis = -1;
        int deadZone = 0;
    };

    void reset(const QVector<bool> &buttons, const QVector<int> &hats, const QVector<int> &axes,
               const QVector<bool> &axesInDeadZone, const QVector<StickPair> &stickPairs);

    static int calibrateAxisValue(int rawValue, double gain, double offset);

    bool allowButtonEvent(int index, bool pressed);
    bool allowHatEvent(int index, int direction);
    bool allowAxisEvent(int index, int value, bool inDeadZone);

    bool isButtonLatched(int index) const;
    bool isHatLatched(int index) const;
    bool isAxisLatched(int index) const;
    bool isStickLatched(int xAxis, int yAxis) const;

    void clear();

  private:
    bool stickPairIsNeutral(const StickPair &pair) const;

    QVector<bool> m_latchedButtons;
    QVector<bool> m_latchedHats;
    QVector<bool> m_latchedAxes;
    QVector<int> m_axisValues;
    QVector<bool> m_latchedStickPairs;
    QVector<StickPair> m_stickPairs;
};

#endif // INPUTRELEASELATCH_H
