// Right-click menu on ImageViewWidget: pure-mouse users need a way to add and
// remove the crosshair and open the profile without holding Ctrl. The menu also
// has to mirror the widget state — "Remove crosshair" and "at crosshair" must be
// greyed out while no crosshair exists.
#include <QAction>
#include <QContextMenuEvent>
#include <QImage>
#include <QMenu>
#include <QScopedPointer>
#include <QSignalSpy>
#include <QTest>
#include <QtMath>

#include "ImageViewWidget.h"
#include "qcustomplot.h"

static QCustomPlot *plotOf(ImageViewWidget *widget)
{
    return widget->findChild<QCustomPlot *>();
}

static const int kImageWidth = 40;
static const int kImageHeight = 20;

static QImage testImage()
{
    QImage image(kImageWidth, kImageHeight, QImage::Format_Grayscale8);
    for (int y = 0; y < kImageHeight; ++y) {
        for (int x = 0; x < kImageWidth; ++x) {
            image.setPixel(x, y, (x + y * kImageWidth) % 256);
        }
    }
    return image;
}

static void showLayout(ImageViewWidget *widget)
{
    widget->show();
    QCoreApplication::processEvents();
    plotOf(widget)->replot();
    QCoreApplication::processEvents();
}

static QPoint pixelAt(QCustomPlot *plot, const QPoint &widgetPos)
{
    return QPoint(static_cast<int>(qFloor(plot->xAxis->pixelToCoord(widgetPos.x()))),
                  static_cast<int>(qFloor(plot->yAxis->pixelToCoord(widgetPos.y()))));
}

static QMenu *openContextMenu(ImageViewWidget *widget, const QPoint &pos,
                              Qt::KeyboardModifiers modifiers = Qt::NoModifier)
{
    auto *plot = plotOf(widget);
    QContextMenuEvent event(QContextMenuEvent::Mouse, pos, plot->mapToGlobal(pos), modifiers);
    QCoreApplication::sendEvent(plot, &event);
    QCoreApplication::processEvents();
    return widget->findChild<QMenu *>(QStringLiteral("imageContextMenu"));
}

static void closeMenus(ImageViewWidget *widget)
{
    const QList<QMenu *> menus = widget->findChildren<QMenu *>();
    for (QMenu *menu : menus) {
        menu->close();
    }
    QCoreApplication::processEvents();
}

static QAction *actionOf(ImageViewWidget *widget, const char *name)
{
    return widget->findChild<QAction *>(QLatin1String(name));
}

class TestImageContextMenu : public QObject
{
    Q_OBJECT

private:
    ImageViewWidget *makeWidget(bool withImage = true)
    {
        auto *widget = new ImageViewWidget;
        widget->resize(800, 600);
        if (withImage) {
            widget->setImage(testImage());
        }
        showLayout(widget);
        return widget;
    }

private slots:
    void test_actions_follow_widget_state()
    {
        QScopedPointer<ImageViewWidget> widget(makeWidget());
        auto *plot = plotOf(widget.data());

        QMenu *menu = openContextMenu(widget.data(), plot->axisRect()->rect().center());
        QVERIFY2(menu, "右键没有弹出菜单");

        QAction *add = actionOf(widget.data(), "contextActionAddCrosshair");
        QAction *remove = actionOf(widget.data(), "contextActionRemoveCrosshair");
        QAction *profileAtMouse = actionOf(widget.data(), "contextActionProfileAtMouse");
        QAction *profileAtCrosshair = actionOf(widget.data(), "contextActionProfileAtCrosshair");
        QVERIFY(add);
        QVERIFY(remove);
        QVERIFY(profileAtMouse);
        QVERIFY(profileAtCrosshair);

        QVERIFY2(add->isEnabled(), "有图像时 Add crosshair 应当可用");
        QVERIFY2(profileAtMouse->isEnabled(), "有图像时 View profile/at mouse 应当可用");
        QVERIFY2(!remove->isEnabled(), "没有十字线时 Remove crosshair 应当置灰");
        QVERIFY2(!profileAtCrosshair->isEnabled(), "没有十字线时 at crosshair 应当置灰");

        widget->addCrosshair(3, 4);
        closeMenus(widget.data());

        menu = openContextMenu(widget.data(), plot->axisRect()->rect().center());
        QVERIFY(menu);
        QVERIFY(actionOf(widget.data(), "contextActionRemoveCrosshair")->isEnabled());
        QVERIFY(actionOf(widget.data(), "contextActionProfileAtCrosshair")->isEnabled());
    }

