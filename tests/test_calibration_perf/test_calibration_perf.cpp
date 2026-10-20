// Performance measurements for the calibration module.
//
// These are benchmarks, not pass/fail gates: like test_postprocess_perf they
// only assert that the output kept its shape and print average/min/max timings.
// Run them in Release for representative numbers; Debug builds are several
// times slower because of unoptimized loops and QVector assertions.
#include <QElapsedTimer>
#include <QImage>
#include <QObject>
#include <QTest>
#include <QVector>
#include <QDebug>

#include "Calibration.h"
#include "CameraTypes.h"

class TestCalibrationPerf : public QObject
{
    Q_OBJECT

private slots:
    void test_dark_matched_6000x4000_g16();
    void test_dark_scaled_6000x4000_g16();
    void test_flat_apply_6000x4000_g16();
    void test_dark_and_flat_6000x4000_g16();
    void test_dark_and_flat_1392x1032_g16();
    void test_classic_flat_build_1392x1032_g16();
    void test_vff_build_1392x1032_g16();

private:
    static QImage makeGradient(int width, int height, int base, int span);
    static Calibration::ReferenceFrame reference(const QImage &image, double exposure,
                                                 int frameCount = 1);
    //! Cheap synthetic gain map; apply-only benchmarks should not pay the
    //! one-time build cost on every run.
    static Calibration::FlatField makeFlatField(int width, int height);
    static void report(const QString &name, const QVector<qint64> &timesUs);
};

QImage TestCalibrationPerf::makeGradient(int width, int height, int base, int span)
{
    QImage image(width, height, QImage::Format_Grayscale16);
    for (int y = 0; y < height; ++y) {
        ushort *row = reinterpret_cast<ushort *>(image.scanLine(y));
        for (int x = 0; x < width; ++x) {
            const int v = base + (span * x) / qMax(1, width - 1);
            row[x] = static_cast<ushort>(qBound(0, v, 65535));
        }
    }
    return image;
}

Calibration::ReferenceFrame TestCalibrationPerf::reference(const QImage &image,
                                                           double exposure,
                                                           int frameCount)
{
    Calibration::ReferenceFrame frame;
    frame.plane = Calibration::planeFromImage(image);
    frame.exposure = exposure;
    frame.frameCount = frameCount;
    return frame;
}

Calibration::FlatField TestCalibrationPerf::makeFlatField(int width, int height)
{
    Calibration::FlatField field;
    field.width = width;
    field.height = height;
    field.channels = 1;
    field.gain.resize(width * height);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            // A mild horizontal gain gradient around 1.0.
            field.gain[y * width + x] = 0.8f + 0.4f * static_cast<float>(x) / qMax(1, width - 1);
        }
    }
    field.meanSignal = 30000.0;
    field.minGain = 0.8f;
    field.maxGain = 1.2f;
    return field;
}

void TestCalibrationPerf::report(const QString &name, const QVector<qint64> &timesUs)
{
    if (timesUs.isEmpty()) {
        return;
    }
    qint64 sum = 0;
    qint64 min = timesUs.first();
    qint64 max = timesUs.first();
    for (qint64 t : timesUs) {
        sum += t;
        min = qMin(min, t);
        max = qMax(max, t);
    }
    const double avg = static_cast<double>(sum) / timesUs.size() / 1000.0;
    qInfo() << "==========================================";
    qInfo() << "Results for:" << name;
    qInfo() << "  Runs:" << timesUs.size();
    qInfo() << "  Average:" << avg << "ms";
    qInfo() << "  Min:" << min / 1000.0 << "ms";
    qInfo() << "  Max:" << max / 1000.0 << "ms";
}

