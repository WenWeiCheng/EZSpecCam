#include "Calibration.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

#include "CameraTypes.h"

namespace Calibration
{

namespace
{

constexpr double kSaturationFraction = 0.995;

bool fail(QString *error, const QString &message)
{
    if (error) {
        *error = message;
    }
    return false;
}

int maxValueForFormat(QImage::Format format)
{
    switch (format) {
    case QImage::Format_Grayscale8:
    case QImage::Format_RGB888:
    case QImage::Format_RGB32:
    case QImage::Format_ARGB32:
    case QImage::Format_ARGB32_Premultiplied:
    case QImage::Format_RGBA8888:
    case QImage::Format_RGBA8888_Premultiplied:
    case QImage::Format_Indexed8:
        return 255;
    default:
        return 65535;
    }
}

//! Pixel loop shared by all supported frame formats.
//!
//! Kept as a separate template so the per-pixel work is a handful of float
//! operations rather than a std::function-like callback; on a 24 MP Debug build
//! the callback version cost roughly 7x the integer-only legacy path.
template <typename Pixel>
void applyPixelLoop(ImageData &frame,
                    const FramePlane *matchedDark,
                    const FramePlane *bias,
                    const FramePlane *longDark,
                    float darkScale,
                    const FlatField *flat,
                    int offset,
                    int maxValue,
                    int channels)
{
    const int width = frame.image.width();
    const int height = frame.image.height();
    const int rowElements = width * channels;

    for (int y = 0; y < height; ++y) {
        Pixel *row = reinterpret_cast<Pixel *>(frame.image.scanLine(y));
        const int rowBase = y * rowElements;
        for (int x = 0; x < rowElements; ++x) {
            const int i = rowBase + x;
            float v = static_cast<float>(row[x]);

            if (matchedDark != nullptr) {
                v -= matchedDark->samples[i];
            } else if (bias != nullptr) {
                // D(t) = B + (D(t0) - B) * t/t0. The bias is the zero-exposure
                // level and is subtracted exactly once; the long dark only adds
                // its excess over that level.
                const float b = bias->samples[i];
                const float d = longDark->samples[i];
                v -= b + (d - b) * darkScale;
            }

            if (flat != nullptr) {
                v *= flat->gain[i];
            }
            v += static_cast<float>(offset);

            int out = static_cast<int>(v + 0.5f);
            if (out < 0) {
                out = 0;
            } else if (out > maxValue) {
                out = maxValue;
            }
            row[x] = static_cast<Pixel>(out);
        }
    }
}

double medianOf(std::vector<double> values)
{
    if (values.empty()) {
        return 0.0;
    }
    const std::size_t mid = values.size() / 2;
    std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(mid), values.end());
    const double upper = values[mid];
    if (values.size() % 2 != 0) {
        return upper;
    }
    std::nth_element(values.begin(), values.begin() + static_cast<std::ptrdiff_t>(mid - 1),
                     values.begin() + static_cast<std::ptrdiff_t>(mid));
    return 0.5 * (values[mid - 1] + upper);
}

bool geometryMatches(const FramePlane &plane, int width, int height, int channels)
{
    return plane.width == width && plane.height == height && plane.channels == channels;
}

bool referenceGeometryMatches(const ReferenceFrame &ref, int width, int height, int channels,
                              const QString &name, QString *reason)
{
    if (ref.isNull()) {
        if (reason) {
            *reason = QStringLiteral("%1 is not available.").arg(name);
        }
        return false;
    }
    if (!geometryMatches(ref.plane, width, height, channels)) {
        if (reason) {
            *reason = QStringLiteral("%1 size mismatch: reference %2x%3x%4 vs frame %5x%6x%7.")
                          .arg(name)
                          .arg(ref.plane.width)
                          .arg(ref.plane.height)
                          .arg(ref.plane.channels)
                          .arg(width)
                          .arg(height)
                          .arg(channels);
        }
        return false;
    }
    return true;
}

//! Build the flat gain map from an already bias-corrected signal plane.
FlatField buildFromSignal(const FramePlane &signal, int maxValue,
                          double lowResponseFraction, double maxGain,
                          QString *error)
{
    FlatField field;
    field.width = signal.width;
    field.height = signal.height;
    field.channels = signal.channels;
    field.gain.resize(signal.samples.size());
    field.gain.fill(1.0f);

    const int n = signal.sampleCount();
    if (n <= 0) {
        fail(error, QStringLiteral("Flat signal is empty."));
        return FlatField();
    }

    const double saturation = kSaturationFraction * static_cast<double>(maxValue);
    std::vector<unsigned char> valid(static_cast<std::size_t>(n), 1);
    std::vector<double> finiteValues;
    finiteValues.reserve(static_cast<std::size_t>(n));

    for (int i = 0; i < n; ++i) {
        const double v = static_cast<double>(signal.samples[i]);
        if (!std::isfinite(v) || v <= 0.0 || v >= saturation) {
            valid[static_cast<std::size_t>(i)] = 0;
            continue;
        }
        finiteValues.push_back(v);
    }
    if (finiteValues.empty()) {
        fail(error, QStringLiteral("Flat frame has no usable pixels (all saturated, zero, or invalid)."));
        return FlatField();
    }

    const double median = medianOf(finiteValues);
    if (!(median > 0.0)) {
        fail(error, QStringLiteral("Flat frame has a non-positive median signal."));
        return FlatField();
    }
    const double lowCut = lowResponseFraction * median;
    for (int i = 0; i < n; ++i) {
        if (valid[static_cast<std::size_t>(i)] != 0
            && static_cast<double>(signal.samples[i]) < lowCut) {
            valid[static_cast<std::size_t>(i)] = 0;
        }
    }

    std::vector<double> sum(static_cast<std::size_t>(signal.channels), 0.0);
    std::vector<quint64> count(static_cast<std::size_t>(signal.channels), 0);
    for (int i = 0; i < n; ++i) {
        if (valid[static_cast<std::size_t>(i)] == 0) {
            continue;
        }
        const int c = i % signal.channels;
        sum[static_cast<std::size_t>(c)] += static_cast<double>(signal.samples[i]);
        ++count[static_cast<std::size_t>(c)];
    }

    std::vector<double> channelMean(static_cast<std::size_t>(signal.channels), 0.0);
    for (int c = 0; c < signal.channels; ++c) {
        if (count[static_cast<std::size_t>(c)] == 0) {
            fail(error, QStringLiteral("Flat frame channel %1 has no usable pixels.").arg(c));
            return FlatField();
        }
        channelMean[static_cast<std::size_t>(c)] =
            sum[static_cast<std::size_t>(c)] / static_cast<double>(count[static_cast<std::size_t>(c)]);
        if (!(channelMean[static_cast<std::size_t>(c)] > 0.0)) {
            fail(error, QStringLiteral("Flat frame channel %1 has a non-positive mean.").arg(c));
            return FlatField();
        }
    }

    const double clampedMax = std::max(maxGain, 1.0);
    bool firstValid = true;
    for (int i = 0; i < n; ++i) {
        if (valid[static_cast<std::size_t>(i)] == 0) {
            continue;
        }
        const int c = i % signal.channels;
        const double v = static_cast<double>(signal.samples[i]);
        double g = channelMean[static_cast<std::size_t>(c)] / v;
        g = std::min(std::max(g, 1.0 / clampedMax), clampedMax);
        field.gain[i] = static_cast<float>(g);
        if (firstValid) {
            field.minGain = field.gain[i];
            field.maxGain = field.gain[i];
            firstValid = false;
        } else {
            field.minGain = std::min(field.minGain, field.gain[i]);
            field.maxGain = std::max(field.maxGain, field.gain[i]);
        }
    }

    double meanSum = 0.0;
    for (double m : channelMean) {
        meanSum += m;
    }
    field.meanSignal = meanSum / static_cast<double>(signal.channels);
    field.excludedSamples = static_cast<int>(std::count(valid.begin(), valid.end(),
                                                        static_cast<unsigned char>(0)));
    return field;
}

} // namespace