    void test_actions_disabled_without_image()
    {
        QScopedPointer<ImageViewWidget> widget(makeWidget(false));
        auto *plot = plotOf(widget.data());

        QMenu *menu = openContextMenu(widget.data(), plot->rect().center());
        QVERIFY(menu);
        QVERIFY(!actionOf(widget.data(), "contextActionAddCrosshair")->isEnabled());
        QVERIFY(!actionOf(widget.data(), "contextActionRemoveCrosshair")->isEnabled());
        QVERIFY(!actionOf(widget.data(), "contextActionProfileAtMouse")->isEnabled());
        QVERIFY(!actionOf(widget.data(), "contextActionProfileAtCrosshair")->isEnabled());
        QVERIFY(!actionOf(widget.data(), "contextActionResetZoom")->isEnabled());
    }

    void test_blank_margin_disables_position_actions()
    {
        // 40x20 的图放进 800x600 的控件，上下会留下大片留白。
        // 那里的右键没有像素可落，位置相关的项应当置灰
        QScopedPointer<ImageViewWidget> widget(makeWidget());
        auto *plot = plotOf(widget.data());
        const QRect area = plot->axisRect()->rect();
        QVERIFY2(area.top() > 2, "测试前提不成立：图像区域上方没有留白");

        QMenu *menu = openContextMenu(widget.data(), QPoint(area.center().x(), 2));
        QVERIFY(menu);
        QVERIFY(!actionOf(widget.data(), "contextActionAddCrosshair")->isEnabled());
        QVERIFY(!actionOf(widget.data(), "contextActionProfileAtMouse")->isEnabled());
        QVERIFY2(actionOf(widget.data(), "contextActionResetZoom")->isEnabled(),
                 "复位缩放与鼠标位置无关，应当仍然可用");
    }

    void test_add_crosshair_at_mouse()
    {
        QScopedPointer<ImageViewWidget> widget(makeWidget());
        auto *plot = plotOf(widget.data());
        const QPoint pos = plot->axisRect()->rect().center();
        const QPoint expected = pixelAt(plot, pos);

        QSignalSpy moved(widget.data(), &ImageViewWidget::crosshairMoved);

        QMenu *menu = openContextMenu(widget.data(), pos);
        QVERIFY(menu);
        actionOf(widget.data(), "contextActionAddCrosshair")->trigger();
        QCoreApplication::processEvents();

        QCOMPARE(widget->crosshairCount(), 1);
        QCOMPARE(widget->crosshairPositions().first().toPoint(), expected);
        QCOMPARE(moved.count(), 1);
        QCOMPARE(moved.first().at(0).toPointF().toPoint(), expected);
    }

    void test_remove_crosshair()
    {
        QScopedPointer<ImageViewWidget> widget(makeWidget());
        auto *plot = plotOf(widget.data());
        widget->addCrosshair(3, 4);

        QSignalSpy cleared(widget.data(), &ImageViewWidget::crosshairsCleared);

        QMenu *menu = openContextMenu(widget.data(), plot->axisRect()->rect().center());
        QVERIFY(menu);
        actionOf(widget.data(), "contextActionRemoveCrosshair")->trigger();
        QCoreApplication::processEvents();

        QCOMPARE(widget->crosshairCount(), 0);
        QCOMPARE(cleared.count(), 1);
    }

