/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef CONTROLWIDGETCLEANUP_H
#define CONTROLWIDGETCLEANUP_H

#include <QLayout>
#include <QList>
#include <QPointer>
#include <QWidget>

namespace ControlWidgetCleanup {
enum class Mode
{
    Deferred,
    Immediate
};

class State final
{
  public:
    void clearLayout(QLayout *layout, Mode mode)
    {
        if (mode == Mode::Immediate)
        {
            // A previous display rebuild may have detached controls that still have queued callbacks.
            QList<QPointer<QWidget>> retired;
            retired.swap(retiredWidgets);
            for (const QPointer<QWidget> &widget : retired)
                delete widget.data();
        } else
        {
            for (auto it = retiredWidgets.begin(); it != retiredWidgets.end();)
            {
                if (it->isNull())
                    it = retiredWidgets.erase(it);
                else
                    ++it;
            }
        }
        removeLayoutWidgets(layout, mode);
    }

  private:
    QList<QPointer<QWidget>> retiredWidgets;

    void removeLayoutWidgets(QLayout *layout, Mode mode)
    {
        if (layout == nullptr)
            return;

        while (QLayoutItem *item = layout->takeAt(0))
        {
            if (QWidget *widget = item->widget())
            {
                if (mode == Mode::Immediate)
                    delete widget;
                else
                {
                    retiredWidgets.append(QPointer<QWidget>(widget));
                    widget->deleteLater();
                }
            } else if (QLayout *childLayout = item->layout())
                removeLayoutWidgets(childLayout, mode);

            delete item;
        }
    }
};
} // namespace ControlWidgetCleanup

#endif // CONTROLWIDGETCLEANUP_H