FramePlane planeFromImage(const QImage &image, QString *error)
{
    if (image.isNull()) {
        fail(error, QStringLiteral("Image is empty."));
        return FramePlane();
    }

    QImage source = image;
    if (source.format() != QImage::Format_Grayscale16
        && source.format() != QImage::Format_Grayscale8
        && source.format() != QImage::Format_RGB888) {
        // Reference files can come back as ARGB32/RGBA8888/Indexed8. Keep the
        // conversion local so the caller's image is never mutated.
        if (source.format() == QImage::Format_Indexed8
            || source.format() == QImage::Format_Mono
            || source.format() == QImage::Format_MonoLSB
            || source.format() == QImage::Format_Alpha8) {
            source = source.convertToFormat(QImage::Format_Grayscale8);
        } else if (source.hasAlphaChannel()
                   || source.format() == QImage::Format_RGB32
                   || source.format() == QImage::Format_RGBX8888
                   || source.format() == QImage::Format_RGBA8888
                   || source.format() == QImage::Format_RGBA8888_Premultiplied) {
            source = source.convertToFormat(QImage::Format_RGB888);
        } else {
            source = source.convertToFormat(QImage::Format_Grayscale16);
        }
        if (source.isNull()) {
            fail(error, QStringLiteral("Unsupported image format."));
            return FramePlane();
        }
    }

    FramePlane plane;
    plane.width = source.width();
    plane.height = source.height();
    plane.maxValue = maxValueForFormat(source.format());
    plane.channels = (source.format() == QImage::Format_RGB888) ? 3 : 1;
    plane.samples.resize(plane.width * plane.height * plane.channels);

    if (source.format() == QImage::Format_Grayscale16) {
        for (int y = 0; y < plane.height; ++y) {
            const ushort *row = reinterpret_cast<const ushort *>(source.constScanLine(y));
            for (int x = 0; x < plane.width; ++x) {
                plane.samples[y * plane.width + x] = static_cast<float>(row[x]);
            }
        }
    } else if (source.format() == QImage::Format_Grayscale8) {
        for (int y = 0; y < plane.height; ++y) {
            const uchar *row = source.constScanLine(y);
            for (int x = 0; x < plane.width; ++x) {
                plane.samples[y * plane.width + x] = static_cast<float>(row[x]);
            }
        }
    } else if (source.format() == QImage::Format_RGB888) {
        for (int y = 0; y < plane.height; ++y) {
            const uchar *row = source.constScanLine(y);
            for (int x = 0; x < plane.width; ++x) {
                const int base = y * plane.width * 3 + x * 3;
                plane.samples[base + 0] = static_cast<float>(row[x * 3 + 0]);
                plane.samples[base + 1] = static_cast<float>(row[x * 3 + 1]);
                plane.samples[base + 2] = static_cast<float>(row[x * 3 + 2]);
            }
        }
    } else {
        fail(error, QStringLiteral("Unsupported image format after conversion."));
        return FramePlane();
    }

    return plane;
}

