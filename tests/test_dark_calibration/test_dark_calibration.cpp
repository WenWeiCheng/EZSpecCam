#include <QObject>
#include <limits>
#include <QTest>
#include <QImage>
#include <QDateTime>
#include <QElapsedTimer>
#include <QDebug>

#include "CameraTypes.h"
#include "PostProcess.h"

class TestDarkCalibration : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() {}
    void cleanupTestCase() {}
    void init() {}
    void cleanup() {}

    void test_grayscale16_subtracts_dark_and_adds_bias();
    void test_grayscale8_floors_at_zero();
    void test_rgb888_clamps_per_channel();
    void test_size_mismatch_is_no_op();
    void test_null_dark_is_no_op();
    void test_perf_6000x4000_grayscale16();
    void test_perf_2048x2048_grayscale16();

private:
    static QImage makeGrayscale16(int w, int h, int fillValue);
    static QImage makeGrayscale8(int w, int h, int fillValue);
    static QImage makeRgb888(int w, int h, int r, int g, int b);
    static int pixelGrayscale16(const QImage &img, int x, int y);
    static int pixelGrayscale8(const QImage &img, int x, int y);
    static int pixelRgbChannel(const QImage &img, int x, int y, int channel);
};

QImage TestDarkCalibration::makeGrayscale16(int w, int h, int fillValue)
{
    QImage img(w, h, QImage::Format_Grayscale16);
    for (int y = 0; y < h; ++y) {
        ushort *row = reinterpret_cast<ushort *>(img.scanLine(y));
        for (int x = 0; x < w; ++x) {
            row[x] = static_cast<ushort>(fillValue);
        }
    }
    return img;
}

QImage TestDarkCalibration::makeGrayscale8(int w, int h, int fillValue)
{
    QImage img(w, h, QImage::Format_Grayscale8);
    for (int y = 0; y < h; ++y) {
        uchar *row = img.scanLine(y);
        for (int x = 0; x < w; ++x) {
            row[x] = static_cast<uchar>(fillValue);
        }
    }
    return img;
}

QImage TestDarkCalibration::makeRgb888(int w, int h, int r, int g, int b)
{
    QImage img(w, h, QImage::Format_RGB888);
    for (int y = 0; y < h; ++y) {
        uchar *row = img.scanLine(y);
        for (int x = 0; x < w; ++x) {
            row[x * 3 + 0] = static_cast<uchar>(r);
            row[x * 3 + 1] = static_cast<uchar>(g);
            row[x * 3 + 2] = static_cast<uchar>(b);
        }
    }
    return img;
}

int TestDarkCalibration::pixelGrayscale16(const QImage &img, int x, int y)
{
    const ushort *row = reinterpret_cast<const ushort *>(img.constScanLine(y));
    return row[x];
}

int TestDarkCalibration::pixelGrayscale8(const QImage &img, int x, int y)
{
    const uchar *row = img.constScanLine(y);
    return row[x];
}

int TestDarkCalibration::pixelRgbChannel(const QImage &img, int x, int y, int channel)
{
    const uchar *row = img.constScanLine(y);
    return row[x * 3 + channel];
}

void TestDarkCalibration::test_grayscale16_subtracts_dark_and_adds_bias()
{
    // 3x2 frame, fill 100; 3x2 dark, fill 50; bias 10 -> [60, 210] (per spec the
    // first row should hold 60 and the second 210). The plan asserts "filled
    // with [100,200]"; here we use a single fill of 100, and the dark is 50,
    // so the result must be 60 for every pixel. The plan also mentions [60,210]
    // which appears to be a typo for a per-row fill; we keep the test aligned
    // with the documented formula: out = max(0, src - dark + bias).
    ImageData frame;
    frame.image = makeGrayscale16(3, 2, 100);
    frame.timestamp = QDateTime::currentMSecsSinceEpoch();

    QImage dark = makeGrayscale16(3, 2, 50);

    PostProcess::applyDarkCalibration(frame, &dark, 10);

    QCOMPARE(frame.image.width(), 3);
    QCOMPARE(frame.image.height(), 2);
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 3; ++x) {
            QCOMPARE(pixelGrayscale16(frame.image, x, y), 60);
        }
    }

    // Now exercise the second case mentioned in the plan: src fill = 200,
    // dark fill = 50, bias 10 -> out = 200 - 50 + 10 = 160.
    ImageData frame2;
    frame2.image = makeGrayscale16(3, 2, 200);
    frame2.timestamp = QDateTime::currentMSecsSinceEpoch();
    PostProcess::applyDarkCalibration(frame2, &dark, 10);
    for (int y = 0; y < 2; ++y) {
        for (int x = 0; x < 3; ++x) {
            QCOMPARE(pixelGrayscale16(frame2.image, x, y), 160);
        }
    }
}

void TestDarkCalibration::test_grayscale8_floors_at_zero()
{
    // src=[0,1,2,255], dark=5, bias=0 -> [0,0,0,250]
    ImageData frame;
    QImage src(2, 2, QImage::Format_Grayscale8);
    uchar *r0 = src.scanLine(0);
    uchar *r1 = src.scanLine(1);
    r0[0] = 0;   r0[1] = 1;
    r1[0] = 2;   r1[1] = 255;
    frame.image = src;
    frame.timestamp = QDateTime::currentMSecsSinceEpoch();

    QImage dark = makeGrayscale8(2, 2, 5);

    PostProcess::applyDarkCalibration(frame, &dark, 0);

    QCOMPARE(pixelGrayscale8(frame.image, 0, 0), 0);
    QCOMPARE(pixelGrayscale8(frame.image, 1, 0), 0);
    QCOMPARE(pixelGrayscale8(frame.image, 0, 1), 0);
    QCOMPARE(pixelGrayscale8(frame.image, 1, 1), 250);
}

