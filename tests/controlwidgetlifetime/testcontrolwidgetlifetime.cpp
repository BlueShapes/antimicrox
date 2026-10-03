/* SPDX-License-Identifier: GPL-3.0-or-later */
#include "controlwidgetcleanup.h"
#include "flashbuttonwidget.h"

#include <QApplication>
#include <QCoreApplication>
#include <QEvent>
#include <QPointer>
#include <QThread>
#include <QVBoxLayout>

#include <atomic>
#include <iostream>

namespace
{
class LabelButton : public FlashButtonWidget
{
  public:
    LabelButton(QObject *model, const std::atomic<bool> &modelAlive, int &reads, int &staleReads, QWidget *parent)
        : FlashButtonWidget(parent), model(model), modelAlive(modelAlive), reads(reads), staleReads(staleReads)
    {
    }

    void disableFlashes() override {}
    void enableFlashes() override {}

  protected:
    QString generateLabel() override
    {
        ++reads;
        if (!modelAlive.load())
        {
            ++staleReads;
            return {};
        }
        return model->objectName();
    }

  private:
    QObject *model;
    const std::atomic<bool> &modelAlive;
    int &reads;
    int &staleReads;
};

bool check(bool condition, const char *description)
{
    if (!condition)
        std::cerr << "FAIL: " << description << '\n';
    return condition;
}

bool removesQueuedLabelsBeforeModelReset()
{
    ControlWidgetCleanup::State cleanup;
    QThread inputThread;
    auto *context = new QObject;
    context->moveToThread(&inputThread);
    QObject::connect(&inputThread, &QThread::finished, context, &QObject::deleteLater);
    inputThread.start();

    auto *model = new QObject;
    model->setObjectName(QStringLiteral("Stick 1"));
    model->moveToThread(&inputThread);
    std::atomic<bool> modelAlive{true};
    QObject::connect(model, &QObject::destroyed, [&modelAlive] { modelAlive.store(false); });

    QWidget owner;
    QVBoxLayout layout(&owner);
    auto *group = new QWidget(&owner);
    auto *groupLayout = new QVBoxLayout(group);
    int reads = 0;
    int staleReads = 0;
    auto *button = new LabelButton(model, modelAlive, reads, staleReads, group);
    groupLayout->addWidget(button);
    layout.addWidget(group);
    QPointer<QWidget> groupGuard(group);
    QPointer<LabelButton> buttonGuard(button);
    bool passed = check(QMetaObject::invokeMethod(button, "refreshLabel", Qt::QueuedConnection),
                        "queue the production label-refresh slot");

    cleanup.clearLayout(&layout, ControlWidgetCleanup::Mode::Immediate);
    passed &= check(groupGuard.isNull() && buttonGuard.isNull(),
                    "destroy old controls and descendants before replacing the input model");
    passed &= check(layout.count() == 0, "detach old layout items");

    passed &= check(QMetaObject::invokeMethod(context, [model] { delete model; }, Qt::BlockingQueuedConnection),
                    "replace the input model on its own thread");
    passed &= check(!modelAlive.load(), "the old input model has been destroyed");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
    passed &= check(reads == 0 && staleReads == 0, "discard queued callbacks to removed controls");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    inputThread.quit();
    inputThread.wait();
    return passed;
}

bool keepsDisplayOnlyRebuildsDeferred()
{
    ControlWidgetCleanup::State cleanup;
    QObject model;
    model.setObjectName(QStringLiteral("Stick 2"));
    std::atomic<bool> modelAlive{true};
    QWidget owner;
    QVBoxLayout layout(&owner);
    int reads = 0;
    int staleReads = 0;
    auto *button = new LabelButton(&model, modelAlive, reads, staleReads, &owner);
    layout.addWidget(button);
    QPointer<LabelButton> guard(button);
    QMetaObject::invokeMethod(button, "refreshLabel", Qt::QueuedConnection);

    cleanup.clearLayout(&layout, ControlWidgetCleanup::Mode::Deferred);
    bool passed = check(!guard.isNull(), "display-only rebuilds keep active signal senders alive until event delivery");
    passed &= check(layout.count() == 0, "display-only rebuilds detach old controls immediately");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
    passed &= check(reads == 1 && staleReads == 0, "the deferred control can finish queued work while its model is alive");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    passed &= check(guard.isNull(), "deferred controls are destroyed at the deferred-delete boundary");
    return passed;
}

bool removesNestedLayoutsAndSpacers()
{
    ControlWidgetCleanup::State cleanup;
    QWidget owner;
    QVBoxLayout layout(&owner);
    auto *nested = new QVBoxLayout;
    auto *child = new QWidget(&owner);
    QPointer<QWidget> guard(child);
    nested->addWidget(child);
    nested->addStretch();
    layout.addLayout(nested);
    layout.addStretch();
    cleanup.clearLayout(nullptr, ControlWidgetCleanup::Mode::Immediate);
    cleanup.clearLayout(&layout, ControlWidgetCleanup::Mode::Immediate);
    bool passed = check(layout.count() == 0 && guard.isNull(), "remove nested control layouts and tolerate spacers");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    return passed;
}

bool removesRetiredControlsBeforeModelReset()
{
    ControlWidgetCleanup::State cleanup;
    auto *model = new QObject;
    model->setObjectName(QStringLiteral("Retired stick"));
    std::atomic<bool> modelAlive{true};
    QWidget owner;
    QVBoxLayout layout(&owner);
    int reads = 0;
    int staleReads = 0;
    auto *oldButton = new LabelButton(model, modelAlive, reads, staleReads, &owner);
    QPointer<LabelButton> oldGuard(oldButton);
    layout.addWidget(oldButton);
    QMetaObject::invokeMethod(oldButton, "refreshLabel", Qt::QueuedConnection);

    cleanup.clearLayout(&layout, ControlWidgetCleanup::Mode::Deferred);
    auto *replacement = new QWidget(&owner);
    QPointer<QWidget> replacementGuard(replacement);
    layout.addWidget(replacement);
    cleanup.clearLayout(&layout, ControlWidgetCleanup::Mode::Immediate);
    bool passed = check(oldGuard.isNull() && replacementGuard.isNull(),
                        "model reset also destroys controls retired by an earlier display-only rebuild");

    delete model;
    modelAlive.store(false);
    QCoreApplication::sendPostedEvents(nullptr, QEvent::MetaCall);
    passed &= check(reads == 0 && staleReads == 0, "retired controls cannot receive callbacks after model reset");
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
    return passed;
}
} // namespace

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    const bool resetPassed = removesQueuedLabelsBeforeModelReset();
    const bool displayPassed = keepsDisplayOnlyRebuildsDeferred();
    const bool nestedPassed = removesNestedLayoutsAndSpacers();
    const bool retiredPassed = removesRetiredControlsBeforeModelReset();
    if (!resetPassed || !displayPassed || !nestedPassed || !retiredPassed)
        return 1;
    std::cout << "ControlWidgetLifetimeTests: 4 cases passed\n";
    return 0;
}
