#pragma once

/**
 * @file Calibration.h
 * @brief Dark-frame and flat-field calibration shared by the GUI and tests.
 *
 * The two dark-frame methods mirror the reference Python scripts:
 *
 *  - MatchedDark:  I' = I - D(t) + C
 *  - BiasScaled:   I' = I - [B + (D(t0) - B) * t/t0] + C
 *
 * The two flat-field methods mirror calib_flat_classic.py and
 * calib_flat_variable_exposure.py:
 *
 *  - Classic:  FF(x,y) = <F - D(t_flat)> / (F(x,y) - D(x,y,t_flat))
 *  - VFF:      per-pixel least-squares slope of F - D(t) over exposure, then
 *              VFF(x,y) = <slope> / slope(x,y)
 *
 * The flat builders take the same DarkSettings as apply(), so the flat signal
 * is isolated with the same dark model instead of a second, separate flat dark
 * reference. MatchedDark is exposure-specific; flat construction at another
 * exposure therefore requires BiasScaled.
 *
 * All arithmetic is kept in floating point until the final write-back, and the
 * output bias C is added after the flat multiplication so a flat field cannot
 * scale the user's offset.
 *
 * Calibration is an additive layer followed by a multiplicative layer:
 *
 *      display = clip((I - D) * FF + C)
 *
 * The caller owns the reference frames; this module only turns them into a
 * dark estimate, a flat gain map, and applies those to an ImageData frame.
 */

#include <QImage>
#include <QString>
#include <QVector>

struct ImageData;

namespace Calibration
{

//! How the dark contribution is estimated for the current exposure.
enum class DarkMethod
{
    MatchedDark, //!< dark frame acquired at the same exposure as the signal
    BiasScaled   //!< bias frame plus a long dark scaled by t/t0
};

//! How the flat-field gain map is constructed.
enum class FlatMethod
{
    Classic,          //!< mean-normalized flat at one exposure
    VariableExposure  //!< per-pixel least-squares slope across several exposures
};

//! Which reference slot an acquisition or file load belongs to.
enum class ReferenceKind
{
    MatchedDark,
    Bias,
    LongDark,
    Flat,
    VffEntry
};

/**
 * @brief Format-independent pixel plane.
 *
 * Grayscale images have channels == 1, RGB images have channels == 3. Samples
 * are interleaved exactly like QImage::Format_RGB888. Keeping calibration
 * references in this form means a frame loader returning ARGB32 or Grayscale8
 * can still feed a Grayscale16 session once it has been converted.
 */
struct FramePlane
{
    int width = 0;
    int height = 0;
    int channels = 1;
    //! Nominal full-scale value of the source format (255 or 65535).
    int maxValue = 65535;
    QVector<float> samples;

    bool isValid() const
    {
        return width > 0 && height > 0 && channels > 0
            && samples.size() == width * height * channels;
    }

    int sampleCount() const { return width * height * channels; }
};

//! One acquired or loaded reference image together with its acquisition facts.
struct ReferenceFrame
{
    FramePlane plane;
    //! Exposure time in seconds, entered by the user.
    double exposure = 0.0;
    //! Number of frames averaged into plane. 1 for a loaded single image.
    int frameCount = 0;
    //! Absolute file path when loaded from disk; empty when held in memory.
    QString source;

    bool isValid() const { return plane.isValid(); }
    bool isNull() const { return !isValid(); }
    bool isInMemory() const { return isValid() && source.isEmpty(); }
};

//! Dark-frame calibration configuration and reference frames.
struct DarkSettings
{
    bool enabled = false;
    DarkMethod method = DarkMethod::MatchedDark;

    //! Exposure of the matched dark frame (seconds).
    double matchedExposure = 0.0;
    //! Exposure of the long dark frame t0 (seconds).
    double longDarkExposure = 0.0;
    //! Exposure of the frame being corrected t (seconds), entered by the user.
    double targetExposure = 0.0;
    //! Constant added after calibration to keep the result positive.
    int outputOffset = 0;

    ReferenceFrame matchedDark;
    ReferenceFrame bias;
    ReferenceFrame longDark;
};

//! Flat-field calibration configuration and reference frames.
struct FlatSettings
{
    bool enabled = false;
    FlatMethod method = FlatMethod::Classic;

    //! Exposure of the classic flat frame (seconds). The dark model is
    //! evaluated at this exposure before normalization.
    double flatExposure = 0.0;

    //! Pixels below this fraction of the median response are marked unusable.
    double lowResponseFraction = 0.2;
    //! Upper clamp for the gain map, so a near-zero response cannot explode.
    double maxGain = 5.0;

    ReferenceFrame flat;
    //! One entry per exposure for VariableExposure. Entries must be sorted or
    //! at least have distinct positive exposures; build functions sort a copy.
    QVector<ReferenceFrame> vffEntries;
};

//! Normalized multiplicative flat-field map, one gain per sample.
struct FlatField
{
    int width = 0;
    int height = 0;
    int channels = 0;
    QVector<float> gain;

    //! Diagnostics for the dialog/status line.
    int excludedSamples = 0;
    double meanSignal = 0.0;
    float minGain = 1.0f;
    float maxGain = 1.0f;

    bool isValid() const
    {
        return width > 0 && height > 0 && channels > 0
            && gain.size() == width * height * channels;
    }
};

//! Convert a QImage to an interleaved float plane. Returns an invalid plane on
//! failure and fills @p error when it is non-null.
FramePlane planeFromImage(const QImage &image, QString *error = nullptr);

//! Build the classic mean-normalized flat field from `F - D(t_flat)`.
//!
//! The dark model comes from @p dark. With `BiasScaled` any flat exposure is
//! valid; with `MatchedDark` the flat exposure must equal the matched dark
//! exposure. When dark calibration is disabled the raw flat is normalized.
//! On failure returns an invalid field and writes the reason to @p error.
FlatField buildClassicFlat(const FlatSettings &settings, const DarkSettings &dark,
                           QString *error = nullptr);

//! Build the variable-exposure flat field from at least two exposures.
//!
//! Each entry is dark-corrected at its own exposure before the per-pixel fit.
//! When dark calibration is enabled this therefore requires `BiasScaled`,
//! because `MatchedDark` only describes one exposure.
FlatField buildVariableExposureFlat(const FlatSettings &settings, const DarkSettings &dark,
                                    QString *error = nullptr);

//! Apply dark subtraction, flat multiplication and the output offset in place.
//!
//! @p flat may be null when flat calibration is disabled. Returns false and
//! writes a human-readable reason to @p reason when a requested correction
//! cannot be applied (missing reference, size mismatch, non-positive exposure,
//! unsupported image format). When both corrections are disabled the frame is
//! left untouched and true is returned.
bool apply(ImageData &frame, const DarkSettings &dark, const FlatField *flat,
           QString *reason = nullptr);

//! Build the dark estimate for an arbitrary exposure without touching a frame.
//!
//! `BiasScaled` can synthesize any exposure from B, D(t0), t0 and the requested
//! time. `MatchedDark` is only valid at `matchedExposure`, so flat construction
//! can reuse it only when the flat exposure matches. Returns false and writes a
//! reason when the model cannot supply that exposure.
bool buildDarkPlane(const DarkSettings &settings, double exposure,
                    int width, int height, int channels, int maxValue,
                    QVector<float> &darkPlane, QString *reason = nullptr);

} // namespace Calibration