FlatField buildClassicFlat(const FlatSettings &settings, const DarkSettings &dark,
                           QString *error)
{
    if (settings.flat.isNull()) {
        fail(error, QStringLiteral("No flat frame is available."));
        return FlatField();
    }

    const FramePlane &flat = settings.flat.plane;
    if (!dark.enabled) {
        // No dark model: normalize the raw flat exactly as before.
        return buildFromSignal(flat, flat.maxValue, settings.lowResponseFraction,
                               settings.maxGain, error);
    }

    if (!(settings.flatExposure > 0.0)) {
        fail(error, QStringLiteral("Flat exposure must be greater than zero when dark calibration is enabled."));
        return FlatField();
    }

    QVector<float> darkPlane;
    if (!buildDarkPlane(dark, settings.flatExposure, flat.width, flat.height,
                        flat.channels, flat.maxValue, darkPlane, error)) {
        return FlatField();
    }

    FramePlane signal;
    signal.width = flat.width;
    signal.height = flat.height;
    signal.channels = flat.channels;
    signal.maxValue = flat.maxValue;
    signal.samples.resize(flat.samples.size());

    // A raw saturated pixel minus the bias can fall below the saturation
    // threshold and look usable. Keep it flagged by leaving it at full scale;
    // buildFromSignal() will then exclude it from the normalization.
    const double saturation = kSaturationFraction * static_cast<double>(flat.maxValue);
    const int n = signal.sampleCount();
    for (int i = 0; i < n; ++i) {
        const double raw = static_cast<double>(flat.samples[i]);
        signal.samples[i] = raw >= saturation
            ? static_cast<float>(flat.maxValue)
            : static_cast<float>(raw - static_cast<double>(darkPlane[i]));
    }

    return buildFromSignal(signal, flat.maxValue, settings.lowResponseFraction,
                           settings.maxGain, error);
}

