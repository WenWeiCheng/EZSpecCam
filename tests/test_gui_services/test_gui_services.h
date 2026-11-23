#ifndef TEST_GUI_SERVICES_H
#define TEST_GUI_SERVICES_H

#include <QObject>
#include <QTest>

class TestGuiServices : public QObject
{
    Q_OBJECT

private slots:
    void test_imageStatisticsFromGrayscale8();
    void test_imageStatisticsFromSpectrum();
    void test_histogramBinningAndOverexposure();
    void test_historyCsvExport();
    void test_fileNaming();
};

#endif // TEST_GUI_SERVICES_H
