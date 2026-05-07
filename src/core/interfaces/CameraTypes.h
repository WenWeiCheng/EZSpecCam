#pragma once

/**
 * @file CameraTypes.h
 * @brief Core data structures for EZSpecCam camera interface
 *
 * @see ICameraDriver for the camera driver interface
 */

#include <QString>
#include <QStringList>
#include <QImage>
#include <QVector>
#include <QVariant>
#include <QJsonObject>

//==============================================================================
// Forward Declarations
//==============================================================================

struct ROI;
struct PixelBinning;
struct VerticalBinning;
struct ParameterConstraint;
struct ParameterDefinition;
struct ImageData;

//==============================================================================
// Enums
//==============================================================================

/**
 * @brief Camera connection and operational state
 */
enum class CameraState {
    Disconnected,  ///< Camera is not connected
    Connecting,    ///< Connection in progress
    Connected,     ///< Camera is connected and idle
    Acquiring,     ///< Actively capturing frames
    Error          ///< Error state
};

/**
 * @brief Supported image file formats for saving
 */
enum class ImageFormat {
    TIFF,
    JPEG
};

/**
 * @brief Parameter value type classification
 */
enum class ParameterType {
    FloatRange,        ///< Floating-point range (min, max, step)
    FloatCollection,   ///< Discrete floating-point values
    IntRange,          ///< Integer range (min, max, step)
    IntCollection,     ///< Discrete integer values
    String,            ///< Single string value
    StringCollection,  ///< Discrete string values
    Boolean            ///< Boolean (true/false)
};

/**
 * @brief Parameter grouping category for UI organization
 */
enum class ParameterCategory {
    Core,     ///< Core acquisition parameters (exposure, gain)
    Cooling,  ///< Temperature control parameters
    Info,     ///< Read-only informational parameters
    Advanced, ///< Advanced/hidden parameters
    Debug     ///< Debug parameters
};

/**
 * @brief Metadata embedding format for saved images
 */
enum class MetaDataSaveFormat {
    Separate,  ///< Metadata saved as separate JSON file
    Embedded   ///< Metadata embedded in image file
};

//==============================================================================
// Basic Types
//==============================================================================

/**
 * @brief Region of Interest (ROI) configuration for selective image capture
 *
 * Defines a rectangular sub-region of the sensor to use for acquisition.
 * Origin (0, 0) is at the top-left corner. Coordinates are in pixels.
 *
 * @note Drivers should validate ROI bounds in setConfig()
 */
struct ROI
{
    int x = 0;
    int y = 0;
    int width = 0;
    int height = 0;

    bool isValid() const { return width > 0 && height > 0; }

    bool operator==(const ROI &other) const
    {
        return x == other.x && y == other.y &&
               width == other.width && height == other.height;
    }

    bool operator!=(const ROI &other) const { return !(*this == other); }
};

/**
 * @brief Pixel binning configuration for combining adjacent pixels
 *
 * Hardware pixel binning combines adjacent pixels during readout to improve
 * signal-to-noise ratio at the cost of reduced resolution.
 *
 * Supported factors: 1, 2, 4, 8
 */
struct PixelBinning
{
    int factor = 1;

    bool isValid() const { return factor == 1 || factor == 2 || factor == 4 || factor == 8; }

    bool operator==(const PixelBinning &other) const { return factor == other.factor; }
    bool operator!=(const PixelBinning &other) const { return !(*this == other); }
};

/**
 * @brief Vertical binning for 1D spectral output
 *
 * Sums all pixel values along the vertical axis within the ROI,
 * converting a 2D image into a 1D spectrum.
 */
struct VerticalBinning
{
    bool enabled = false;

    bool operator==(const VerticalBinning &other) const { return enabled == other.enabled; }
    bool operator!=(const VerticalBinning &other) const { return !(*this == other); }
};

//==============================================================================
// Parameter System Types
//==============================================================================

/**
 * @brief Constraint definition for parameter values
 *
 * Defines valid range or set of values for a parameter.
 * Usage depends on ParameterType:
 * - FloatRange/IntRange: minValue, maxValue, step
 * - FloatCollection/IntCollection/StringCollection: validValues
 * - String/Boolean: no constraints
 */
