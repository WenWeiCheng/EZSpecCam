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
    void setImageColorMap(int map);
    void setImageColorScaleMode(int mode);
    void setSpectrumScaleType(int type);
    void setSpectrumLineStyle(int style);

    int imageScaleType() const;
    int imageColorMap() const;
    int imageColorScaleMode() const;
    int spectrumScaleType() const;
    int spectrumLineStyle() const;

signals:
    void imageScaleTypeChanged(int type);
    void imageColorMapChanged(int map);
    void imageColorScaleModeChanged(int mode);
    void spectrumScaleTypeChanged(int type);
    void spectrumLineStyleChanged(int style);

private slots:
    void onImageScaleTypeChanged(int index);
    void onImageColorMapChanged(int index);
    void onImageColorScaleModeChanged(int index);
    void onSpectrumScaleTypeChanged(int index);
    void onSpectrumLineStyleChanged(int index);

private:
    void createImageGroup();
    void createSpectrumGroup();

    QGroupBox *m_imageGroup;
    QGroupBox *m_spectrumGroup;

    QComboBox *m_imageScaleTypeCombo;
    QComboBox *m_imageColorMapCombo;
    QComboBox *m_imageColorScaleModeCombo;
    QComboBox *m_spectrumScaleTypeCombo;
    QComboBox *m_spectrumLineStyleCombo;
};

#endif