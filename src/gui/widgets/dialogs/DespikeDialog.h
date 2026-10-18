#ifndef DESPIKEDIALOG_H
#define DESPIKEDIALOG_H

#include <QDialog>

#include "../PostProcess.h"

class QDoubleSpinBox;
class QSpinBox;

//! 光谱去尖峰的参数窗口。
//!
//! 只暴露三个参数：主阈值、次阈值、均值窗口半宽。改动即时生效，窗口上没有
//! OK/Cancel —— 去尖峰看的是谱线形状，调参数时得一边调一边看效果
class DespikeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DespikeDialog(QWidget *parent = nullptr);
    ~DespikeDialog() override;

    PostProcess::DespikeParams despikeParams() const;
    //! 写回控件，不触发 paramsChanged（和 ScaleControlDialog 的 setter 一致）
    void setDespikeParams(const PostProcess::DespikeParams &params);

    double primaryThreshold() const;
    double secondaryThreshold() const;
    int windowHalfWidth() const;

signals:
    //! 三个参数一起发，外层只需重算一次，不会出现「阈值已改、窗口还是旧的」的中间态
    void paramsChanged(double primaryThreshold, double secondaryThreshold, int windowHalfWidth);

private:
    void setupUi();

    QDoubleSpinBox *m_primarySpin = nullptr;
    QDoubleSpinBox *m_secondarySpin = nullptr;
    QSpinBox *m_windowSpin = nullptr;
};

#endif // DESPIKEDIALOG_H
