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
    void setRange(int start, int end);

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