void TestCalibrationPerf::test_dark_matched_6000x4000_g16()
{
    constexpr int W = 6000;
    constexpr int H = 4000;
    constexpr int RUNS = 2;

    const QImage source = makeGradient(W, H, 1000, 5000);
    const QImage darkImage = makeGradient(W, H, 900, 100);

    Calibration::DarkSettings dark;
    dark.enabled = true;
    dark.method = Calibration::DarkMethod::MatchedDark;
    dark.matchedDark = reference(darkImage, 1.0);
    dark.outputOffset = 1000;

    QVector<qint64> times;
    for (int i = 0; i < RUNS; ++i) {
        ImageData frame;
        frame.image = source;

        QElapsedTimer timer;
        timer.start();
        QString reason;
        const bool ok = Calibration::apply(frame, dark, nullptr, &reason);
        times.append(timer.nsecsElapsed() / 1000);
        QVERIFY2(ok, qPrintable(reason));
        QCOMPARE(frame.image.size(), source.size());
    }
    report("Calibration::apply dark matched 6000x4000 Grayscale16", times);
}

void TestCalibrationPerf::test_dark_scaled_6000x4000_g16()
{
    constexpr int W = 6000;
    constexpr int H = 4000;
    constexpr int RUNS = 2;

    const QImage source = makeGradient(W, H, 1000, 5000);
    const QImage biasImage = makeGradient(W, H, 900, 50);
    const QImage longDarkImage = makeGradient(W, H, 1900, 50);

    Calibration::DarkSettings dark;
    dark.enabled = true;
    dark.method = Calibration::DarkMethod::BiasScaled;
    dark.bias = reference(biasImage, 0.001);
    dark.longDark = reference(longDarkImage, 10.0);
    dark.longDarkExposure = 10.0;
    dark.targetExposure = 1.0;
    dark.outputOffset = 1000;

    QVector<qint64> times;
    for (int i = 0; i < RUNS; ++i) {
        ImageData frame;
        frame.image = source;

        QElapsedTimer timer;
        timer.start();
        QString reason;
        const bool ok = Calibration::apply(frame, dark, nullptr, &reason);
        times.append(timer.nsecsElapsed() / 1000);
        QVERIFY2(ok, qPrintable(reason));
        QCOMPARE(frame.image.size(), source.size());
    }
    report("Calibration::apply dark bias+scaled 6000x4000 Grayscale16", times);
}

void TestCalibrationPerf::test_flat_apply_6000x4000_g16()
{
    constexpr int W = 6000;
    constexpr int H = 4000;
    constexpr int RUNS = 2;

    const QImage source = makeGradient(W, H, 1000, 5000);
    const Calibration::FlatField field = makeFlatField(W, H);

    QVector<qint64> times;
    for (int i = 0; i < RUNS; ++i) {
        ImageData frame;
        frame.image = source;

        QElapsedTimer timer;
        timer.start();
        QString reason;
        const bool ok = Calibration::apply(frame, Calibration::DarkSettings{}, &field, &reason);
        times.append(timer.nsecsElapsed() / 1000);
        QVERIFY2(ok, qPrintable(reason));
        QCOMPARE(frame.image.size(), source.size());
    }
    report("Calibration::apply flat classic 6000x4000 Grayscale16", times);
}

void TestCalibrationPerf::test_dark_and_flat_6000x4000_g16()
{
    constexpr int W = 6000;
    constexpr int H = 4000;
    constexpr int RUNS = 2;

    const QImage source = makeGradient(W, H, 1000, 5000);
    const QImage darkImage = makeGradient(W, H, 900, 100);

    Calibration::DarkSettings dark;
    dark.enabled = true;
    dark.method = Calibration::DarkMethod::MatchedDark;
    dark.matchedDark = reference(darkImage, 1.0);
    dark.outputOffset = 1000;
    const Calibration::FlatField field = makeFlatField(W, H);

    QVector<qint64> times;
    for (int i = 0; i < RUNS; ++i) {
        ImageData frame;
        frame.image = source;

        QElapsedTimer timer;
        timer.start();
        QString reason;
        const bool ok = Calibration::apply(frame, dark, &field, &reason);
        times.append(timer.nsecsElapsed() / 1000);
        QVERIFY2(ok, qPrintable(reason));
        QCOMPARE(frame.image.size(), source.size());
    }
    report("Calibration::apply dark + flat 6000x4000 Grayscale16", times);
}