struct ParameterConstraint
{
    double minValue = 0;
    double maxValue = 0;
    double step = 0;
    QVector<QVariant> validValues;
    QStringList unit;
    QVector<double> unitRange;

    bool hasUnitRange() const { return !unit.isEmpty() && unitRange.size() == unit.size() - 1; }

    int getUnitIndex(double rawValue) const
    {
        if (unitRange.isEmpty()) return 0;
        for (int i = 0; i < unitRange.size(); ++i) {
            if (rawValue < unitRange[i]) return i;
        }
        return unitRange.size();
    }

    double toDisplayValue(double rawValue, int unitIndex) const
    {
        if (unitIndex == 0 || unitRange.isEmpty()) return rawValue;
        if (unitIndex > 0 && unitIndex <= unitRange.size()) {
            return rawValue / unitRange[unitIndex - 1];
        }
        return rawValue;
    }

    double toRawValue(double displayValue, int unitIndex) const
    {
        if (unitIndex == 0 || unitRange.isEmpty()) return displayValue;
        if (unitIndex > 0 && unitIndex <= unitRange.size()) {
            return displayValue * unitRange[unitIndex - 1];
        }
        return displayValue;
    }

    bool isValid() const { return minValue <= maxValue; }

    bool operator==(const ParameterConstraint &other) const
    {
        return minValue == other.minValue && maxValue == other.maxValue &&
               step == other.step && validValues == other.validValues &&
               unit == other.unit && unitRange == other.unitRange;
    }

    bool operator!=(const ParameterConstraint &other) const { return !(*this == other); }
};

/**
 * @brief Complete parameter definition for dynamic parameter system
 *
 * Defines a camera parameter including identity, display properties,
 * category, type, constraints, and default value.
 */
struct ParameterDefinition
{
    QString name;
    QString displayName;
    QString description;
    ParameterCategory category;
    ParameterType type;
    ParameterConstraint constraint;
    QVariant defaultValue;
    bool isReadOnly = false;
    bool isDynamic = false;
    bool isExtrinsic = false;
    bool needReconnect = false;
    float order = 0.0f;

    bool isValid() const { return !name.isEmpty() && constraint.isValid(); }

    bool operator==(const ParameterDefinition &other) const
    {
        return name == other.name && displayName == other.displayName &&
               description == other.description && category == other.category &&
               type == other.type && constraint == other.constraint &&
               defaultValue == other.defaultValue && isReadOnly == other.isReadOnly &&
               isDynamic == other.isDynamic && isExtrinsic == other.isExtrinsic &&
               needReconnect == other.needReconnect && order == other.order;
    }

    bool operator!=(const ParameterDefinition &other) const { return !(*this == other); }
};

//==============================================================================
// Parameter Validation (free functions)
//==============================================================================

inline bool validate(const QVariant &value,
                     const ParameterConstraint &constraint,
                     ParameterType type)
{
    switch (type) {
    case ParameterType::FloatRange: {
        double val = value.toDouble();
        return val >= constraint.minValue && val <= constraint.maxValue;
    }
    case ParameterType::FloatCollection: {
        double val = value.toDouble();
        for (const QVariant &v : constraint.validValues) {
            if (qAbs(v.toDouble() - val) < 0.0001) {
                return true;
            }
        }
        return false;
    }
    case ParameterType::IntRange: {
        int val = value.toInt();
        return val >= constraint.minValue && val <= constraint.maxValue;
    }
    case ParameterType::IntCollection: {
        int val = value.toInt();
        for (const QVariant &v : constraint.validValues) {
            if (v.toInt() == val) {
                return true;
            }
        }
        return false;
    }
    case ParameterType::String: {
        return value.canConvert<QString>();
    }
    case ParameterType::StringCollection: {
        QString val = value.toString();
        for (const QVariant &v : constraint.validValues) {
            if (v.toString() == val) {
                return true;
            }
        }
        return false;
    }
    case ParameterType::Boolean: {
        return value.canConvert<bool>();
    }
    default:
        return true;
    }
}

