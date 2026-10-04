#include "antimicrosettings.h"
#include "gamecontroller/gamecontroller.h"
#include "inputdaemon.h"
#include "joybuttontypes/joybutton.h"
#include "joybuttontypes/joycontrolstickbutton.h"
#include "joycontrolstick.h"

#include <SDL2/SDL.h>

#include <QApplication>
#include <QElapsedTimer>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

#include <memory>

#if !SDL_VERSION_ATLEAST(2, 0, 14)
    #error InputSuspensionTests requires SDL virtual joystick support (SDL 2.0.14 or later).
#endif

namespace
{
class PipelineDaemon final : public InputDaemon
{
  public:
    PipelineDaemon(QMap<SDL_JoystickID, InputDevice *> *devices, AntiMicroSettings *settings)
        : InputDaemon(devices, settings, false)
    {
    }

    bool drain()
    {
        QElapsedTimer elapsed;
        elapsed.start();
        QQueue<SDL_Event> events;
        firstInputPass(&events);
        modifyUnplugEvents(&events);
        secondInputPass(&events);
        clearBitArrayStatusInstances();
        return events.isEmpty() && elapsed.elapsed() < 1000;
    }
};
} // namespace

// Every control has empty output slots unless a test assigns JoySetChange.
// Neither keyboard/mouse output nor the SDL worker thread is started here.
class InputSuspensionTests final : public QObject
{
    Q_OBJECT

  private slots:
    void init();
    void cleanup();
    void pausedControllerEventsDrainAndPreserveRawMonitor();
    void manualDisableSurvivesResume();
    void heldButtonRequiresRelease_data();
    void heldButtonRequiresRelease();
    void queuedPauseEventsCannotReplayAfterResume();
    void suspensionCancelsReleaseTimeSetChange();
    void calibratedProfileReloadStaysInactive_data();
    void calibratedProfileReloadStaysInactive();
    void radialStickNeutralDoesNotLatchChildAxes();
    void suspensionClearsSpringQueue();

  private:
    bool setPhysicalButton(bool pressed);
    bool setPhysicalAxis(int axis, Sint16 value);
    SDL_Event buttonEvent(bool pressed) const;
    SDL_Event axisEvent(int axis, Sint16 value) const;
    bool pushAndDrain(const SDL_Event &event);
    bool sendButton(bool pressed);
    bool sendAxis(int axis, Sint16 value);

    QTemporaryDir directory;
    QMap<SDL_JoystickID, InputDevice *> devices;
    std::unique_ptr<AntiMicroSettings> settings;
    std::unique_ptr<PipelineDaemon> daemon;
    SDL_Joystick *virtualJoystick = nullptr;
    int virtualIndex = -1;
    SDL_JoystickID instanceId = -1;
    GameController *controller = nullptr;
};

