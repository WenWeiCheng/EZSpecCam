// Calibration math: the two dark-frame methods and the two flat-field methods
// from the reference Python scripts.
//
// These are pure functions over FramePlane/QImage, so no camera or MainWindow
// is involved. The images are deliberately tiny and hand-computable.
#include <QImage>
#include <QTest>

#include "Calibration.h"
#include "CameraTypes.h"

class TestFlatCalibration : public QObject
{
    Q_OBJECT

private slots:
    void test_matched_dark_subtracts_and_adds_offset();
    void test_bias_scaled_dark_uses_t_over_t0();
    void test_bias_scaled_dark_rejects_zero_exposure();
    void test_dark_size_mismatch_is_reported();
    void test_dark_plane_can_be_evaluated_at_an_arbitrary_exposure();
    void test_dark_plane_rejects_matched_dark_at_another_exposure();
    void test_classic_flat_flattens_a_gradient();
    void test_classic_flat_reuses_bias_scaled_dark();
    void test_classic_flat_rejects_matched_dark_at_another_exposure();
    void test_classic_flat_leaves_low_response_untouched();
    void test_classic_flat_requires_a_flat_frame();
    void test_vff_recovers_per_pixel_slopes();
    void test_vff_reuses_bias_scaled_dark();
    void test_vff_rejects_matched_dark();
    void test_vff_requires_two_distinct_exposures();
    void test_plane_from_grayscale8_and_rgb();

private:
    static QImage makeGrayscale16(const QVector<int> &values);
    static Calibration::ReferenceFrame referenceFrom(const QVector<int> &values,
                                                     double exposure,
                                                     int frameCount = 1);
};

QImage TestFlatCalibration::makeGrayscale16(const QVector<int> &values)
{
    QImage image(values.size(), 1, QImage::Format_Grayscale16);
    ushort *row = reinterpret_cast<ushort *>(image.scanLine(0));
    for (int x = 0; x < values.size(); ++x) {
        row[x] = static_cast<ushort>(values[x]);
    }
    return image;
}

Calibration::ReferenceFrame TestFlatCalibration::referenceFrom(const QVector<int> &values,
                                                               double exposure,
                                                               int frameCount)
{
    Calibration::ReferenceFrame reference;
    reference.plane = Calibration::planeFromImage(makeGrayscale16(values));
    reference.exposure = exposure;
    reference.frameCount = frameCount;
    return reference;
}

void TestFlatCalibration::test_matched_dark_subtracts_and_adds_offset()
{
    ImageData frame;
    frame.image = makeGrayscale16({100, 200, 300});

    Calibration::DarkSettings dark;
    dark.enabled = true;
    dark.method = Calibration::DarkMethod::MatchedDark;
    dark.outputOffset = 10;
    dark.matchedDark = referenceFrom({50, 60, 70}, 1.0);

    QString reason;
    QVERIFY2(Calibration::apply(frame, dark, nullptr, &reason), qPrintable(reason));

    const ushort *row = reinterpret_cast<const ushort *>(frame.image.constScanLine(0));
    QCOMPARE(int(row[0]), 60);
    QCOMPARE(int(row[1]), 150);
    QCOMPARE(int(row[2]), 240);
}

void TestFlatCalibration::test_bias_scaled_dark_uses_t_over_t0()
{
    ImageData frame;
    frame.image = makeGrayscale16({100, 200, 300});

    Calibration::DarkSettings dark;
    dark.enabled = true;
    dark.method = Calibration::DarkMethod::BiasScaled;
    dark.longDarkExposure = 10.0;
    dark.targetExposure = 1.0;      // t/t0 = 0.1
    dark.outputOffset = 5;
    dark.bias = referenceFrom({10, 20, 30}, 0.001);
    dark.longDark = referenceFrom({110, 220, 330}, 10.0);

    QString reason;
    QVERIFY2(Calibration::apply(frame, dark, nullptr, &reason), qPrintable(reason));

    // dark = B + (D - B) * 0.1 = {20, 40, 60}; out = I - dark + 5
    const ushort *row = reinterpret_cast<const ushort *>(frame.image.constScanLine(0));
    QCOMPARE(int(row[0]), 85);
    QCOMPARE(int(row[1]), 165);
    QCOMPARE(int(row[2]), 245);
}

