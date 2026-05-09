#ifndef PROFILEWINDOW_H
#define PROFILEWINDOW_H

#include <QDialog>
#include <QVector>
#include <QLabel>

class SpectrumViewWidget;

class ProfileWindow : public QDialog
{
    Q_OBJECT
public:
    explicit ProfileWindow(QWidget *parent = nullptr);
    ~ProfileWindow() override;

    void updateProfile(int x, int y,
                       const QVector<double> &horizontalData,
                       const QVector<double> &verticalData);

    void setImageSize(int width, int height);

private:
    void setupUi();

    SpectrumViewWidget *m_horizontalProfile;
    SpectrumViewWidget *m_verticalProfile;
    QLabel *m_horizontalLabel;
    QLabel *m_verticalLabel;
    int m_imageWidth;
    int m_imageHeight;
};

#endif // PROFILEWINDOW_H