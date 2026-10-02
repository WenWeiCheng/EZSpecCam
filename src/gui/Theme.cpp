#include "Theme.h"

#include <QApplication>
#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
#include <QStyleHints>
#endif

#include "qcustomplot.h"

namespace {

// 深色调色板。取的是偏中性的深灰，和常见的编辑器主题接近；
// 亮部留给文字，暗部留给底，正文与背景的对比度在 10:1 以上。
const QColor kDarkWindow(0x2b, 0x2d, 0x30);
const QColor kDarkBase(0x1e, 0x1f, 0x22);
const QColor kDarkAlternate(0x31, 0x33, 0x36);
const QColor kDarkText(0xe4, 0xe6, 0xe9);
const QColor kDarkDisabled(0x7a, 0x7d, 0x82);
const QColor kDarkHighlight(0x3d, 0x7e, 0xbd);

// 绘图区比 Base 再暗一点，让它读起来像一个「井」，和四周的控件面板分开。
const QColor kDarkPlotBackground(0x18, 0x19, 0x1b);
const QColor kDarkPlotAxis(0xb0, 0xb3, 0xb8);
const QColor kDarkPlotText(0xd4, 0xd7, 0xdb);
const QColor kDarkPlotGrid(0x45, 0x48, 0x4d);
const QColor kDarkPlotCurve(0xe8, 0xea, 0xed);
const QColor kDarkCrosshair(0xff, 0x6b, 0x6b, 180);

} // namespace

Theme::Theme(QObject *parent)
    : QObject(parent)
    , m_basePalette()
{
    // 这里只算颜色，不动 QApplication 的调色板 —— instance() 会被绘图控件在
    // 构造时调用，测试里只想取颜色，不该顺手把整个应用的观感改了
    m_basePalette = qApp ? qApp->palette() : QPalette();
    m_effectiveMode = detectSystemMode();
    m_plotColors = plotColorsFor(m_effectiveMode);
}

Theme *Theme::instance()
{
    static Theme theme;
    return &theme;
}

void Theme::initialize()
{
    // 先把系统原生的调色板存下来：切回浅色时要靠它还原
    m_basePalette = qApp ? qApp->palette() : QPalette();
    m_effectiveMode = (m_mode == Mode::System) ? detectSystemMode() : m_mode;
    m_plotColors = plotColorsFor(m_effectiveMode);

    if (qApp) {
        qApp->setPalette(m_effectiveMode == Mode::Dark ? darkPalette() : m_basePalette);
    }

    watchSystemChanges();
}

Theme::Mode Theme::detectSystemMode()
{
    if (!qApp) {
        return Mode::Light;
    }

#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    const Qt::ColorScheme scheme = QGuiApplication::styleHints()->colorScheme();
    if (scheme == Qt::ColorScheme::Dark) {
        return Mode::Dark;
    }
    if (scheme == Qt::ColorScheme::Light) {
        return Mode::Light;
    }
#endif

    // Qt 6.5 以下没有官方接口。平台主题（GTK/KDE）会把应用调色板换成深色的，
    // 所以用 Window 色的亮度兜底；给不出结论就当浅色，绝不擅自翻成深色。
    const QColor window = QGuiApplication::palette().color(QPalette::Window);
    return window.lightness() < 128 ? Mode::Dark : Mode::Light;
}

Theme::PlotColors Theme::plotColorsFor(Mode mode)
{
    PlotColors c;

    if (mode == Mode::Dark) {
        c.background = kDarkPlotBackground;
        c.axis = kDarkPlotAxis;
        c.text = kDarkPlotText;
        c.grid = kDarkPlotGrid;
        // 深色底上黑色曲线等于没有曲线，浅色下再翻回黑色
        c.curve = kDarkPlotCurve;
        c.crosshair = kDarkCrosshair;
        c.overlayText = kDarkText;
        c.overlayBackground = kDarkWindow;
        c.spinner = kDarkPlotAxis;
        return c;
    }

    c.background = Qt::white;
    c.axis = Qt::black;
    c.text = Qt::black;
    c.grid = QColor(200, 200, 200);
    c.curve = Qt::black;
    c.crosshair = QColor(255, 0, 0, 180);
    c.overlayText = Qt::black;
    c.overlayBackground = Qt::white;
    c.spinner = Qt::darkGray;
    return c;
}