void TestFlatCalibration::test_bias_scaled_dark_rejects_zero_exposure()
{
    ImageData frame;
    frame.image = makeGrayscale16({100});

    Calibration::DarkSettings dark;
    dark.enabled = true;
    dark.method = Calibration::DarkMethod::BiasScaled;
    dark.longDarkExposure = 10.0;
    dark.targetExposure = 0.0;
    dark.bias = referenceFrom({10}, 0.001);
    dark.longDark = referenceFrom({110}, 10.0);

    QString reason;
    QVERIFY(!Calibration::apply(frame, dark, nullptr, &reason));
    QVERIFY2(reason.contains(QStringLiteral("exposure"), Qt::CaseInsensitive),
             qPrintable(reason));
}

void TestFlatCalibration::test_dark_size_mismatch_is_reported()
{
    ImageData frame;
    frame.image = makeGrayscale16({100, 200});

    Calibration::DarkSettings dark;
    dark.enabled = true;
    dark.matchedDark = referenceFrom({10}, 1.0);

    QString reason;
    QVERIFY(!Calibration::apply(frame, dark, nullptr, &reason));
    QVERIFY2(reason.contains(QStringLiteral("mismatch"), Qt::CaseInsensitive),
             qPrintable(reason));
}

void TestFlatCalibration::test_dark_plane_can_be_evaluated_at_an_arbitrary_exposure()
{
    Calibration::DarkSettings dark;
    dark.enabled = true;
    dark.method = Calibration::DarkMethod::BiasScaled;
    dark.longDarkExposure = 10.0;
    dark.bias = referenceFrom({10, 20}, 0.0);
    dark.longDark = referenceFrom({110, 220}, 10.0);

    QVector<float> plane;
    QString error;
    QVERIFY2(Calibration::buildDarkPlane(dark, 2.0, 2, 1, 1, 65535, plane, &error),
             qPrintable(error));
    QCOMPARE(plane.size(), 2);
    QCOMPARE(plane[0], 30.0f);
    QCOMPARE(plane[1], 60.0f);
}

void TestFlatCalibration::test_dark_plane_rejects_matched_dark_at_another_exposure()
{
    Calibration::DarkSettings dark;
    dark.enabled = true;
    dark.method = Calibration::DarkMethod::MatchedDark;
    dark.matchedExposure = 1.0;
    dark.matchedDark = referenceFrom({10}, 1.0);

    QVector<float> plane;
    QString error;
    QVERIFY(!Calibration::buildDarkPlane(dark, 2.0, 1, 1, 1, 65535, plane, &error));
    QVERIFY2(error.contains(QStringLiteral("Matched dark"), Qt::CaseInsensitive),
             qPrintable(error));
}

void TestFlatCalibration::test_classic_flat_flattens_a_gradient()
{
    Calibration::FlatSettings settings;
    settings.flat = referenceFrom({100, 200, 400}, 1.0);
    settings.flatExposure = 1.0;
    settings.lowResponseFraction = 0.2;

    Calibration::DarkSettings dark;
    dark.enabled = true;
    dark.method = Calibration::DarkMethod::MatchedDark;
    dark.matchedExposure = 1.0;
    dark.matchedDark = referenceFrom({10, 10, 10}, 1.0);

    QString error;
    const Calibration::FlatField field =
        Calibration::buildClassicFlat(settings, dark, &error);
    QVERIFY2(field.isValid(), qPrintable(error));
    QCOMPARE(field.width, 3);
    QCOMPARE(field.channels, 1);

    // A signal twice as bright as the dark-corrected flat: (180,380,780).
    // Multiplying by the normalized flat should make it uniform at
    // 2 * mean(90,190,390) = 446.7 -> 447.
    ImageData frame;
    frame.image = makeGrayscale16({180, 380, 780});
    QString reason;
    QVERIFY2(Calibration::apply(frame, Calibration::DarkSettings{}, &field, &reason),
             qPrintable(reason));

    const ushort *row = reinterpret_cast<const ushort *>(frame.image.constScanLine(0));
    QCOMPARE(int(row[0]), 447);
    QCOMPARE(int(row[1]), 447);
    QCOMPARE(int(row[2]), 447);
}

