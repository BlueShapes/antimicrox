/* antimicrox Gamepad to KB+M event mapper
 * Copyright (C) 2026
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 */

#include "inputreleaselatch.h"

#include <QtTest>

class InputReleaseLatchTests : public QObject
{
    Q_OBJECT

  private slots:
    void buttonRemainsLatchedUntilRelease();
    void buttonPressedAfterResumeSnapshotIsAllowed();
    void hatRemainsLatchedUntilCentered();
    void axisRemainsLatchedUntilThrottleAwareNeutral();
    void stickUsesRadialNeutralAndWideArithmetic();
    void calibratedAxisValuesAreUsedForNeutralChecks();
};

void InputReleaseLatchTests::buttonRemainsLatchedUntilRelease()
{
    InputReleaseLatch latch;
    latch.reset({true}, {}, {}, {}, {});

    QVERIFY(latch.isButtonLatched(0));
    QVERIFY(!latch.allowButtonEvent(0, true));
    QVERIFY(!latch.allowButtonEvent(0, false));
    QVERIFY(!latch.isButtonLatched(0));
    QVERIFY(latch.allowButtonEvent(0, true));
}

void InputReleaseLatchTests::buttonPressedAfterResumeSnapshotIsAllowed()
{
    InputReleaseLatch latch;
    latch.reset({false}, {}, {}, {}, {});

    QVERIFY(!latch.isButtonLatched(0));
    QVERIFY(latch.allowButtonEvent(0, true));
}

void InputReleaseLatchTests::hatRemainsLatchedUntilCentered()
{
    InputReleaseLatch latch;
    constexpr int hatRightUp = 3;
    constexpr int hatCentered = 0;
    constexpr int hatLeft = 8;
    latch.reset({}, {hatRightUp}, {}, {}, {});

    QVERIFY(latch.isHatLatched(0));
    QVERIFY(!latch.allowHatEvent(0, hatRightUp));
    QVERIFY(!latch.allowHatEvent(0, hatCentered));
    QVERIFY(!latch.isHatLatched(0));
    QVERIFY(latch.allowHatEvent(0, hatLeft));
}

void InputReleaseLatchTests::axisRemainsLatchedUntilThrottleAwareNeutral()
{
    InputReleaseLatch latch;
    // The caller derives this neutral from JoyAxis's configured throttle. For
    // a positive trigger, SDL's -32768 endpoint can be the released value.
    latch.reset({}, {}, {0}, {false}, {});

    QVERIFY(latch.isAxisLatched(0));
    QVERIFY(!latch.allowAxisEvent(0, -20000, false));
    QVERIFY(!latch.allowAxisEvent(0, -32768, true));
    QVERIFY(!latch.isAxisLatched(0));
    QVERIFY(latch.allowAxisEvent(0, -10000, false));
}

void InputReleaseLatchTests::stickUsesRadialNeutralAndWideArithmetic()
{
    InputReleaseLatch latch;
    // Individually neutral axes can still form a held radial stick vector.
    latch.reset({}, {}, {25000, 25000}, {true, true}, {{0, 1, 32000}});
    QVERIFY(latch.isAxisLatched(0));
    QVERIFY(latch.isAxisLatched(1));
    QVERIFY(!latch.allowAxisEvent(0, 0, true));
    QVERIFY(!latch.isAxisLatched(0));
    QVERIFY(latch.allowAxisEvent(1, 25000, true));
    QVERIFY(latch.allowAxisEvent(0, 10000, false));

    // A tighter radial deadzone must remain latched while the other axis is
    // still outside it, even after the first axis returns to zero.
    latch.reset({}, {}, {25000, 25000}, {true, true}, {{0, 1, 20000}});
    QVERIFY(latch.isStickLatched(0, 1));
    QVERIFY(!latch.allowAxisEvent(0, 0, true));
    QVERIFY(latch.isAxisLatched(1));
    QVERIFY(!latch.allowAxisEvent(1, 25000, false));
    QVERIFY(!latch.allowAxisEvent(1, 0, true));
    QVERIFY(!latch.isStickLatched(0, 1));
    QVERIFY(latch.allowAxisEvent(0, 5000, false));

    // Individual axis deadzones must not over-latch a radially neutral stick.
    latch.reset({}, {}, {10000, 10000}, {false, false}, {{0, 1, 15000}});
    QVERIFY(!latch.isStickLatched(0, 1));
    QVERIFY(!latch.isAxisLatched(0));
    QVERIFY(latch.allowAxisEvent(0, 12000, false));

    // This vector exceeds the deadzone; 32-bit square-and-add would overflow
    // and incorrectly classify it as neutral.
    latch.reset({}, {}, {32767, 32767}, {true, true}, {{0, 1, 32000}});
    QVERIFY(latch.isStickLatched(0, 1));
}

void InputReleaseLatchTests::calibratedAxisValuesAreUsedForNeutralChecks()
{
    const int positiveTriggerNeutral = InputReleaseLatch::calibrateAxisValue(1000, 1.0, -33768.0);
    QCOMPARE(positiveTriggerNeutral, -32768);

    InputReleaseLatch latch;
    latch.reset({}, {}, {positiveTriggerNeutral}, {true}, {});
    QVERIFY(!latch.isAxisLatched(0));

    const int calibratedNeutral = InputReleaseLatch::calibrateAxisValue(20000, 0.5, -10000.0);
    QCOMPARE(calibratedNeutral, 0);
    latch.reset({}, {}, {calibratedNeutral}, {true}, {});
    QVERIFY(!latch.isAxisLatched(0));
    const int calibratedMovement = InputReleaseLatch::calibrateAxisValue(20200, 0.5, -10000.0);
    QCOMPARE(calibratedMovement, 100);
    QVERIFY(latch.allowAxisEvent(0, calibratedMovement, false));
}

QTEST_APPLESS_MAIN(InputReleaseLatchTests)
#include "testinputreleaselatch.moc"
