#include "sticknamestate.h"

#include <QThread>
#include <QStringList>

#include <atomic>
#include <functional>
#include <iostream>
#include <utility>

namespace
{
int failures = 0;

void expect(bool condition, const char *message)
{
    if (!condition)
    {
        std::cerr << "FAIL: " << message << '\n';
        ++failures;
    }
}

class FunctionThread final : public QThread
{
  public:
    explicit FunctionThread(std::function<void()> function) : function(std::move(function)) {}

  protected:
    void run() override { function(); }

  private:
    std::function<void()> function;
};

void testNameRendering()
{
    StickNameState state;
    const QString translatedStickLabel = QStringLiteral("Translated stick");

    expect(state.partialName(translatedStickLabel, 3, false, false)
               == QStringLiteral("Translated stick 3"),
           "an unnamed stick uses the translated numbered label");
    expect(state.partialName(translatedStickLabel, 3, true, true)
               == QStringLiteral("Translated stick 3"),
           "numbered labels retain their full prefix regardless of display flags");

    state.setDefaultName(QStringLiteral("Default name"));
    expect(state.partialName(translatedStickLabel, 3, false, false) == QStringLiteral("Default name"),
           "a default name is used when no display name is selected");
    expect(state.partialName(translatedStickLabel, 3, true, false)
               == QStringLiteral("Translated stick Default name"),
           "the translated label prefix is used for full default names");

    expect(state.setCustomName(QStringLiteral("Custom name")), "a new custom name is accepted");
    expect(state.partialName(translatedStickLabel, 3, false, true) == QStringLiteral("Custom name"),
           "display names select the custom name");
    expect(state.partialName(translatedStickLabel, 3, true, true)
               == QStringLiteral("Translated stick Custom name"),
           "the translated label prefix is used for full custom names");
    expect(state.partialName(translatedStickLabel, 3, false, false) == QStringLiteral("Default name"),
           "custom names remain hidden when displayNames is false");

    expect(state.customName() == QStringLiteral("Custom name"), "rendering leaves the custom name unchanged");
    expect(state.defaultName() == QStringLiteral("Default name"), "rendering leaves the default name unchanged");
}

void testNameUpdatesAndReset()
{
    StickNameState state;
    expect(state.setCustomName(QStringLiteral("Accepted")), "a valid changed custom name is accepted");
    expect(!state.setCustomName(QStringLiteral("Accepted")), "an unchanged custom name is not reported as changed");

    const QString maximumLengthName(20, QLatin1Char('m'));
    expect(state.setCustomName(maximumLengthName), "a custom name of exactly 20 characters is accepted");
    const QString tooLong(21, QLatin1Char('x'));
    expect(!state.setCustomName(tooLong), "a custom name longer than 20 characters is rejected");
    expect(state.customName() == maximumLengthName, "a rejected name leaves the current value intact");

    state.setDefaultName(QStringLiteral("Preserved default"));
    state.clearCustomName();
    expect(state.customName().isEmpty(), "reset clears the custom name");
    expect(state.defaultName() == QStringLiteral("Preserved default"), "reset preserves the default name");
    expect(state.partialName(QStringLiteral("Stick"), 1, false, true) == QStringLiteral("Preserved default"),
           "rendering after reset falls back to the default name");

    StickNameState destination;
    destination.setDefaultName(QStringLiteral("Destination default"));
    expect(destination.setCustomName(QStringLiteral("Destination custom")), "destination custom name is prepared");
    expect(state.setCustomName(QStringLiteral("Source custom")), "source custom name is prepared");
    destination.copyCustomNameFrom(state);
    expect(destination.customName() == QStringLiteral("Source custom"), "copy transfers the custom name");
    expect(destination.defaultName() == QStringLiteral("Destination default"), "copy leaves the default name alone");
}

void testConcurrentNameSnapshots()
{
    StickNameState state;
    const QString label = QStringLiteral("Stick");
    const QString customA = QStringLiteral("Custom-A");
    const QString customB = QStringLiteral("Custom-B");
    const QString defaultA = QStringLiteral("Heap-backed default A ") + QString(384, QLatin1Char('A'));
    const QString defaultB = QStringLiteral("Heap-backed default B ") + QString(384, QLatin1Char('B'));

    QStringList validRenderedNames;
    validRenderedNames << QStringLiteral("Stick 7") << QStringLiteral("Stick Custom-A")
                       << QStringLiteral("Stick Custom-B") << QStringLiteral("Stick ") + defaultA
                       << QStringLiteral("Stick ") + defaultB;

    std::atomic<bool> start{false};
    std::atomic<bool> invalidSnapshot{false};
    std::atomic<int> completedReads{0};

    FunctionThread writer([&]() {
        while (!start.load(std::memory_order_acquire))
            QThread::yieldCurrentThread();

        for (int index = 0; index < 12000; ++index)
        {
            state.setDefaultName((index & 1) ? defaultA : defaultB);
            state.setCustomName((index & 1) ? customA : customB);
        }
    });
    FunctionThread resetter([&]() {
        while (!start.load(std::memory_order_acquire))
            QThread::yieldCurrentThread();

        for (int index = 0; index < 6000; ++index)
            state.clearCustomName();
    });

    auto makeReader = [&]() {
        return FunctionThread([&]() {
            while (!start.load(std::memory_order_acquire))
                QThread::yieldCurrentThread();

            for (int index = 0; index < 12000; ++index)
            {
                const QString customSnapshot = state.customName();
                const QString defaultSnapshot = state.defaultName();
                const QString rendered = state.partialName(label, 7, true, true);

                if ((customSnapshot != QString() && customSnapshot != customA && customSnapshot != customB)
                    || (defaultSnapshot != QString() && defaultSnapshot != defaultA && defaultSnapshot != defaultB)
                    || !validRenderedNames.contains(rendered))
                {
                    invalidSnapshot.store(true, std::memory_order_release);
                    break;
                }
                completedReads.fetch_add(1, std::memory_order_relaxed);
            }
        });
    };

    FunctionThread readerA = makeReader();
    FunctionThread readerB = makeReader();
    FunctionThread readerC = makeReader();
    writer.start();
    resetter.start();
    readerA.start();
    readerB.start();
    readerC.start();
    start.store(true, std::memory_order_release);

    writer.wait();
    resetter.wait();
    readerA.wait();
    readerB.wait();
    readerC.wait();

    expect(!invalidSnapshot.load(std::memory_order_acquire), "concurrent reads and rendering return complete names");
    expect(completedReads.load(std::memory_order_relaxed) == 36000,
           "all bounded concurrent reader iterations complete");
}
} // namespace

int main()
{
    testNameRendering();
    testNameUpdatesAndReset();
    testConcurrentNameSnapshots();

    if (failures == 0)
        std::cout << "All stick name state tests passed.\n";
    return failures == 0 ? 0 : 1;
}