void TestFlatCalibration::test_classic_flat_reuses_bias_scaled_dark()
{
    Calibration::FlatSettings settings;
    // Raw flat = illumination {100,200,400} + dark(1 s) {20,20,20}.
    settings.flat = referenceFrom({120, 220, 420}, 1.0);
    settings.flatExposure = 1.0;
    settings.lowResponseFraction = 0.2;

    Calibration::DarkSettings dark;
    dark.enabled = true;
    dark.method = Calibration::DarkMethod::BiasScaled;
    dark.longDarkExposure = 10.0;
    dark.bias = referenceFrom({10, 10, 10}, 0.0);
    dark.longDark = referenceFrom({110, 110, 110}, 10.0);

    QString error;
    const Calibration::FlatField field =
        Calibration::buildClassicFlat(settings, dark, &error);
    QVERIFY2(field.isValid(), qPrintable(error));

    // The dark model gives 10 + (110-10)*1/10 = 20, so the normalized flat
    // must come from {100,200,400}, not from the raw {120,220,420}.
    QVERIFY2(qAbs(field.meanSignal - 233.333333) < 1e-3,
             qPrintable(QString::number(field.meanSignal)));
    QVERIFY2(qAbs(field.gain[0] - 2.333333f) < 1e-5f,
             qPrintable(QString::number(field.gain[0])));
}

void TestFlatCalibration::test_classic_flat_rejects_matched_dark_at_another_exposure()
{
    Calibration::FlatSettings settings;
    settings.flat = referenceFrom({100, 200}, 2.0);
    settings.flatExposure = 2.0;

    Calibration::DarkSettings dark;
    dark.enabled = true;
    dark.method = Calibration::DarkMethod::MatchedDark;
    dark.matchedExposure = 1.0;
    dark.matchedDark = referenceFrom({10, 10}, 1.0);

    QString error;
    const Calibration::FlatField field =
        Calibration::buildClassicFlat(settings, dark, &error);
    QVERIFY(!field.isValid());
    QVERIFY2(error.contains(QStringLiteral("Matched dark"), Qt::CaseInsensitive),
             qPrintable(error));
}

void TestFlatCalibration::test_classic_flat_leaves_low_response_untouched()
{
    Calibration::FlatSettings settings;
    settings.flat = referenceFrom({100, 1}, 1.0);
    settings.lowResponseFraction = 0.2;

    QString error;
    const Calibration::FlatField field =
        Calibration::buildClassicFlat(settings, Calibration::DarkSettings{}, &error);
    QVERIFY2(field.isValid(), qPrintable(error));
    QCOMPARE(field.excludedSamples, 1);

    ImageData frame;
    frame.image = makeGrayscale16({500, 500});
    QString reason;
    QVERIFY2(Calibration::apply(frame, Calibration::DarkSettings{}, &field, &reason),
             qPrintable(reason));

    const ushort *row = reinterpret_cast<const ushort *>(frame.image.constScanLine(0));
    // Pixel 0 gets the normalized gain (exactly 1), pixel 1 is left alone.
    QCOMPARE(int(row[0]), 500);
    QCOMPARE(int(row[1]), 500);
}

void TestFlatCalibration::test_classic_flat_requires_a_flat_frame()
{
    Calibration::FlatSettings settings;
    QString error;
    const Calibration::FlatField field =
        Calibration::buildClassicFlat(settings, Calibration::DarkSettings{}, &error);
    QVERIFY(!field.isValid());
    QVERIFY(!error.isEmpty());
}

void TestFlatCalibration::test_vff_recovers_per_pixel_slopes()
{
    Calibration::FlatSettings settings;
    settings.lowResponseFraction = 0.2;

    // Pixel 0: 100*t + 10, pixel 1: 200*t + 10.
    const QVector<double> exposures{1.0, 2.0, 4.0};
    for (double t : exposures) {
        Calibration::ReferenceFrame entry;
        entry.plane = Calibration::planeFromImage(
            makeGrayscale16({static_cast<int>(100.0 * t + 10.0),
                             static_cast<int>(200.0 * t + 10.0)}));
        entry.exposure = t;
        entry.frameCount = 2;
        settings.vffEntries.push_back(entry);
    }

    QString error;
    const Calibration::FlatField field =
        Calibration::buildVariableExposureFlat(settings, Calibration::DarkSettings{}, &error);
    QVERIFY2(field.isValid(), qPrintable(error));
    QCOMPARE(field.gain.size(), 2);

    // mean slope = 150, so gains are 1.5 and 0.75.
    QVERIFY2(qAbs(field.gain[0] - 1.5f) < 1e-5f,
             qPrintable(QString::number(field.gain[0])));
    QVERIFY2(qAbs(field.gain[1] - 0.75f) < 1e-5f,
             qPrintable(QString::number(field.gain[1])));
}

