#ifndef SCALECONTROLDIALOG_H
#define SCALECONTROLDIALOG_H

#include <QDialog>
#include <QComboBox>
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

    int imageScaleType() const;
    int imageColorScaleMode() const;
    int spectrumScaleType() const;

signals:
    void imageScaleTypeChanged(int type);
    void imageColorScaleModeChanged(int mode);
    void spectrumScaleTypeChanged(int type);

private slots:
    void onImageScaleTypeChanged(int index);
    void onImageColorScaleModeChanged(int index);
    void onSpectrumScaleTypeChanged(int index);

private:
    void createImageGroup();
    void createSpectrumGroup();

    QGroupBox *m_imageGroup;
    QGroupBox *m_spectrumGroup;

    QComboBox *m_imageScaleTypeCombo;
    QComboBox *m_imageColorScaleModeCombo;
    QComboBox *m_spectrumScaleTypeCombo;
};

#endif