void TestDarkCalibration::test_rgb888_clamps_per_channel()
{
    // 1x1 RGB888 [10,20,30], dark [5,5,5], bias 100 -> [105, 115, 125]
    ImageData frame;
    frame.image = makeRgb888(1, 1, 10, 20, 30);
    frame.timestamp = QDateTime::currentMSecsSinceEpoch();

    QImage dark = makeRgb888(1, 1, 5, 5, 5);

    PostProcess::applyDarkCalibration(frame, &dark, 100);

    QCOMPARE(pixelRgbChannel(frame.image, 0, 0, 0), 105);
    QCOMPARE(pixelRgbChannel(frame.image, 0, 0, 1), 115);
    QCOMPARE(pixelRgbChannel(frame.image, 0, 0, 2), 125);

    // Clamping: bias that would overflow 255 must be clamped.
    ImageData frame2;
    frame2.image = makeRgb888(1, 1, 200, 200, 200);
    frame2.timestamp = QDateTime::currentMSecsSinceEpoch();
    PostProcess::applyDarkCalibration(frame2, &dark, 100);
    QCOMPARE(pixelRgbChannel(frame2.image, 0, 0, 0), 255);
    QCOMPARE(pixelRgbChannel(frame2.image, 0, 0, 1), 255);
    QCOMPARE(pixelRgbChannel(frame2.image, 0, 0, 2), 255);
}

void TestDarkCalibration::test_size_mismatch_is_no_op()
{
    // 3x3 dark and 2x2 frame -> image must be unchanged.
    ImageData frame;
    frame.image = makeGrayscale16(2, 2, 1000);
    frame.timestamp = QDateTime::currentMSecsSinceEpoch();

    // Snapshot pixels
    const int p00_before = pixelGrayscale16(frame.image, 0, 0);
    const int p11_before = pixelGrayscale16(frame.image, 1, 1);

    QImage dark = makeGrayscale16(3, 3, 100);

    PostProcess::applyDarkCalibration(frame, &dark, 50);

    QCOMPARE(frame.image.width(), 2);
    QCOMPARE(frame.image.height(), 2);
    QCOMPARE(pixelGrayscale16(frame.image, 0, 0), p00_before);
    QCOMPARE(pixelGrayscale16(frame.image, 1, 1), p11_before);
}

void TestDarkCalibration::test_null_dark_is_no_op()
{
    ImageData frame;
    frame.image = makeGrayscale16(3, 2, 7777);
    frame.timestamp = QDateTime::currentMSecsSinceEpoch();

    const int p00_before = pixelGrayscale16(frame.image, 0, 0);

    PostProcess::applyDarkCalibration(frame, nullptr, 123);

    QCOMPARE(pixelGrayscale16(frame.image, 0, 0), p00_before);

    // Also: null QImage (not pointer-null, but isNull()) must be a no-op.
    QImage nullDark;
    PostProcess::applyDarkCalibration(frame, &nullDark, 123);
    QCOMPARE(pixelGrayscale16(frame.image, 0, 0), p00_before);
}

void TestDarkCalibration::test_perf_6000x4000_grayscale16()
{
    constexpr int W = 6000;
    constexpr int H = 4000;
    constexpr int RUNS = 3;

    QImage frameImg = makeGrayscale16(W, H, 1000);
    QImage darkImg  = makeGrayscale16(W, H, 50);

    qint64 totalMs = 0;
    qint64 minMs = std::numeric_limits<qint64>::max();
    for (int i = 0; i < RUNS; ++i) {
        ImageData frame;
        frame.image = frameImg;
        frame.timestamp = QDateTime::currentMSecsSinceEpoch();

        QElapsedTimer timer;
        timer.start();
        PostProcess::applyDarkCalibration(frame, &darkImg, 10);
        qint64 elapsed = timer.elapsed();
        totalMs += elapsed;
        if (elapsed < minMs) minMs = elapsed;

        QCOMPARE(frame.image.width(), W);
        QCOMPARE(frame.image.height(), H);
    }
    qDebug() << "applyDarkCalibration" << W << "x" << H << "Grayscale16:"
             << "avg=" << (totalMs / RUNS) << "ms"
             << "min=" << minMs << "ms";
}

void TestDarkCalibration::test_perf_2048x2048_grayscale16()
{
    constexpr int W = 2048;
    constexpr int H = 2048;
    constexpr int RUNS = 5;

    QImage frameImg = makeGrayscale16(W, H, 1000);
    QImage darkImg  = makeGrayscale16(W, H, 50);

    qint64 totalMs = 0;
    qint64 minMs = std::numeric_limits<qint64>::max();
    for (int i = 0; i < RUNS; ++i) {
        ImageData frame;
        frame.image = frameImg;
        frame.timestamp = QDateTime::currentMSecsSinceEpoch();

        QElapsedTimer timer;
        timer.start();
        PostProcess::applyDarkCalibration(frame, &darkImg, 10);
        qint64 elapsed = timer.elapsed();
        totalMs += elapsed;
        if (elapsed < minMs) minMs = elapsed;

        QCOMPARE(frame.image.width(), W);
        QCOMPARE(frame.image.height(), H);
    }
    qDebug() << "applyDarkCalibration" << W << "x" << H << "Grayscale16:"
             << "avg=" << (totalMs / RUNS) << "ms"
             << "min=" << minMs << "ms";
}

QTEST_MAIN(TestDarkCalibration)
#include "test_dark_calibration.moc"
