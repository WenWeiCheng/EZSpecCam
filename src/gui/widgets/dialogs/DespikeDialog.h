#ifndef DESPIKEDIALOG_H
#define DESPIKEDIALOG_H

#include <QDialog>

#include "../PostProcess.h"

class QDoubleSpinBox;

//! 去尖峰的参数窗口。
//!
//! 现在只有一个参数：主阈值。改动即时生效，窗口上没有 OK/Cancel —— 去尖峰
//! 看的是画面，调参数时得一边调一边看效果。生效的行范围跟着 Process →
//! Row Range 走，不在这里重复设置。
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

signals:
    void paramsChanged();

private:
    void setupUi();

    QDoubleSpinBox *m_primarySpin = nullptr;
};

#endif // DESPIKEDIALOG_H