FlatField buildVariableExposureFlat(const FlatSettings &settings, const DarkSettings &dark,
                                    QString *error)
{
    QVector<ReferenceFrame> entries;
    entries.reserve(settings.vffEntries.size());
    for (const ReferenceFrame &entry : settings.vffEntries) {
        if (entry.isValid() && entry.exposure > 0.0) {
            entries.push_back(entry);
        }
    }
    if (entries.size() < 2) {
        fail(error, QStringLiteral("Variable-exposure flat needs at least two exposures."));
        return FlatField();
    }

    std::sort(entries.begin(), entries.end(), [](const ReferenceFrame &a, const ReferenceFrame &b) {
        return a.exposure < b.exposure;
    });

    const FramePlane &first = entries.first().plane;
    for (const ReferenceFrame &entry : entries) {
        if (!geometryMatches(entry.plane, first.width, first.height, first.channels)) {
            fail(error, QStringLiteral("Variable-exposure flat frames have different sizes."));
            return FlatField();
        }
        if (entry.plane.maxValue != first.maxValue) {
            fail(error, QStringLiteral("Variable-exposure flat frames mix 8-bit and 16-bit data."));
            return FlatField();
        }
    }

    // Reject duplicate exposures: the least-squares denominator would be zero.
    bool haveDistinct = false;
    for (int i = 1; i < entries.size(); ++i) {
        if (std::abs(entries[i].exposure - entries[i - 1].exposure)
            > 1e-9 * std::max(1.0, std::abs(entries[i].exposure))) {
            haveDistinct = true;
            break;
        }
    }
    if (!haveDistinct) {
        fail(error, QStringLiteral("Variable-exposure flat needs at least two distinct exposures."));
        return FlatField();
    }

    const int n = first.width * first.height * first.channels;
    const int channels = first.channels;

    // Saturation is checked on the raw flats, before dark subtraction: a
    // saturated pixel minus the bias can otherwise look like a valid signal.
    std::vector<unsigned char> saturated(static_cast<std::size_t>(n), 0);
    for (const ReferenceFrame &entry : entries) {
        const double saturation = kSaturationFraction * static_cast<double>(entry.plane.maxValue);
        for (int i = 0; i < n; ++i) {
            if (static_cast<double>(entry.plane.samples[i]) >= saturation) {
                saturated[static_cast<std::size_t>(i)] = 1;
            }
        }
    }

    if (dark.enabled) {
        if (dark.method == DarkMethod::MatchedDark) {
            fail(error, QStringLiteral("Variable-exposure flat needs the bias+scaled dark model; "
                                       "a matched dark covers only one exposure."));
            return FlatField();
        }
        for (ReferenceFrame &entry : entries) {
            QVector<float> darkPlane;
            if (!buildDarkPlane(dark, entry.exposure, first.width, first.height,
                                channels, first.maxValue, darkPlane, error)) {
                return FlatField();
            }
            for (int i = 0; i < n; ++i) {
                entry.plane.samples[i] -= darkPlane[i];
            }
        }
    }

    std::vector<double> slopes(static_cast<std::size_t>(n), 0.0);

    double sumW = 0.0;
    double sumWt = 0.0;
    double sumWt2 = 0.0;
    for (const ReferenceFrame &entry : entries) {
        const double w = static_cast<double>(entry.frameCount > 0 ? entry.frameCount : 1);
        sumW += w;
        sumWt += w * entry.exposure;
        sumWt2 += w * entry.exposure * entry.exposure;
    }
    const double denominator = sumW * sumWt2 - sumWt * sumWt;
    if (denominator <= 1e-12 * std::max(1.0, sumW * sumW)) {
        fail(error, QStringLiteral("Variable-exposure flat exposures are too close together."));
        return FlatField();
    }

    for (int i = 0; i < n; ++i) {
        if (saturated[static_cast<std::size_t>(i)] != 0) {
            continue;
        }
        double sumWy = 0.0;
        double sumWty = 0.0;
        for (const ReferenceFrame &entry : entries) {
            const double w = static_cast<double>(entry.frameCount > 0 ? entry.frameCount : 1);
            const double y = static_cast<double>(entry.plane.samples[i]);
            sumWy += w * y;
            sumWty += w * entry.exposure * y;
        }
        slopes[static_cast<std::size_t>(i)] =
            (sumW * sumWty - sumWt * sumWy) / denominator;
    }

    std::vector<double> validSlopes;
    validSlopes.reserve(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) {
        if (saturated[static_cast<std::size_t>(i)] == 0) {
            const double s = slopes[static_cast<std::size_t>(i)];
            if (std::isfinite(s) && s > 0.0) {
                validSlopes.push_back(s);
            }
        }
    }
    if (validSlopes.empty()) {
        fail(error, QStringLiteral("Variable-exposure flat has no usable pixels."));
        return FlatField();
    }

    const double medianSlope = medianOf(validSlopes);
    const double lowCut = settings.lowResponseFraction * medianSlope;

    std::vector<double> sum(static_cast<std::size_t>(channels), 0.0);
    std::vector<quint64> count(static_cast<std::size_t>(channels), 0);
    for (int i = 0; i < n; ++i) {
        if (saturated[static_cast<std::size_t>(i)] != 0) {
            continue;
        }
        const double s = slopes[static_cast<std::size_t>(i)];
        if (!std::isfinite(s) || s <= 0.0 || s < lowCut) {
            continue;
        }
        const int c = i % channels;
        sum[static_cast<std::size_t>(c)] += s;
        ++count[static_cast<std::size_t>(c)];
    }

    std::vector<double> channelMean(static_cast<std::size_t>(channels), 0.0);
    for (int c = 0; c < channels; ++c) {
        if (count[static_cast<std::size_t>(c)] == 0) {
            fail(error, QStringLiteral("Variable-exposure flat channel %1 has no usable pixels.").arg(c));
            return FlatField();
        }
        channelMean[static_cast<std::size_t>(c)] =
            sum[static_cast<std::size_t>(c)] / static_cast<double>(count[static_cast<std::size_t>(c)]);
    }

    FlatField field;
    field.width = first.width;
    field.height = first.height;
    field.channels = channels;
    field.gain.resize(n);
    field.gain.fill(1.0f);

    const double clampedMax = std::max(settings.maxGain, 1.0);
    bool firstValid = true;
    int excluded = 0;
    for (int i = 0; i < n; ++i) {
        const double s = slopes[static_cast<std::size_t>(i)];
        if (saturated[static_cast<std::size_t>(i)] != 0
            || !std::isfinite(s) || s <= 0.0 || s < lowCut) {
            ++excluded;
            continue;
        }
        const int c = i % channels;
        double g = channelMean[static_cast<std::size_t>(c)] / s;
        g = std::min(std::max(g, 1.0 / clampedMax), clampedMax);
        field.gain[i] = static_cast<float>(g);
        if (firstValid) {
            field.minGain = field.gain[i];
            field.maxGain = field.gain[i];
            firstValid = false;
        } else {
            field.minGain = std::min(field.minGain, field.gain[i]);
            field.maxGain = std::max(field.maxGain, field.gain[i]);
        }
    }

    double meanSum = 0.0;
    for (double m : channelMean) {
        meanSum += m;
    }
    field.meanSignal = meanSum / static_cast<double>(channels);
    field.excludedSamples = excluded;
    return field;
}

