#ifndef DISPLAYSTYLEDIALOG_H
#define DISPLAYSTYLEDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QCheckBox>
#include <QGroupBox>

class DisplayStyleDialog : public QDialog
{
    Q_OBJECT

public:
    explicit DisplayStyleDialog(QWidget *parent = nullptr);
    ~DisplayStyleDialog() override;

    void setColorScaleVisible(bool visible);
    void setImageColorMap(int map);
    void setSpectrumLineStyle(int style);

    bool isColorScaleVisible() const;
    int imageColorMap() const;
    int spectrumLineStyle() const;

signals:
    void colorScaleToggled(bool visible);
    void imageColorMapChanged(int map);
    void spectrumLineStyleChanged(int style);

private slots:
    void onColorScaleToggled(bool checked);
    void onImageColorMapChanged(int index);
    void onSpectrumLineStyleChanged(int index);

private:
    void createImageGroup();
    void createSpectrumGroup();

    QGroupBox *m_imageGroup;
    QGroupBox *m_spectrumGroup;

    QCheckBox *m_showColorScaleCheck;
    QComboBox *m_imageColorMapCombo;
    QComboBox *m_spectrumLineStyleCombo;
};

#endif
