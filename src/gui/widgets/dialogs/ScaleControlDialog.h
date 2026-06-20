#ifndef SCALECONTROLDIALOG_H
#define SCALECONTROLDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QGroupBox>
#include <QRadioButton>
#include <QButtonGroup>

class ScaleControlDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ScaleControlDialog(QWidget *parent = nullptr);
    ~ScaleControlDialog() override;

    void setImageScaleType(int type);
    void setImageColorScaleMode(int mode);
    void setSpectrumScaleType(int type);

    void setSpectrumXRangeMode(int mode);
    void setSpectrumYRangeMode(int mode);

    int spectrumXRangeMode() const;
    int spectrumYRangeMode() const;

    void setSpectrumManualXRange(double min, double max);
    void setSpectrumManualYRange(double min, double max);

    int imageScaleType() const;
    int imageColorScaleMode() const;
    int spectrumScaleType() const;

    double spectrumManualXMin() const;
    double spectrumManualXMax() const;
    double spectrumManualYMin() const;
    double spectrumManualYMax() const;

signals:
    void imageScaleTypeChanged(int type);
    void imageColorScaleModeChanged(int mode);
    void spectrumScaleTypeChanged(int type);
    void spectrumXRangeModeChanged(int mode);
    void spectrumYRangeModeChanged(int mode);
    void spectrumManualXRangeChanged(double min, double max);
    void spectrumManualYRangeChanged(double min, double max);

private slots:
    void onImageScaleTypeChanged(int index);
    void onImageColorScaleModeChanged(int index);
    void onSpectrumScaleTypeChanged(int index);
    void onSpectrumXRangeModeChanged(int index);
    void onSpectrumYRangeModeChanged(int index);
    void onSpectrumManualXMinChanged();
    void onSpectrumManualXMaxChanged();
    void onSpectrumManualYMinChanged();
    void onSpectrumManualYMaxChanged();

private:
    void createImageGroup();
    void createSpectrumGroup();
    void updateSpectrumManualXEnabled();
    void updateSpectrumManualYEnabled();

    QGroupBox *m_imageGroup;
    QGroupBox *m_spectrumGroup;

    QComboBox *m_imageScaleTypeCombo;
    QComboBox *m_imageColorScaleModeCombo;
    QComboBox *m_spectrumScaleTypeCombo;
    QComboBox *m_spectrumXRangeModeCombo;
    QComboBox *m_spectrumYRangeModeCombo;
    QDoubleSpinBox *m_spectrumManualXMinSpin;
    QDoubleSpinBox *m_spectrumManualXMaxSpin;
    QDoubleSpinBox *m_spectrumManualYMinSpin;
    QDoubleSpinBox *m_spectrumManualYMaxSpin;
};

#endif