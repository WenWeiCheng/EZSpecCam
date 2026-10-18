#ifndef RORRANGEDIALOG_H
#define RORRANGEDIALOG_H

#include <QDialog>
#include <QSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QFormLayout>

class RowRangeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit RowRangeDialog(QWidget *parent = nullptr);
    ~RowRangeDialog() override;

    void setImageHeight(int height);
    //! 0-based 闭区间，和 Profile 窗口里的行号一致
    void setRange(int start, int end);

    //! 对话框显示 0-based 行索引，verticalBinning/metadata 用 1-based。
    //! 边界转换集中在这里，避免两边各写一次 ±1
    static int toBinningRow(int row) { return row + 1; }
    static int fromBinningRow(int row) { return row - 1; }

    int startRow() const;
    int endRow() const;

signals:
    void applyClicked(int startRow, int endRow);

private slots:
    void onOkClicked();
    void onApplyClicked();
    void onHeightChanged(int height);

private:
    QSpinBox *m_startSpinBox;
    QSpinBox *m_endSpinBox;
    QPushButton *m_okButton;
    QPushButton *m_applyButton;
    QPushButton *m_cancelButton;
    int m_imageHeight;
};

#endif // RORRANGEDIALOG_H