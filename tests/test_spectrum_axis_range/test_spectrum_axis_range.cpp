#include <QCoreApplication>
#include <QObject>
#include <QTest>
#include <QVector>
#include "SpectrumViewWidget.h"

class TestSpectrumAxisRange : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() {}
    void cleanupTestCase() {}
    void init() {}
    void cleanup() {}

    void test_default_modes_are_auto()
    {
        SpectrumViewWidget widget;
        widget.resize(800, 600);

        QVector<double> x(100), y(100);
        for (int i = 0; i < 100; ++i) {
            x[i] = i;
            y[i] = i * 10.0;
        }
        widget.setData(x, y);

        QCOMPARE(widget.xAxisRangeMode(), SpectrumViewWidget::AxisRangeMode::Auto);
        QCOMPARE(widget.yAxisRangeMode(), SpectrumViewWidget::AxisRangeMode::Auto);

        QCOMPARE(widget.currentXMin(), -2.0);
        QCOMPARE(widget.currentXMax(), 102.0);
        QCOMPARE(widget.currentYMin(), -2.0);
        QCOMPARE(widget.currentYMax(), 1002.0);
    }

    void test_manual_x_range_applies()
    {
        SpectrumViewWidget widget;
        widget.resize(800, 600);

        QVector<double> x(100), y(100);
        for (int i = 0; i < 100; ++i) {
            x[i] = i;
            y[i] = i;
        }
        widget.setData(x, y);

        widget.setXAxisRangeMode(SpectrumViewWidget::AxisRangeMode::Manual);
        widget.setManualXRange(10.0, 50.0);

        QCOMPARE(widget.currentXMin(), 10.0);
        QCOMPARE(widget.currentXMax(), 50.0);
        QCOMPARE(widget.currentYMin(), -2.0);
        QCOMPARE(widget.currentYMax(), 102.0);
    }

    void test_manual_y_range_applies()
    {
        SpectrumViewWidget widget;
        widget.resize(800, 600);

        QVector<double> x(100), y(100);
        for (int i = 0; i < 100; ++i) {
            x[i] = i;
            y[i] = i;
        }
        widget.setData(x, y);

        widget.setYAxisRangeMode(SpectrumViewWidget::AxisRangeMode::Manual);
        widget.setManualYRange(0.0, 1000.0);

        QCOMPARE(widget.currentYMin(), 0.0);
        QCOMPARE(widget.currentYMax(), 1000.0);
        QCOMPARE(widget.currentXMin(), -2.0);
        QCOMPARE(widget.currentXMax(), 102.0);
    }

    void test_right_click_resets_manual()
    {
        SpectrumViewWidget widget;
        widget.resize(800, 600);

        QVector<double> x(100), y(100);
        for (int i = 0; i < 100; ++i) {
            x[i] = i;
            y[i] = i;
        }
        widget.setData(x, y);

        widget.setXAxisRangeMode(SpectrumViewWidget::AxisRangeMode::Manual);
        widget.setYAxisRangeMode(SpectrumViewWidget::AxisRangeMode::Manual);
        widget.setManualXRange(20.0, 80.0);
        widget.setManualYRange(-5.0, 50.0);

        widget.setZoomedForTest(true);
        widget.resetZoom();

        QCOMPARE(widget.currentXMin(), 20.0);
        QCOMPARE(widget.currentXMax(), 80.0);
        QCOMPARE(widget.currentYMin(), -5.0);
        QCOMPARE(widget.currentYMax(), 50.0);
    }

    void test_right_click_resets_auto_to_data_bounds()
    {
        SpectrumViewWidget widget;
        widget.resize(800, 600);

        QVector<double> x(100), y(100);
        for (int i = 0; i < 100; ++i) {
            x[i] = i;
            y[i] = i;
        }
        widget.setData(x, y);

        widget.setManualXRange(20.0, 80.0);
        widget.setManualYRange(-5.0, 50.0);
        widget.setXAxisRangeMode(SpectrumViewWidget::AxisRangeMode::Manual);
        widget.setYAxisRangeMode(SpectrumViewWidget::AxisRangeMode::Manual);

        widget.setZoomedForTest(true);

        widget.setXAxisRangeMode(SpectrumViewWidget::AxisRangeMode::Auto);
        widget.setYAxisRangeMode(SpectrumViewWidget::AxisRangeMode::Auto);

        widget.resetZoom();

        QCOMPARE(widget.currentXMin(), -2.0);
        QCOMPARE(widget.currentXMax(), 102.0);
        QCOMPARE(widget.currentYMin(), -2.0);
        QCOMPARE(widget.currentYMax(), 102.0);
    }

    void test_right_click_mixed_modes()
    {
        SpectrumViewWidget widget;
        widget.resize(800, 600);

        QVector<double> x(100), y(100);
        for (int i = 0; i < 100; ++i) {
            x[i] = i;
            y[i] = i;
        }
        widget.setData(x, y);

        widget.setXAxisRangeMode(SpectrumViewWidget::AxisRangeMode::Manual);
        widget.setYAxisRangeMode(SpectrumViewWidget::AxisRangeMode::Auto);
        widget.setManualXRange(10.0, 90.0);

        widget.setZoomedForTest(true);
        widget.resetZoom();

        QCOMPARE(widget.currentXMin(), 10.0);
        QCOMPARE(widget.currentXMax(), 90.0);
        QCOMPARE(widget.currentYMin(), -2.0);
        QCOMPARE(widget.currentYMax(), 102.0);
    }

    void test_new_data_preserves_zoom()
    {
        SpectrumViewWidget widget;
        widget.resize(800, 600);

        QVector<double> x(100), y(100);
        for (int i = 0; i < 100; ++i) {
            x[i] = i;
            y[i] = i;
        }
        widget.setData(x, y);

        widget.setZoomedForTest(true);
        widget.setManualXRange(20.0, 60.0);
        widget.setManualYRange(10.0, 70.0);

        QVector<double> y2(100);
        for (int i = 0; i < 100; ++i) {
            y2[i] = (i + 500) * 1.0;
        }
        widget.setData(x, y2);

        QCOMPARE(widget.currentXMin(), 20.0);
        QCOMPARE(widget.currentXMax(), 60.0);
        QCOMPARE(widget.currentYMin(), 10.0);
        QCOMPARE(widget.currentYMax(), 70.0);
    }

    void test_new_data_rescales_when_not_zoomed()
    {
        SpectrumViewWidget widget;
        widget.resize(800, 600);

        QVector<double> x(100), y(100);
        for (int i = 0; i < 100; ++i) {
            x[i] = i;
            y[i] = i;
        }
        widget.setData(x, y);

        QVector<double> y2(100);
        for (int i = 0; i < 100; ++i) {
            y2[i] = (i + 500) * 1.0;
        }
        widget.setData(x, y2);

        QCOMPARE(widget.currentYMin(), 498.0);
        QCOMPARE(widget.currentYMax(), 602.0);
    }

    void test_axis_modes_independent()
    {
        SpectrumViewWidget widget;
        widget.resize(800, 600);

        QVector<double> x(10), y(10);
        for (int i = 0; i < 10; ++i) {
            x[i] = i;
            y[i] = i;
        }
        widget.setData(x, y);

        widget.setXAxisRangeMode(SpectrumViewWidget::AxisRangeMode::Manual);
        QCOMPARE(widget.yAxisRangeMode(), SpectrumViewWidget::AxisRangeMode::Auto);

        widget.setYAxisRangeMode(SpectrumViewWidget::AxisRangeMode::Manual);
        QCOMPARE(widget.xAxisRangeMode(), SpectrumViewWidget::AxisRangeMode::Manual);

        widget.setXAxisRangeMode(SpectrumViewWidget::AxisRangeMode::Auto);
        QCOMPARE(widget.yAxisRangeMode(), SpectrumViewWidget::AxisRangeMode::Manual);
    }
};

QTEST_MAIN(TestSpectrumAxisRange)
#include "test_spectrum_axis_range.moc"