bool buildDarkPlane(const DarkSettings &settings, double exposure,
                    int width, int height, int channels, int maxValue,
                    QVector<float> &darkPlane, QString *reason)
{
    darkPlane.clear();
    if (!settings.enabled) {
        return true;
    }

    if (settings.method == DarkMethod::MatchedDark) {
        if (!referenceGeometryMatches(settings.matchedDark, width, height, channels,
                                      QStringLiteral("Dark frame"), reason)) {
            return false;
        }
        if (settings.matchedDark.plane.maxValue != maxValue) {
            return fail(reason, QStringLiteral("Dark frame bit depth does not match the frame."));
        }
        if (!(settings.matchedExposure > 0.0)) {
            return fail(reason, QStringLiteral("Matched dark exposure must be greater than zero."));
        }
        // The dialog stores seconds with microsecond resolution; allow that
        // much slack so "1.0" and "1.000001" are treated as the same nominal
        // exposure instead of forcing the user to retype both fields.
        const double tolerance = 1e-6 * std::max({1.0, std::abs(exposure),
                                                  std::abs(settings.matchedExposure)});
        if (std::abs(exposure - settings.matchedExposure) > tolerance) {
            return fail(reason,
                        QStringLiteral("Matched dark is at %1 s but %2 s was requested. "
                                       "Use bias + scaled dark for other exposures.")
                            .arg(settings.matchedExposure)
                            .arg(exposure));
        }
        darkPlane = settings.matchedDark.plane.samples;
        return true;
    }

    if (!referenceGeometryMatches(settings.bias, width, height, channels,
                                  QStringLiteral("Bias frame"), reason)
        || !referenceGeometryMatches(settings.longDark, width, height, channels,
                                     QStringLiteral("Long dark frame"), reason)) {
        return false;
    }
    if (settings.bias.plane.maxValue != maxValue
        || settings.longDark.plane.maxValue != maxValue) {
        return fail(reason, QStringLiteral("Bias/long dark bit depth does not match the frame."));
    }
    if (!(exposure >= 0.0)) {
        return fail(reason, QStringLiteral("Exposure must not be negative."));
    }
    if (!(settings.longDarkExposure > 0.0)) {
        return fail(reason, QStringLiteral("Long dark exposure must be greater than zero."));
    }

    const double scale = exposure / settings.longDarkExposure;
    const int n = width * height * channels;
    darkPlane.resize(n);
    for (int i = 0; i < n; ++i) {
        const double b = static_cast<double>(settings.bias.plane.samples[i]);
        const double d = static_cast<double>(settings.longDark.plane.samples[i]);
        darkPlane[i] = static_cast<float>(b + (d - b) * scale);
    }
    return true;
}