inline QString validateReason(const QVariant &value,
                               const ParameterConstraint &constraint,
                               ParameterType type)
{
    switch (type) {
    case ParameterType::FloatRange: {
        double val = value.toDouble();
        if (val < constraint.minValue) {
            return QString("Value %1 is below minimum %2").arg(val).arg(constraint.minValue);
        }
        if (val > constraint.maxValue) {
            return QString("Value %1 is above maximum %2").arg(val).arg(constraint.maxValue);
        }
        return QString();
    }
    case ParameterType::FloatCollection: {
        double val = value.toDouble();
        bool found = false;
        for (const QVariant &v : constraint.validValues) {
            if (qAbs(v.toDouble() - val) < 0.0001) {
                found = true;
                break;
            }
        }
        if (!found) {
            return QString("Value %1 is not in valid collection").arg(val);
        }
        return QString();
    }
    case ParameterType::IntRange: {
        int val = value.toInt();
        if (val < constraint.minValue) {
            return QString("Value %1 is below minimum %2").arg(val).arg(static_cast<int>(constraint.minValue));
        }
        if (val > constraint.maxValue) {
            return QString("Value %1 is above maximum %2").arg(val).arg(static_cast<int>(constraint.maxValue));
        }
        return QString();
    }
    case ParameterType::IntCollection: {
        int val = value.toInt();
        bool found = false;
        for (const QVariant &v : constraint.validValues) {
            if (v.toInt() == val) {
                found = true;
                break;
            }
        }
        if (!found) {
            return QString("Value %1 is not in valid collection").arg(val);
        }
        return QString();
    }
    case ParameterType::String: {
        if (!value.canConvert<QString>()) {
            return QString("Value cannot be converted to string");
        }
        return QString();
    }
    case ParameterType::StringCollection: {
        QString val = value.toString();
        bool found = false;
        for (const QVariant &v : constraint.validValues) {
            if (v.toString() == val) {
                found = true;
                break;
            }
        }
        if (!found) {
            return QString("Value '%1' is not in valid collection").arg(val);
        }
        return QString();
    }
    case ParameterType::Boolean: {
        if (!value.canConvert<bool>()) {
            return QString("Value cannot be converted to boolean");
        }
        return QString();
    }
    default:
        return QString();
    }
}

//==============================================================================
// Error Handling
//==============================================================================

/**
 * @brief Camera error information structure
 *
 * Encapsulates detailed error information including classification,
 * severity level, affected parameter, and recovery capability.
 */
struct CameraError
{
    enum class Code {
        None = 0,
        InvalidParameter,
        ValueOutOfRange,
        CommitFailed,
        HardwareFault,
        NotConnected,
        NotSupported,
        ConnectionFailed,
        CaptureFailed,
        Timeout,
        BufferOverflow,
        StateInvalid,
        DriverError,
        PluginLoadFailed,
        CommunicationError
    };

    enum class Severity {
        Info,
        Warning,
        Error,
        Fatal
    };

    Code code = Code::None;
    Severity severity = Severity::Info;
    QString parameterName;
    QString description;
    bool recoverable = true;

    bool hasError() const { return code != Code::None; }

    static CameraError success() { return CameraError(); }

    static CameraError makeError(Code errorCode, const QString &errorDescription,
                                 Severity errorSeverity = Severity::Error)
    {
        CameraError error;
        error.code = errorCode;
        error.description = errorDescription;
        error.severity = errorSeverity;
        return error;
    }

    bool operator==(const CameraError &other) const
    {
        return code == other.code && severity == other.severity &&
               parameterName == other.parameterName && description == other.description &&
               recoverable == other.recoverable;
    }

    bool operator!=(const CameraError &other) const { return !(*this == other); }
};

//==============================================================================
// Image and Frame Data
//==============================================================================

/**
 * @brief Image data container with capture metadata
 *
 * Primary data structure returned by frame acquisition methods.
 * Timestamp is recorded at frame availability from driver (ns since epoch).
 * Frame numbers are sequential within a session, reset on disconnect.
 */
struct ImageData
{
    QImage image;
    QImage originalImage;
    quint64 timestamp = 0;
    int frameNumber = 0;
    QString cameraId;
    QVariantMap parameters;
    QVariantMap config;

    bool isValid() const { return !image.isNull() && timestamp > 0; }
    bool hasOriginal() const { return !originalImage.isNull(); }
};

/**
 * @brief Metadata associated with a captured frame
 */