void TestFlatCalibration::test_vff_reuses_bias_scaled_dark()
{
    Calibration::FlatSettings settings;
    settings.lowResponseFraction = 0.2;

    // Signal slopes are 100 and 200 ADU/s. Raw flats also contain the dark
    // model B + (D-B)*t/t0 = 10 + 10*t, so fitting raw values would recover
    // 110 and 210 instead. The gain must come from the dark-corrected slopes.
    const QVector<double> exposures{1.0, 2.0, 4.0};
    for (double t : exposures) {
        Calibration::ReferenceFrame entry;
        entry.plane = Calibration::planeFromImage(
            makeGrayscale16({static_cast<int>(100.0 * t + 10.0 + 10.0 * t),
                             static_cast<int>(200.0 * t + 10.0 + 10.0 * t)}));
        entry.exposure = t;
        entry.frameCount = 1;
        settings.vffEntries.push_back(entry);
    }

    Calibration::DarkSettings dark;
    dark.enabled = true;
    dark.method = Calibration::DarkMethod::BiasScaled;
    dark.longDarkExposure = 10.0;
    dark.bias = referenceFrom({10, 10}, 0.0);
    dark.longDark = referenceFrom({110, 110}, 10.0);

    QString error;
    const Calibration::FlatField field =
        Calibration::buildVariableExposureFlat(settings, dark, &error);
    QVERIFY2(field.isValid(), qPrintable(error));
    QCOMPARE(field.gain.size(), 2);
    QVERIFY2(qAbs(field.gain[0] - 1.5f) < 1e-5f,
             qPrintable(QString::number(field.gain[0])));
    QVERIFY2(qAbs(field.gain[1] - 0.75f) < 1e-5f,
             qPrintable(QString::number(field.gain[1])));
}

void TestFlatCalibration::test_vff_rejects_matched_dark()
{
    Calibration::FlatSettings settings;
    settings.vffEntries.push_back(referenceFrom({100, 200}, 1.0));
    settings.vffEntries.push_back(referenceFrom({200, 400}, 2.0));

    Calibration::DarkSettings dark;
    dark.enabled = true;
    dark.method = Calibration::DarkMethod::MatchedDark;
    dark.matchedExposure = 1.0;
    dark.matchedDark = referenceFrom({10, 10}, 1.0);

    QString error;
    const Calibration::FlatField field =
        Calibration::buildVariableExposureFlat(settings, dark, &error);
    QVERIFY(!field.isValid());
    QVERIFY2(error.contains(QStringLiteral("bias"), Qt::CaseInsensitive),
             qPrintable(error));
}

void TestFlatCalibration::test_vff_requires_two_distinct_exposures()
{
    Calibration::FlatSettings settings;
    settings.vffEntries.push_back(referenceFrom({100, 200}, 1.0));
    settings.vffEntries.push_back(referenceFrom({120, 240}, 1.0));

    QString error;
    const Calibration::FlatField field =
        Calibration::buildVariableExposureFlat(settings, Calibration::DarkSettings{}, &error);
    QVERIFY(!field.isValid());
    QVERIFY2(error.contains(QStringLiteral("distinct")), qPrintable(error));
}

void TestFlatCalibration::test_plane_from_grayscale8_and_rgb()
{
    QImage gray8(2, 1, QImage::Format_Grayscale8);
    gray8.scanLine(0)[0] = 10;
    gray8.scanLine(0)[1] = 250;
    const Calibration::FramePlane gray = Calibration::planeFromImage(gray8);
    QVERIFY(gray.isValid());
    QCOMPARE(gray.channels, 1);
    QCOMPARE(gray.maxValue, 255);
    QCOMPARE(gray.samples[0], 10.0f);
    QCOMPARE(gray.samples[1], 250.0f);

    QImage rgb(1, 1, QImage::Format_RGB888);
    uchar *pixel = rgb.scanLine(0);
    pixel[0] = 1;
    pixel[1] = 2;
    pixel[2] = 3;
    const Calibration::FramePlane color = Calibration::planeFromImage(rgb);
    QVERIFY(color.isValid());
    QCOMPARE(color.channels, 3);
    QCOMPARE(color.samples[0], 1.0f);
    QCOMPARE(color.samples[1], 2.0f);
    QCOMPARE(color.samples[2], 3.0f);
}

QTEST_MAIN(TestFlatCalibration)
#include "test_flat_calibration.moc"