void TestCalibrationPerf::test_dark_and_flat_1392x1032_g16()
{
    // Representative size of the QHY990 sensor used by the reference scripts.
    constexpr int W = 1392;
    constexpr int H = 1032;
    constexpr int RUNS = 10;

    const QImage source = makeGradient(W, H, 1000, 5000);
    const QImage darkImage = makeGradient(W, H, 900, 100);

    Calibration::DarkSettings dark;
    dark.enabled = true;
    dark.method = Calibration::DarkMethod::MatchedDark;
    dark.matchedDark = reference(darkImage, 1.0);
    dark.outputOffset = 1000;
    const Calibration::FlatField field = makeFlatField(W, H);

    QVector<qint64> times;
    for (int i = 0; i < RUNS; ++i) {
        ImageData frame;
        frame.image = source;

        QElapsedTimer timer;
        timer.start();
        QString reason;
        const bool ok = Calibration::apply(frame, dark, &field, &reason);
        times.append(timer.nsecsElapsed() / 1000);
        QVERIFY2(ok, qPrintable(reason));
        QCOMPARE(frame.image.size(), source.size());
    }
    report("Calibration::apply dark + flat 1392x1032 Grayscale16", times);
}

void TestCalibrationPerf::test_classic_flat_build_1392x1032_g16()
{
    constexpr int W = 1392;
    constexpr int H = 1032;
    constexpr int RUNS = 3;

    const QImage flatImage = makeGradient(W, H, 30000, 2000);
    const QImage darkImage = makeGradient(W, H, 1000, 50);

    Calibration::FlatSettings settings;
    settings.flat = reference(flatImage, 1.0);
    settings.flatExposure = 1.0;

    Calibration::DarkSettings dark;
    dark.enabled = true;
    dark.method = Calibration::DarkMethod::MatchedDark;
    dark.matchedExposure = 1.0;
    dark.matchedDark = reference(darkImage, 1.0);

    QVector<qint64> times;
    for (int i = 0; i < RUNS; ++i) {
        QElapsedTimer timer;
        timer.start();
        QString error;
        const Calibration::FlatField field =
            Calibration::buildClassicFlat(settings, dark, &error);
        times.append(timer.nsecsElapsed() / 1000);
        QVERIFY2(field.isValid(), qPrintable(error));
        QCOMPARE(field.width, W);
        QCOMPARE(field.height, H);
    }
    report("Calibration::buildClassicFlat 1392x1032 Grayscale16", times);
}

void TestCalibrationPerf::test_vff_build_1392x1032_g16()
{
    constexpr int W = 1392;
    constexpr int H = 1032;
    constexpr int RUNS = 3;
    const QVector<double> exposures{0.01, 0.03, 0.06, 0.1, 1.0};

    Calibration::FlatSettings settings;
    for (double exposure : exposures) {
        // A realistic flat has an illumination gradient plus a dark offset.
        const int base = 3000;
        const int span = static_cast<int>(2000.0 * exposure / exposures.last());
        settings.vffEntries.push_back(reference(makeGradient(W, H, base, span),
                                                exposure, 3));
    }

    Calibration::DarkSettings dark;
    dark.enabled = true;
    dark.method = Calibration::DarkMethod::BiasScaled;
    dark.longDarkExposure = exposures.last();
    dark.bias = reference(makeGradient(W, H, 1000, 0), 0.0);
    dark.longDark = reference(makeGradient(W, H, 1100, 0), exposures.last());

    QVector<qint64> times;
    for (int i = 0; i < RUNS; ++i) {
        QElapsedTimer timer;
        timer.start();
        QString error;
        const Calibration::FlatField field =
            Calibration::buildVariableExposureFlat(settings, dark, &error);
        times.append(timer.nsecsElapsed() / 1000);
        QVERIFY2(field.isValid(), qPrintable(error));
        QCOMPARE(field.width, W);
        QCOMPARE(field.height, H);
    }
    report("Calibration::buildVariableExposureFlat 1392x1032 x5 Grayscale16", times);
}

QTEST_MAIN(TestCalibrationPerf)
#include "test_calibration_perf.moc"
