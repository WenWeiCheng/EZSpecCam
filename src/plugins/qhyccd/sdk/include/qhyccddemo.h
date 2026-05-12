#include <QImage>
#include "qhyccd.h"
#include "livemodethread.h"
#include "camstruct.h"

#ifndef QHYCCDDEMO_H
#define QHYCCDDEMO_H

#include <QMainWindow>

//using namespace cv;

namespace Ui {
class QHYCCDDemo;
}

class QHYCCDDemo : public QMainWindow
{
    Q_OBJECT

//protected:
//    void wheelEvent(QWheelEvent *event);//{printf("*********************************************\n");}

public:
    explicit QHYCCDDemo(QWidget *parent = 0);
    ~QHYCCDDemo();

    uint32_t ret = QHYCCD_ERROR;
    bool callFlag;

    LiveModeThread *liveThread;

private:
    Ui::QHYCCDDemo *ui;

    void closeEvent(QCloseEvent *event);

private slots:
    void on_pBtn_ScanCamera_clicked();
    void on_comboBox_CameraList_currentTextChanged(const QString &arg1);
    void on_pBtn_Connect_clicked();
    void on_pBtn_Disconnect_clicked();

    void processImageData();
    void showFPS();
    void showFrame();
    void on_pBtn_Capture_clicked();
    void on_pBtn_Stop_clicked();
    void on_comboBox_Format_currentTextChanged(const QString &arg1);
    void on_comboBox_Bin_currentTextChanged(const QString &arg1);
    void on_spinBox_Exposure_valueChanged(int arg1);
    void on_comboBox_Unit_currentTextChanged(const QString &arg1);
    void on_hSlider_Exposure_valueChanged(int value);
    void on_hSlider_Gain_valueChanged(int value);
    void on_spinBox_Gain_valueChanged(int arg1);
    void on_hSlider_Offset_valueChanged(int value);
    void on_spinBox_Offset_valueChanged(int arg1);
    void on_hSlider_Traffic_valueChanged(int value);
    void on_spinBox_Traffic_valueChanged(int arg1);
};

#endif // QHYCCDDEMO_H