QPalette Theme::darkPalette()
{
    QPalette p;

    p.setColor(QPalette::Window, kDarkWindow);
    p.setColor(QPalette::WindowText, kDarkText);
    p.setColor(QPalette::Base, kDarkBase);
    p.setColor(QPalette::AlternateBase, kDarkAlternate);
    p.setColor(QPalette::ToolTipBase, kDarkWindow);
    p.setColor(QPalette::ToolTipText, kDarkText);
    p.setColor(QPalette::Text, kDarkText);
    p.setColor(QPalette::Button, kDarkWindow);
    p.setColor(QPalette::ButtonText, kDarkText);
    p.setColor(QPalette::BrightText, Qt::red);
    p.setColor(QPalette::Link, kDarkHighlight);
    p.setColor(QPalette::Highlight, kDarkHighlight);
    p.setColor(QPalette::HighlightedText, Qt::white);
    p.setColor(QPalette::PlaceholderText, kDarkDisabled);

    // 色阶留给控件立体感用，比正文暗一档
    p.setColor(QPalette::Light, kDarkAlternate);
    p.setColor(QPalette::Midlight, kDarkWindow);
    p.setColor(QPalette::Mid, kDarkDisabled);
    p.setColor(QPalette::Dark, kDarkBase);
    p.setColor(QPalette::Shadow, Qt::black);

    // 禁用态：不能只是变淡，否则在深底上会和启用态糊在一起
    p.setColor(QPalette::Disabled, QPalette::WindowText, kDarkDisabled);
    p.setColor(QPalette::Disabled, QPalette::Text, kDarkDisabled);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, kDarkDisabled);
    p.setColor(QPalette::Disabled, QPalette::HighlightedText, kDarkDisabled);
    p.setColor(QPalette::Disabled, QPalette::Highlight, kDarkBase);

    return p;
}

void Theme::setMode(Mode mode)
{
    if (m_mode == mode) {
        return;
    }

    m_mode = mode;
    refresh();
}

void Theme::applyCommandLine(const QCommandLineParser &parser)
{
    if (!parser.isSet(QStringLiteral("theme"))) {
        return;
    }

    const QString value = parser.value(QStringLiteral("theme")).toLower();
    if (value == QStringLiteral("light")) {
        m_mode = Mode::Light;
    } else if (value == QStringLiteral("dark")) {
        m_mode = Mode::Dark;
    } else {
        m_mode = Mode::System;
    }
}

void Theme::watchSystemChanges()
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this, [this] {
        if (m_mode == Mode::System) {
            refresh();
        }
    });
#endif
}

void Theme::refresh()
{
    const Mode effective = (m_mode == Mode::System) ? detectSystemMode() : m_mode;
    if (effective == m_effectiveMode) {
        return;
    }

    m_effectiveMode = effective;
    m_plotColors = plotColorsFor(effective);

    if (qApp) {
        // 浅色时装回初始化时记下的那份，不能用 QPalette() 现取——
        // 那取到的是上一次的调色板，深色切回浅色就切不回去了
        qApp->setPalette(effective == Mode::Dark ? darkPalette() : m_basePalette);
    }

    emit themeChanged();
}

void Theme::applyToPlot(QCustomPlot *plot)
{
    if (!plot) {
        return;
    }

    const PlotColors &c = m_plotColors;

    plot->setBackground(QBrush(c.background));

    // 色标那一块是把 plotLayout 全清空后单独塞进去 QCPColorScale 的，
    // 压根没有 axisRect；这时只能刷背景，碰 axisRect() 会直接崩
    if (plot->axisRectCount() < 1) {
        return;
    }

    const QPen basePen(c.axis);
    const QPen tickPen(c.axis);
    // 网格保持 QCustomPlot 默认的虚线，只换颜色，免得浅色模式的观感变了
    const QPen gridPen(QPen(c.grid, 0, Qt::DotLine));
    const QPen subGridPen(QPen(c.grid.lighter(115), 0, Qt::DotLine));

    const QList<QCPAxis *> axes = plot->axisRect()->axes();
    for (QCPAxis *axis : axes) {
        axis->setBasePen(basePen);
        axis->setTickPen(tickPen);
        axis->setSubTickPen(tickPen);
        axis->setTickLabelColor(c.text);
        axis->setLabelColor(c.text);
        axis->grid()->setPen(gridPen);
        axis->grid()->setSubGridPen(subGridPen);
        axis->grid()->setZeroLinePen(QPen(c.grid.lighter(130), 0, Qt::SolidLine));
    }
}
