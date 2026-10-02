#ifndef THEME_H
#define THEME_H

#include <QColor>
#include <QObject>
#include <QPalette>

class QCommandLineParser;
class QCustomPlot;

// 界面的深浅配色。系统是深色就把整个 GUI（含 QCustomPlot 绘图）切成深色。
//
// 取色一律从这里拿，不要再往别处写死颜色 —— 尤其是 QCustomPlot：它完全不读
// QWidget 的调色板，背景、轴线、刻度文字、网格都得由绘图控件自己显式设色，
// 所以 PlotColors 单独把这几项拎出来，applyToPlot() 统一刷到绘图对象上。
class Theme : public QObject
{
    Q_OBJECT
public:
    enum class Mode {
        System, //!< 跟随系统设置
        Light,
        Dark
    };
    Q_ENUM(Mode)

    // 绘图区专用配色。QCustomPlot 不吃 QPalette，必须逐项显式设。
    struct PlotColors {
        QColor background;      //!< 绘图区背景
        QColor axis;            //!< 图框线与刻度线
        QColor text;            //!< 刻度标签与轴标题
        QColor grid;            //!< 网格线
        QColor curve;           //!< 数据曲线
        QColor crosshair;       //!< 图像十字线
        QColor overlayText;     //!< 悬浮读数的文字
        QColor overlayBackground; //!< 悬浮读数的底色
        QColor spinner;         //!< 加载指示器
    };

    //! 单例。首次调用时按当时的系统设置定下初始配色。
    static Theme *instance();

    //! 把配色刷到 QApplication 上并开始监听系统切换。只在 main() 里调一次；
    //! 单纯取色（instance()->plotColors()）不会动全局调色板。
    void initialize();

    Mode mode() const { return m_mode; }
    //! 切换配色并广播 themeChanged()。System 表示重新跟随系统。
    void setMode(Mode mode);

    //! 当前实际生效的是深色还是浅色（mode 为 System 时由系统决定）。
    bool isDark() const { return m_effectiveMode == Mode::Dark; }
    Mode effectiveMode() const { return m_effectiveMode; }

    const PlotColors &plotColors() const { return m_plotColors; }

    //! 解析 --theme 选项；没给就用 System。不认识的取值按 System 处理。
    void applyCommandLine(const QCommandLineParser &parser);

    //! 监听系统的深浅切换。Qt 6.5+ 有官方信号，更早的版本只能被动接受。
    void watchSystemChanges();

    static PlotColors plotColorsFor(Mode mode);
    //! 深色 QPalette。浅色沿用系统原生的调色板，因此不提供。
    static QPalette darkPalette();

    //! Qt 6.5+ 走 styleHints()->colorScheme()；更早的 Qt 没有这个 API，
    //! 只能从平台调色板的亮度去猜（GTK/KDE 的深色主题会给出深色调色板）。
    static Mode detectSystemMode();

    //! 把绘图区背景、四条轴、网格统一刷成当前主题。绘图控件都走这里，
    //! 免得三个绘图控件各写一份。
    void applyToPlot(QCustomPlot *plot);

signals:
    void themeChanged();

private:
    explicit Theme(QObject *parent = nullptr);

    void refresh();

    Mode m_mode = Mode::System;
    Mode m_effectiveMode = Mode::Light;
    PlotColors m_plotColors;
    //! 首次 setPalette 之前系统给的调色板；切回浅色时原样装回去
    QPalette m_basePalette;
};

#endif // THEME_H