void InputSuspensionTests::init()
{
    QVERIFY(directory.isValid());
    settings = std::make_unique<AntiMicroSettings>(directory.filePath("suspension.ini"), QSettings::IniFormat);
    settings->clear();
    SDL_SetHint(SDL_HINT_JOYSTICK_ALLOW_BACKGROUND_EVENTS, "1");
    QCOMPARE(SDL_Init(SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK), 0);
    virtualIndex = SDL_JoystickAttachVirtual(SDL_JOYSTICK_TYPE_GAMECONTROLLER, SDL_CONTROLLER_AXIS_MAX,
                                            SDL_CONTROLLER_BUTTON_MAX, 1);
    QVERIFY2(virtualIndex >= 0, SDL_GetError());
    virtualJoystick = SDL_JoystickOpen(virtualIndex);
    QVERIFY2(virtualJoystick != nullptr, SDL_GetError());
    instanceId = SDL_JoystickInstanceID(virtualJoystick);

    // Explicit bindings make the virtual raw controls deterministic across SDL releases.
    char guid[33] = {};
    SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(virtualJoystick), guid, sizeof(guid));
    const QByteArray mapping = QByteArray(guid) +
        ",Suspension test controller,a:b0,b:b1,x:b2,y:b3,back:b4,guide:b5,start:b6,"
        "leftstick:b7,rightstick:b8,leftshoulder:b9,rightshoulder:b10,"
        "dpup:b11,dpdown:b12,dpleft:b13,dpright:b14,"
        "leftx:a0,lefty:a1,rightx:a2,righty:a3,lefttrigger:a4,righttrigger:a5,";
    QVERIFY2(SDL_GameControllerAddMapping(mapping.constData()) >= 0, SDL_GetError());
    QVERIFY(SDL_IsGameController(virtualIndex));
    QCOMPARE(SDL_JoystickSetVirtualAxis(virtualJoystick, SDL_CONTROLLER_AXIS_TRIGGERLEFT, -32768), 0);
    QCOMPARE(SDL_JoystickSetVirtualAxis(virtualJoystick, SDL_CONTROLLER_AXIS_TRIGGERRIGHT, -32768), 0);
    SDL_JoystickUpdate();

    daemon = std::make_unique<PipelineDaemon>(&devices, settings.get());
    controller = qobject_cast<GameController *>(devices.value(instanceId));
    QVERIFY(controller != nullptr);
    QVERIFY(controller->getActiveSetJoystick()->getJoyButton(SDL_CONTROLLER_BUTTON_A) != nullptr);
    QVERIFY(controller->getActiveSetJoystick()->getJoyStick(0) != nullptr);
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
}