bool apply(ImageData &frame, const DarkSettings &dark, const FlatField *flat,
           QString *reason)
{
    if (reason) {
        reason->clear();
    }
    if (!dark.enabled && flat == nullptr) {
        return true;
    }
    if (frame.image.isNull()) {
        return fail(reason, QStringLiteral("Frame is empty."));
    }

    const QImage::Format format = frame.image.format();
    if (format != QImage::Format_Grayscale16
        && format != QImage::Format_Grayscale8
        && format != QImage::Format_RGB888) {
        return fail(reason, QStringLiteral("Unsupported frame format for calibration."));
    }

    const int width = frame.image.width();
    const int height = frame.image.height();
    const int channels = (format == QImage::Format_RGB888) ? 3 : 1;
    const int maxValue = (format == QImage::Format_Grayscale16) ? 65535 : 255;

    // Read the reference planes directly instead of materializing a full float
    // dark plane for every frame. At 6000x4000 that copy alone is ~96 MB per
    // frame; the scaled method is cheap enough to evaluate inline.
    const FramePlane *matchedPlane = nullptr;
    const FramePlane *biasPlane = nullptr;
    const FramePlane *longDarkPlane = nullptr;
    double darkScale = 1.0;
    if (dark.enabled) {
        if (dark.method == DarkMethod::MatchedDark) {
            if (!referenceGeometryMatches(dark.matchedDark, width, height, channels,
                                          QStringLiteral("Dark frame"), reason)) {
                return false;
            }
            if (dark.matchedDark.plane.maxValue != maxValue) {
                return fail(reason, QStringLiteral("Dark frame bit depth does not match the frame."));
            }
            matchedPlane = &dark.matchedDark.plane;
        } else {
            if (!referenceGeometryMatches(dark.bias, width, height, channels,
                                          QStringLiteral("Bias frame"), reason)
                || !referenceGeometryMatches(dark.longDark, width, height, channels,
                                             QStringLiteral("Long dark frame"), reason)) {
                return false;
            }
            if (dark.bias.plane.maxValue != maxValue
                || dark.longDark.plane.maxValue != maxValue) {
                return fail(reason, QStringLiteral("Bias/long dark bit depth does not match the frame."));
            }
            if (!(dark.targetExposure > 0.0)) {
                return fail(reason, QStringLiteral("Signal exposure must be greater than zero."));
            }
            if (!(dark.longDarkExposure > 0.0)) {
                return fail(reason, QStringLiteral("Long dark exposure must be greater than zero."));
            }
            darkScale = dark.targetExposure / dark.longDarkExposure;
            biasPlane = &dark.bias.plane;
            longDarkPlane = &dark.longDark.plane;
        }
    }

    if (flat != nullptr) {
        if (!flat->isValid()) {
            return fail(reason, QStringLiteral("Flat field is not valid."));
        }
        if (flat->width != width || flat->height != height || flat->channels != channels) {
            return fail(reason, QStringLiteral("Flat field size mismatch: field %1x%2x%3 vs frame %4x%5x%6.")
                                                    .arg(flat->width).arg(flat->height).arg(flat->channels)
                                                    .arg(width).arg(height).arg(channels));
        }
    }

    const int offset = dark.enabled ? dark.outputOffset : 0;

    // Detach once before taking write pointers. Asking for constScanLine() and
    // scanLine() on the same row would otherwise leave the read pointer dangling
    // when scanLine() triggers a copy-on-write detach.
    frame.image.detach();

    if (format == QImage::Format_Grayscale16) {
        applyPixelLoop<ushort>(frame, matchedPlane, biasPlane, longDarkPlane,
                               static_cast<float>(darkScale), flat, offset, maxValue, 1);
    } else if (format == QImage::Format_Grayscale8) {
        applyPixelLoop<uchar>(frame, matchedPlane, biasPlane, longDarkPlane,
                              static_cast<float>(darkScale), flat, offset, maxValue, 1);
    } else { // RGB888
        applyPixelLoop<uchar>(frame, matchedPlane, biasPlane, longDarkPlane,
                              static_cast<float>(darkScale), flat, offset, maxValue, 3);
    }

    return true;
}

} // namespace Calibration