    void test_view_profile_at_mouse_places_crosshair()
    {
        QScopedPointer<ImageViewWidget> widget(makeWidget());
        auto *plot = plotOf(widget.data());
        const QPoint pos = plot->axisRect()->rect().center();
        const QPoint expected = pixelAt(plot, pos);

        QSignalSpy requested(widget.data(), &ImageViewWidget::profileRequested);

        QMenu *menu = openContextMenu(widget.data(), pos);
        QVERIFY(menu);
        actionOf(widget.data(), "contextActionProfileAtMouse")->trigger();
        QCoreApplication::processEvents();

        QCOMPARE(widget->crosshairCount(), 1);
        QCOMPARE(widget->crosshairPositions().first().toPoint(), expected);
        QCOMPARE(requested.count(), 1);
        QCOMPARE(requested.first().at(0).toPointF().toPoint(), expected);
    }

    void test_view_profile_at_crosshair_uses_existing_position()
    {
        QScopedPointer<ImageViewWidget> widget(makeWidget());
        auto *plot = plotOf(widget.data());
        widget->addCrosshair(7, 9);

        QSignalSpy requested(widget.data(), &ImageViewWidget::profileRequested);

        QMenu *menu = openContextMenu(widget.data(), plot->axisRect()->rect().center());
        QVERIFY(menu);
        QAction *action = actionOf(widget.data(), "contextActionProfileAtCrosshair");
        QVERIFY(action->isEnabled());
        action->trigger();
        QCoreApplication::processEvents();

        QCOMPARE(requested.count(), 1);
        QCOMPARE(requested.first().at(0).toPointF().toPoint(), QPoint(7, 9));
        // 十字线本身不该被这次操作挪动
        QCOMPARE(widget->crosshairPositions().first().toPoint(), QPoint(7, 9));
    }

    void test_ctrl_right_click_still_removes_crosshair()
    {
        QScopedPointer<ImageViewWidget> widget(makeWidget());
        auto *plot = plotOf(widget.data());
        widget->addCrosshair(3, 4);

        QMenu *menu = openContextMenu(widget.data(), plot->axisRect()->rect().center(),
                                      Qt::ControlModifier);

        QCOMPARE(widget->crosshairCount(), 0);
        QVERIFY2(!menu, "Ctrl+右键是快捷删除，不该再弹菜单");
    }

    void test_reset_zoom_action_restores_full_image()
    {
        QScopedPointer<ImageViewWidget> widget(makeWidget());
        auto *plot = plotOf(widget.data());

        const QRect area = plot->axisRect()->rect();
        // 用一次选框缩放把轴范围缩进去
        const QPoint press(area.left() + area.width() / 4, area.center().y());
        const QPoint release(press.x() + 100, press.y() + 20);
        auto sendMouse = [plot](QEvent::Type type, const QPoint &pos,
                                Qt::MouseButton button, Qt::MouseButtons buttons) {
            QMouseEvent event(type, QPointF(pos), QPointF(plot->mapToGlobal(pos)),
                              button, buttons, Qt::NoModifier);
            QCoreApplication::sendEvent(plot, &event);
        };
        sendMouse(QEvent::MouseButtonPress, press, Qt::LeftButton, Qt::LeftButton);
        sendMouse(QEvent::MouseMove, release, Qt::NoButton, Qt::LeftButton);
        sendMouse(QEvent::MouseButtonRelease, release, Qt::LeftButton, Qt::NoButton);
        QCoreApplication::processEvents();
        QVERIFY(plot->xAxis->range().size() < kImageWidth);

        QMenu *menu = openContextMenu(widget.data(), area.center());
        QVERIFY(menu);
        actionOf(widget.data(), "contextActionResetZoom")->trigger();
        QCoreApplication::processEvents();

        QCOMPARE(plot->xAxis->range().size(), static_cast<double>(kImageWidth));
        QCOMPARE(plot->yAxis->range().size(), static_cast<double>(kImageHeight));
    }
};

QTEST_MAIN(TestImageContextMenu)
#include "test_image_context_menu.moc"