void InputSuspensionTests::cleanup()
{
    JoyButton::getStaticMouseEventTimer()->stop();
    JoyButton::clearSuspendedMouseState();
    if (virtualJoystick != nullptr)
    {
        SDL_JoystickClose(virtualJoystick);
        virtualJoystick = nullptr;
    }
    if (virtualIndex >= 0 && SDL_WasInit(SDL_INIT_JOYSTICK))
        SDL_JoystickDetachVirtual(virtualIndex);
    daemon.reset(); // Its reader closes the SDL handles and SDL itself.
    controller = nullptr;
    devices.clear();
    settings.reset();
    SDL_Quit();
    virtualIndex = -1;
    instanceId = -1;
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

bool InputSuspensionTests::setPhysicalButton(bool pressed)
{
    if (SDL_JoystickSetVirtualButton(virtualJoystick, SDL_CONTROLLER_BUTTON_A, pressed ? 1 : 0) != 0)
        return false;
    SDL_JoystickUpdate();
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
    return true;
}

bool InputSuspensionTests::setPhysicalAxis(int axis, Sint16 value)
{
    if (SDL_JoystickSetVirtualAxis(virtualJoystick, axis, value) != 0)
        return false;
    SDL_JoystickUpdate();
    SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
    return true;
}

SDL_Event InputSuspensionTests::buttonEvent(bool pressed) const
{
    SDL_Event event = {};
    event.type = pressed ? SDL_CONTROLLERBUTTONDOWN : SDL_CONTROLLERBUTTONUP;
    event.cbutton.timestamp = SDL_GetTicks();
    event.cbutton.which = instanceId;
    event.cbutton.button = SDL_CONTROLLER_BUTTON_A;
    event.cbutton.state = pressed ? SDL_PRESSED : SDL_RELEASED;
    return event;
}

SDL_Event InputSuspensionTests::axisEvent(int axis, Sint16 value) const
{
    SDL_Event event = {};
    event.type = SDL_CONTROLLERAXISMOTION;
    event.caxis.timestamp = SDL_GetTicks();
    event.caxis.which = instanceId;
    event.caxis.axis = static_cast<Uint8>(axis);
    event.caxis.value = value;
    return event;
}

bool InputSuspensionTests::pushAndDrain(const SDL_Event &event)
{
    SDL_Event queued = event;
    return SDL_PushEvent(&queued) == 1 && daemon->drain();
}

bool InputSuspensionTests::sendButton(bool pressed)
{
    return setPhysicalButton(pressed) && pushAndDrain(buttonEvent(pressed));
}

bool InputSuspensionTests::sendAxis(int axis, Sint16 value)
{
    return setPhysicalAxis(axis, value) && pushAndDrain(axisEvent(axis, value));
}

void InputSuspensionTests::pausedControllerEventsDrainAndPreserveRawMonitor()
{
    JoyButton *button = controller->getActiveSetJoystick()->getJoyButton(SDL_CONTROLLER_BUTTON_A);
    QSignalSpy mappedClicks(button, &JoyButton::clicked);
    QSignalSpy rawClicks(controller, &InputDevice::rawButtonClick);
    QSignalSpy rawReleases(controller, &InputDevice::rawButtonRelease);
    QSignalSpy rawAxis(controller, &InputDevice::rawAxisActivated);
    daemon->setNativeGameInputSuspended(true);
    QVERIFY(controller->isControllerInputEnabled());
    QVERIFY(!controller->isEffectiveInputEnabled());

    QVERIFY(sendButton(true));
    QVERIFY(sendAxis(SDL_CONTROLLER_AXIS_LEFTX, 24000));
    SDL_Event sensor = {};
    sensor.type = SDL_CONTROLLERSENSORUPDATE;
    sensor.csensor.timestamp = SDL_GetTicks();
    sensor.csensor.which = instanceId;
    sensor.csensor.sensor = SDL_SENSOR_GYRO;
    sensor.csensor.data[0] = 1.0f;
    QVERIFY(pushAndDrain(sensor));
    QVERIFY(sendButton(false));
    QCOMPARE(mappedClicks.count(), 0);
    QCOMPARE(rawClicks.count(), 1);
    QCOMPARE(rawReleases.count(), 1);
    QCOMPARE(rawAxis.count(), 1);
    QVERIFY(!button->getButtonState());
    QVERIFY(!button->hasPendingEvent());
}

void InputSuspensionTests::manualDisableSurvivesResume()
{
    JoyButton *button = controller->getActiveSetJoystick()->getJoyButton(SDL_CONTROLLER_BUTTON_A);
    QSignalSpy clicks(button, &JoyButton::clicked);
    daemon->setControllerInputEnabled(controller, false);
    daemon->setNativeGameInputSuspended(true);
    QVERIFY(sendButton(true));
    daemon->setNativeGameInputSuspended(false);
    QVERIFY(!controller->isControllerInputEnabled());
    QVERIFY(!controller->isEffectiveInputEnabled());
    QTest::qWait(2);
    QVERIFY(sendButton(false));
    QVERIFY(sendButton(true));
    QCOMPARE(clicks.count(), 0);
    QVERIFY(!button->getButtonState());
}

void InputSuspensionTests::heldButtonRequiresRelease_data()
{
    QTest::addColumn<bool>("pressedBeforePause");
    QTest::newRow("held before pause") << true;
    QTest::newRow("pressed during pause") << false;
}

void InputSuspensionTests::heldButtonRequiresRelease()
{
    QFETCH(bool, pressedBeforePause);
    JoyButton *button = controller->getActiveSetJoystick()->getJoyButton(SDL_CONTROLLER_BUTTON_A);
    QSignalSpy clicks(button, &JoyButton::clicked);
    if (pressedBeforePause)
    {
        QVERIFY(sendButton(true));
        QCOMPARE(clicks.count(), 1);
    }
    daemon->setNativeGameInputSuspended(true);
    QVERIFY(!button->getButtonState());
    clicks.clear();
    if (!pressedBeforePause)
        QVERIFY(sendButton(true));
    daemon->setNativeGameInputSuspended(false);
    QVERIFY(controller->isEffectiveInputEnabled());
    QVERIFY(controller->isButtonAwaitingNeutral(SDL_CONTROLLER_BUTTON_A));
    QTest::qWait(2);
    QVERIFY(sendButton(true));
    QCOMPARE(clicks.count(), 0);
    QVERIFY(sendButton(false));
    QVERIFY(!controller->isButtonAwaitingNeutral(SDL_CONTROLLER_BUTTON_A));
    QVERIFY(sendButton(true));
    QCOMPARE(clicks.count(), 1);
}

void InputSuspensionTests::queuedPauseEventsCannotReplayAfterResume()
{
    JoyButton *button = controller->getActiveSetJoystick()->getJoyButton(SDL_CONTROLLER_BUTTON_A);
    QSignalSpy clicks(button, &JoyButton::clicked);
    QSignalSpy rawClicks(controller, &InputDevice::rawButtonClick);
    QSignalSpy rawReleases(controller, &InputDevice::rawButtonRelease);
    daemon->setNativeGameInputSuspended(true);
    QVERIFY(setPhysicalButton(false));
    SDL_Event down = buttonEvent(true);
    SDL_Event up = buttonEvent(false);
    QCOMPARE(SDL_PushEvent(&down), 1);
    QCOMPARE(SDL_PushEvent(&up), 1);
    QTest::qWait(2);
    daemon->setNativeGameInputSuspended(false);
    QVERIFY(!controller->isButtonAwaitingNeutral(SDL_CONTROLLER_BUTTON_A));
    QVERIFY(daemon->drain());
    QCOMPARE(clicks.count(), 0);
    QCOMPARE(rawClicks.count(), 1);
    QCOMPARE(rawReleases.count(), 1);
    QVERIFY(!button->getButtonState());
    QTest::qWait(2);
    QVERIFY(sendButton(true));
    QCOMPARE(clicks.count(), 1);
}

void InputSuspensionTests::suspensionCancelsReleaseTimeSetChange()
{
    JoyButton *button = controller->getActiveSetJoystick()->getJoyButton(SDL_CONTROLLER_BUTTON_A);
    QVERIFY(button->setAssignedSlot(1, JoyButtonSlot::JoySetChange));
    QSignalSpy changes(button, &JoyButton::setChangeActivated);
    QVERIFY(sendButton(true));
    QTRY_VERIFY_WITH_TIMEOUT(button->hasActiveSlots(), 1000);
    QCOMPARE(controller->getActiveSetNumber(), 0);
    daemon->setNativeGameInputSuspended(true);
    QVERIFY(!button->hasActiveSlots());
    QVERIFY(!button->getButtonState());
    QTest::qWait(20); // Release of JoySetChange used to rearm its zero-delay timer.
    QCOMPARE(changes.count(), 0);
    QCOMPARE(controller->getActiveSetNumber(), 0);
    daemon->setNativeGameInputSuspended(false);
    QTest::qWait(2);
    QVERIFY(sendButton(false));
    QVERIFY(sendButton(true));
    QTRY_VERIFY_WITH_TIMEOUT(button->hasActiveSlots(), 1000);
    QVERIFY(sendButton(false));
    QTRY_COMPARE_WITH_TIMEOUT(controller->getActiveSetNumber(), 1, 1000);
    QCOMPARE(changes.count(), 1);
}

void InputSuspensionTests::calibratedProfileReloadStaysInactive_data()
{
    QTest::addColumn<bool>("neutralAtResume");
    QTest::newRow("calibrated neutral") << true;
    QTest::newRow("calibrated displaced") << false;
}

void InputSuspensionTests::calibratedProfileReloadStaysInactive()
{
    QFETCH(bool, neutralAtResume);
    controller->updateStickCalibration(0, 12000.0, 1.0, 0.0, 1.0);
    daemon->setNativeGameInputSuspended(true);
    QVERIFY(sendAxis(SDL_CONTROLLER_AXIS_LEFTX, neutralAtResume ? -12000 : 12000));

    // These are the profile reader's state capture/rebuild/replay steps.
    controller->transferReset();
    controller->applyStickCalibration(0, 12000.0, 1.0, 0.0, 1.0);
    JoyControlStick *stick = controller->getActiveSetJoystick()->getJoyStick(0);
    QVERIFY(stick != nullptr);
    JoyControlStickButton *right = stick->getDirectionButton(JoyControlStick::StickRight);
    QSignalSpy clicks(right, &JoyButton::clicked);
    controller->reInitButtons();
    QCoreApplication::processEvents();
    QCOMPARE(clicks.count(), 0);
    QVERIFY(!right->getButtonState());
    QCOMPARE(stick->getCurrentDirection(), JoyControlStick::StickCentered);

    daemon->setNativeGameInputSuspended(false);
    QCOMPARE(controller->isAxisAwaitingNeutral(SDL_CONTROLLER_AXIS_LEFTX), !neutralAtResume);
    QTest::qWait(2);
    if (!neutralAtResume)
    {
        QVERIFY(sendAxis(SDL_CONTROLLER_AXIS_LEFTX, 14000));
        QCOMPARE(clicks.count(), 0);
        QVERIFY(sendAxis(SDL_CONTROLLER_AXIS_LEFTX, 0)); // Raw zero is still displaced after calibration.
        QVERIFY(controller->isAxisAwaitingNeutral(SDL_CONTROLLER_AXIS_LEFTX));
        QVERIFY(sendAxis(SDL_CONTROLLER_AXIS_LEFTX, -12000));
        QVERIFY(!controller->isAxisAwaitingNeutral(SDL_CONTROLLER_AXIS_LEFTX));
        QCOMPARE(clicks.count(), 0);
    }
    QVERIFY(sendAxis(SDL_CONTROLLER_AXIS_LEFTX, 16000));
    QCOMPARE(clicks.count(), 1);
}

void InputSuspensionTests::radialStickNeutralDoesNotLatchChildAxes()
{
    JoyControlStick *stick = controller->getActiveSetJoystick()->getJoyStick(0);
    stick->setDeadZone(15000);
    stick->getAxisX()->setDeadZone(8000);
    stick->getAxisY()->setDeadZone(8000);
    JoyControlStickButton *right = stick->getDirectionButton(JoyControlStick::StickRight);
    QSignalSpy clicks(right, &JoyButton::clicked);

    // Each coordinate exceeds its axis dead zone, but the stick's vector
    // length is about 14142 and therefore remains inside its radial dead zone.
    QVERIFY(sendAxis(SDL_CONTROLLER_AXIS_LEFTX, 10000));
    QVERIFY(sendAxis(SDL_CONTROLLER_AXIS_LEFTY, 10000));
    QVERIFY(stick->inDeadZone());
    QCOMPARE(clicks.count(), 0);
    daemon->setNativeGameInputSuspended(true);
    daemon->setNativeGameInputSuspended(false);
    QVERIFY(!controller->isAxisAwaitingNeutral(SDL_CONTROLLER_AXIS_LEFTX));
    QVERIFY(!controller->isAxisAwaitingNeutral(SDL_CONTROLLER_AXIS_LEFTY));
    QTest::qWait(2);
    QVERIFY(sendAxis(SDL_CONTROLLER_AXIS_LEFTX, 24000));
    QCOMPARE(clicks.count(), 1);
}

void InputSuspensionTests::suspensionClearsSpringQueue()
{
    PadderCommon::springModeInfo spring = {};
    spring.displacementX = 0.5;
    spring.displacementY = 0.5;
    spring.width = 640;
    spring.height = 480;
    JoyButton::getSpringXSpeeds()->append(spring);
    JoyButton::getSpringYSpeeds()->append(spring);
    QVERIFY(!JoyButton::getSpringXSpeeds()->isEmpty());
    QVERIFY(!JoyButton::getSpringYSpeeds()->isEmpty());
    daemon->setNativeGameInputSuspended(true);
    QVERIFY(JoyButton::getSpringXSpeeds()->isEmpty());
    QVERIFY(JoyButton::getSpringYSpeeds()->isEmpty());
    daemon->setNativeGameInputSuspended(false);
    QVERIFY(JoyButton::getSpringXSpeeds()->isEmpty());
    QVERIFY(JoyButton::getSpringYSpeeds()->isEmpty());
}

QTEST_MAIN(InputSuspensionTests)
#include "testinputsuspension.moc"
