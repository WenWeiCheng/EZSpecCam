/**
 * @file CustomRangeDialog.h
 * @brief Custom X-Axis Range Dialog for EZSpecCam Application
 *
 * Dialog for setting custom X-axis range values with min/max spin boxes.
 * Validates that min < max before accepting.
 */

#ifndef CUSTOMRANGEDIALOG_H
#define CUSTOMRANGEDIALOG_H

#include <QDialog>
#include <QDoubleSpinBox>
#include <QPushButton>
#include <QLabel>
#include <QHBoxLayout>
#include <QVBoxLayout>

/**
 * @class CustomRangeDialog
 * @brief Dialog for setting custom X-axis range values
 *
 * This dialog provides a single window with two spin boxes for entering
 * minimum and maximum values, with validation to ensure the range is valid.
 *
 * @section usage Usage
 * @code
 * CustomRangeDialog dialog(parent);
 * dialog.setRange(-1000000, 1000000);  // Set valid range
 * dialog.setValues(0, 100);            // Set default values
 * if (dialog.exec() == QDialog::Accepted) {
 *     double minVal = dialog.minValue();
 *     double maxVal = dialog.maxValue();
 *     // Apply values...
 * }
 * @endcode
 */
class CustomRangeDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CustomRangeDialog(QWidget *parent = nullptr);
    ~CustomRangeDialog() override;

    void setRange(double min, double max);
    void setValues(double min, double max);
    void setImageWidth(double width);
    double minValue() const;
    double maxValue() const;

private slots:
    void onOkClicked();

private:
    QDoubleSpinBox *m_minSpinBox;
    QDoubleSpinBox *m_maxSpinBox;
    double m_imageWidth;
    QPushButton *m_okButton;
    QPushButton *m_cancelButton;
};

#endif // CUSTOMRANGEDIALOG_H