struct FrameMetadata
{
    QString cameraId;
    double exposure = 0.0;
    double gain = 0.0;
    ROI roi;
    double temperature = 0.0;
    quint64 timestamp = 0;
    int frameNumber = 0;
    QString softwareVersion;

    QJsonObject toJson() const;
};

//==============================================================================
// Configuration and Capabilities
//==============================================================================

/**
 * @brief Cooling system configuration
 */
struct CoolingConfig {
    bool enabled = false;
    double targetTemperature = -10.0;
};

/**
 * @brief Camera configuration for acquisition settings
 */
struct CameraConfig {
    QString mode;
    double exposure = 100.0;
    double gain = 0.0;
    double offset = 0.0;
    int burstFrameCount = 1;
    ROI roi;
    PixelBinning pixelBinning;
    VerticalBinning verticalBinning;
    CoolingConfig cooling;
};

/**
 * @brief Camera hardware capabilities and limits
 */
struct CameraCapabilities {
    int maxWidth = 2048;
    int maxHeight = 2048;
    int bitDepth = 16;
    double minExposure = 1.0;
    double maxExposure = 1000.0;
    double exposureStep = 1.0;
    bool hardwareSupportsROI = true;
    bool hardwareSupportsPixelBinning = true;
    bool hardwareSupportsVerticalBinning = true;
    bool hardwareSupportedLiveMode = true;
    QVector<int> hardwareSupportedPixelBinningFactors;

    CameraCapabilities() : hardwareSupportedPixelBinningFactors({1, 2, 4, 8}) {}
};

//==============================================================================
// Save Options
//==============================================================================

/**
 * @brief Image saving options and format configuration
 */
struct ImageSaveOptions {
    ImageFormat format = ImageFormat::TIFF;
    int quality = 100;
    QString compression = "None";

    QString fileExtension() const {
        switch (format) {
        case ImageFormat::TIFF: return "tiff";
        case ImageFormat::JPEG: return "jpg";
        }
        return "tiff";
    }

    bool operator==(const ImageSaveOptions &other) const {
        return format == other.format && quality == other.quality &&
               compression == other.compression;
    }
};

/**
 * @brief Frame saving options with metadata configuration
 */
struct FrameSaveOptions : public ImageSaveOptions {
    MetaDataSaveFormat frameFormat = MetaDataSaveFormat::Separate;
    bool includeCoolingData = true;
    bool includeTimestamp = true;
    bool includeRoiInfo = true;

    bool operator==(const FrameSaveOptions &other) const {
        return ImageSaveOptions::operator==(other) &&
               frameFormat == other.frameFormat &&
               includeCoolingData == other.includeCoolingData &&
               includeTimestamp == other.includeTimestamp &&
               includeRoiInfo == other.includeRoiInfo;
    }
};

//==============================================================================
// MetaType Declarations (for Qt signals/slots)
//==============================================================================

Q_DECLARE_METATYPE(ImageFormat)
Q_DECLARE_METATYPE(ImageSaveOptions)
Q_DECLARE_METATYPE(MetaDataSaveFormat)
Q_DECLARE_METATYPE(FrameSaveOptions)
Q_DECLARE_METATYPE(ParameterType)
Q_DECLARE_METATYPE(ParameterCategory)
Q_DECLARE_METATYPE(ParameterConstraint)
Q_DECLARE_METATYPE(ParameterDefinition)
Q_DECLARE_METATYPE(CameraError)
Q_DECLARE_METATYPE(CameraError::Code)
Q_DECLARE_METATYPE(CameraError::Severity)
Q_DECLARE_METATYPE(CameraState)

//==============================================================================
// Inline Method Implementations
//==============================================================================

inline QJsonObject FrameMetadata::toJson() const
{
    QJsonObject obj;
    obj["cameraId"] = cameraId;
    obj["exposure"] = exposure;
    obj["gain"] = gain;
    obj["temperature"] = temperature;
    obj["timestamp"] = static_cast<qint64>(timestamp);
    obj["frameNumber"] = frameNumber;
    obj["softwareVersion"] = softwareVersion;

    QJsonObject roiObj;
    roiObj["x"] = roi.x;
    roiObj["y"] = roi.y;
    roiObj["width"] = roi.width;
    roiObj["height"] = roi.height;
    obj["roi"] = roiObj;

    return obj;
}