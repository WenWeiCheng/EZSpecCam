#include <QCoreApplication>
#include <QImage>
#include <QObject>
#include <QSignalSpy>
#include <QTest>
#include <QVector>

#include "Theme.h"
#include "ImageViewWidget.h"
#include "SpectrumViewWidget.h"
#include "qcustomplot.h"


// QCustomPlot 没有公开背景画笔的 getter，QBrush 存进去就取不回来了。
// 干脆按用户实际看到的样子验：渲染出来数像素。
static int countColor(const QCustomPlot *plot, const QColor &color)
{
    const QImage image = const_cast<QCustomPlot *>(plot)->grab().toImage();
    const QRect area = plot->axisRect()->rect();
    int hits = 0;
    for (int y = area.top(); y <= area.bottom(); ++y) {
        for (int x = area.left(); x <= area.right(); ++x) {
            if (image.pixelColor(x, y) == color) {
                ++hits;
            }
        }
    }
    return hits;
}

// 深浅配色的行为测试。重点不是「好看」，而是「切到深色之后没有一处还是浅色」：
// QCustomPlot 根本不读 QWidget 的调色板，忘了显式设色的地方在浅色下看不出问题，
// 一到深色就变成黑底黑字。
class TestTheme : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() {}
    void cleanupTestCase() {}
    void init()
    {
        // Theme 是单例，每个用例都从「跟随系统」这个出厂状态开始
        m_theme = Theme::instance();
        m_theme->setMode(Theme::Mode::System);
    }
    void cleanup() {}

    void test_light_curve_is_black()
    {
        const Theme::PlotColors c = Theme::plotColorsFor(Theme::Mode::Light);
        QCOMPARE(c.curve, QColor(Qt::black));
        QCOMPARE(c.background, QColor(Qt::white));
        QCOMPARE(c.text, QColor(Qt::black));
    }

    void test_dark_curve_is_not_black()
    {
        const Theme::PlotColors c = Theme::plotColorsFor(Theme::Mode::Dark);
        // 黑底上的黑曲线等于没有曲线
        QVERIFY(c.curve != QColor(Qt::black));
        QVERIFY(c.background.lightness() < 128);
    }

    void test_every_plot_color_contrasts_with_background()
    {
        // 每一项画在绘图区背景上的颜色都必须和背景拉开亮度差，
        // 否则深色下会出现「黑底黑字」这种整块看不见的东西
        for (Theme::Mode mode : {Theme::Mode::Light, Theme::Mode::Dark}) {
            const Theme::PlotColors c = Theme::plotColorsFor(mode);
            const int bg = c.background.lightness();

            for (const QColor &drawn : {c.axis, c.text, c.curve, c.overlayText}) {
                const int delta = qAbs(drawn.lightness() - bg);
                QVERIFY2(delta >= 40,
                         qPrintable(QStringLiteral("mode=%1 color=%2 delta=%3")
                                        .arg(int(mode))
                                        .arg(drawn.name())
                                        .arg(delta)));
            }
        }
    }

    void test_overlay_label_text_reads_on_its_own_background()
    {
        // 悬浮读数是个不透明色块，两种主题下文字都必须能从块里读出来。
        // 浅色下块底是白的、和绘图区同色，靠一圈黑边区分——这是原有设计，
        // 所以不断言块底必须和绘图区背景不同。
        for (Theme::Mode mode : {Theme::Mode::Light, Theme::Mode::Dark}) {
            const Theme::PlotColors c = Theme::plotColorsFor(mode);
            QVERIFY2(qAbs(c.overlayText.lightness() - c.overlayBackground.lightness()) >= 40,
                     qPrintable(QStringLiteral("mode=%1").arg(int(mode))));
        }
    }

    void test_dark_overlay_box_differs_from_plot_well()
    {
        // 深色下绘图区背景很深，读数框若同色就会和绘图区糊成一片，
        // 这里要求它比绘图区背景亮一档
        const Theme::PlotColors c = Theme::plotColorsFor(Theme::Mode::Dark);
        QVERIFY(c.overlayBackground != c.background);
        QVERIFY(c.overlayBackground.lightness() > c.background.lightness());
    }

    void test_dark_palette_text_readable_on_window()
    {
        const QPalette p = Theme::darkPalette();
        const QColor window = p.color(QPalette::Window);
        const QColor text = p.color(QPalette::WindowText);

        QVERIFY(window.lightness() < 128);
        QVERIFY(text.lightness() > 128);
        // 禁用态不能淡到和背景糊在一起
        QVERIFY(p.color(QPalette::Disabled, QPalette::WindowText).lightness() > window.lightness());
    }

    void test_dark_palette_covers_link_and_tooltip()
    {
        // 这两个角色忘了设的话，链接和悬浮提示在深色下会退回系统默认的浅色
        const QPalette p = Theme::darkPalette();
        QVERIFY(p.color(QPalette::ToolTipBase).lightness() < 128);
        QVERIFY(p.color(QPalette::ToolTipText).lightness() > 128);
        QVERIFY(p.color(QPalette::Link) != QPalette().color(QPalette::Link));
    }

    void test_setMode_switches_palette_and_emits()
    {
        QSignalSpy spy(m_theme, &Theme::themeChanged);

        m_theme->setMode(Theme::Mode::Dark);
        QCOMPARE(m_theme->mode(), Theme::Mode::Dark);
        QVERIFY(m_theme->isDark());
        QVERIFY(qApp->palette().color(QPalette::Window).lightness() < 128);
        QCOMPARE(spy.count(), 1);

        // 切回浅色必须回到系统原样，而不是回不去了
        m_theme->setMode(Theme::Mode::Light);
        QVERIFY(!m_theme->isDark());
        QVERIFY(qApp->palette().color(QPalette::Window).lightness() > 128);
        QCOMPARE(spy.count(), 2);

        // 重复设同一个模式不算变化，不该再广播一次
        m_theme->setMode(Theme::Mode::Light);
        QCOMPARE(spy.count(), 2);
    }

    void test_plot_background_turns_dark()
    {
        // 空白绘图区整片都该是背景色，深色下渲染出来必须是深灰而不是白
        QCustomPlot plot;
        plot.resize(400, 300);
        plot.setBackground(Qt::white);

        m_theme->setMode(Theme::Mode::Dark);
        Theme::instance()->applyToPlot(&plot);
        plot.replot();

        const QColor darkBg = Theme::plotColorsFor(Theme::Mode::Dark).background;
        const int hits = countColor(&plot, darkBg);
        const QRect area = plot.axisRect()->rect();
        QVERIFY2(hits > area.width() * area.height() / 4,
                 qPrintable(QStringLiteral("深色背景只占了 %1 像素").arg(hits)));
    }

    void test_spectrum_plot_follows_theme()
    {
        SpectrumViewWidget widget;
        widget.resize(800, 600);

        QVector<double> x(100), y(100);
        for (int i = 0; i < 100; ++i) {
            x[i] = i;
            y[i] = i * 1.0;
        }
        widget.setData(x, y);

        auto *plot = widget.findChild<QCustomPlot *>();
        QVERIFY(plot);

        m_theme->setMode(Theme::Mode::Dark);
        const Theme::PlotColors dark = Theme::plotColorsFor(Theme::Mode::Dark);

        // 四条轴都要换色，只换左下两条的话图框还是有半边是黑的
        const QList<QCPAxis *> axes = plot->axisRect()->axes();
        QCOMPARE(axes.size(), 4);
        for (QCPAxis *axis : axes) {
            QCOMPARE(axis->basePen().color(), dark.axis);
            QCOMPARE(axis->tickLabelColor(), dark.text);
            QCOMPARE(axis->labelColor(), dark.text);
            QCOMPARE(axis->grid()->pen().color(), dark.grid);
        }
    }

    void test_spectrum_curve_color_follows_theme()
    {
        SpectrumViewWidget widget;
        widget.resize(800, 600);

        QVector<double> x(100), y(100);
        for (int i = 0; i < 100; ++i) {
            x[i] = i;
            y[i] = i * 1.0;
        }
        widget.setData(x, y);

        auto *plot = widget.findChild<QCustomPlot *>();
        QVERIFY(plot);
        QCPGraph *graph = plot->graph(0);
        QVERIFY(graph);

        m_theme->setMode(Theme::Mode::Light);
        QCOMPARE(graph->pen().color(), QColor(Qt::black));

        m_theme->setMode(Theme::Mode::Dark);
        QCOMPARE(graph->pen().color(), Theme::plotColorsFor(Theme::Mode::Dark).curve);
        QVERIFY(graph->pen().color() != QColor(Qt::black));
    }

    void test_spectrum_cursor_label_follows_theme()
    {
        // 读数框是白色实底，不跟着换的话深色下就是一块白斑
        SpectrumViewWidget widget;
        widget.resize(800, 600);

        m_theme->setMode(Theme::Mode::Dark);
        const Theme::PlotColors dark = Theme::plotColorsFor(Theme::Mode::Dark);

        auto *plot = widget.findChild<QCustomPlot *>();
        QVERIFY(plot);
        QCPItemText *label = nullptr;
        for (int i = 0; i < plot->itemCount(); ++i) {
            if (auto *text = dynamic_cast<QCPItemText *>(plot->item(i))) {
                label = text;
            }
        }
        QVERIFY(label);
        QCOMPARE(label->brush().color(), dark.overlayBackground);
        QCOMPARE(label->pen().color(), dark.overlayText);
    }

    void test_image_colorbar_axis_is_themed()
    {
        // 色标的刻度数字画在 QCPColorScale 自己的内部轴上，不在 plot->axisRect() 里，
        // 漏了它就是深色下的黑底黑字
        m_theme->setMode(Theme::Mode::Dark);

        ImageViewWidget widget;
        widget.resize(600, 400);
        QImage img(64, 48, QImage::Format_Grayscale8);
        img.fill(120);
        widget.setImage(img);
        widget.setColorScaleVisible(true);

        auto *scale = widget.findChild<QCPColorScale *>();
        QVERIFY(scale);
        QVERIFY(scale->axis());

        const Theme::PlotColors dark = Theme::plotColorsFor(Theme::Mode::Dark);
        QCOMPARE(scale->axis()->tickLabelColor(), dark.text);
        QCOMPARE(scale->axis()->basePen().color(), dark.axis);
    }

    void test_image_plot_background_is_themed()
    {
        // 图像本身占了满屏，背景要单独确认，否则图外沿一圈还是白的
        m_theme->setMode(Theme::Mode::Dark);

        ImageViewWidget widget;
        widget.resize(400, 300);
        widget.show();
        QCoreApplication::processEvents();

        auto *plot = widget.findChild<QCustomPlot *>();
        QVERIFY(plot);
        QVERIFY2(countColor(plot, Theme::plotColorsFor(Theme::Mode::Dark).background) > 0,
                 "深色主题下图像视图的背景一个像素都没换");
    }

    void test_theme_survives_widget_recreation()
    {
        // Theme 是单例，先设深色再建控件，控件必须一出生就是深色
        m_theme->setMode(Theme::Mode::Dark);
        SpectrumViewWidget widget;
        widget.resize(400, 300);

        auto *plot = widget.findChild<QCustomPlot *>();
        QVERIFY(plot);
        widget.show();
        QCoreApplication::processEvents();

        const QColor darkBg = Theme::plotColorsFor(Theme::Mode::Dark).background;
        QVERIFY2(countColor(plot, darkBg) > 0, "深色主题下绘图区一个背景像素都没有");
    }

private:
    Theme *m_theme = nullptr;
};

QTEST_MAIN(TestTheme)
#include "test_theme.moc"
