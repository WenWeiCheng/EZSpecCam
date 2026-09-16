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
//==============================================================================
// Forward Declarations
//==============================================================================

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

    bool hasUnitRange() const { return !unit.isEmpty() && !unitRange.isEmpty() && unitRange.size() == unit.size() - 1; }

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
            return rawValue / unitRange[unitIndex-1];
        }
        return rawValue;
    }

    double toRawValue(double displayValue, int unitIndex) const
    {
        if (unitIndex == 0 || unitRange.isEmpty()) return displayValue;
        if (unitIndex > 0 && unitIndex <= unitRange.size()) {
            return displayValue * unitRange[unitIndex-1];
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

// Forward declaration for validate function (defined later in file)
bool validate(const QVariant &value, const ParameterConstraint &constraint, ParameterType type);

/**
 * @brief Complete parameter definition for dynamic parameter system
 *
 * Defines a camera parameter including identity, display properties,
 * category, type, constraints, and default value.
 */
struct ParameterDefinition
{
    QString name;                      // Unique identifier for the parameter (e.g., "exposure", "gain")
    QString displayName;               // User-friendly name for UI display 
    QString description;               // Detailed description for tooltips or documentation
    ParameterCategory category;
    ParameterType type;
    ParameterConstraint constraint;
    QVariant defaultValue;
    bool isReadOnly = false;
    bool isDynamic = false;            // True if parameter can be changed with other params, environment, time changing
    bool isExtrinsic = false;          // True if parameter change with environment or time changing, but cannot be changed by user directly
    bool needReconnect = false;        // True if parameter change need to reconnect
    float order = 10000000.0f;         // Order of parameter in GUI, lower number means higher priority

    bool isValid() const {
        // name, displayName, description must be non-empty
        if (name.isEmpty() || displayName.isEmpty() || description.isEmpty()) return false;
        // For non-info, writable parameters, default value and constraint must be valid
        if (!isReadOnly && defaultValue.isNull() && !constraint.isValid()) return false;
        // Validate default value against constraints for applicable categories
        if(!isReadOnly && category != ParameterCategory::Info){
            return validate(defaultValue, constraint, type);
        }
        return true;
    }

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
// Parameter Clamping (free functions)
//==============================================================================

/**
 * @brief Result of a clampValue() call.
 */
struct ClampResult
{
    QVariant value;       ///< Clamped value; QVariant() if rejected.
    bool changed = false; ///< True iff the output differs from the input.
    QString reason;       ///< Human-readable explanation (for qWarning), empty when !changed.
};

/**
 * @brief Snap an arbitrary user input into a parameter's valid domain.
 *
 * Mirrors the GUI's widget-level silent-clamp behaviour: out-of-range Range
 * values are qBound() to [min,max] then snapped to the nearest step; Collection
 * values that are not in validValues are rejected (QVariant() returned);
 * Boolean and String values are type-coerced.
 *
 * Gate order (return {QVariant(), false, reason} on first match):
 *   1. def.name.isEmpty()        -> "unknown parameter"
 *   2. def.isReadOnly            -> "parameter is read-only"
 *   3. !def.isValid()            -> "invalid parameter definition"
 *
 * @param raw  User-supplied value (from CLI argv or JSON).
 * @param def  Parameter metadata from driver->parameter(name).
 * @return     ClampResult describing the corrected value (or rejection).
 */
inline ClampResult clampValue(const QVariant &raw,
                              const ParameterDefinition &def)
{
    if (def.name.isEmpty())
        return {QVariant(), false, QStringLiteral("unknown parameter")};
    if (def.isReadOnly)
        return {QVariant(), false, QStringLiteral("parameter is read-only")};
    if (!def.isValid())
        return {QVariant(), false, QStringLiteral("invalid parameter definition")};

    switch (def.type) {
    case ParameterType::FloatRange: {
        double min = def.constraint.minValue;
        double max = def.constraint.maxValue;
        double step = def.constraint.step;
        if (qFuzzyCompare(step, 0.0)) step = 1.0;
        double rawVal = raw.toDouble();
        double bounded = qBound(min, rawVal, max);
        double stepped = min + qRound((bounded - min) / step) * step;
        if (stepped > max) stepped = max;
        bool changed = !qFuzzyCompare(1.0 + rawVal, 1.0 + stepped);
        QString reason;
        if (changed) {
            if (rawVal < min)      reason = QString("clamped from %1 to %2").arg(rawVal).arg(min);
            else if (rawVal > max) reason = QString("clamped from %1 to %2").arg(rawVal).arg(max);
            else                   reason = QString("stepped from %1 to %2 (step=%3)")
                                              .arg(rawVal).arg(stepped).arg(step);
        }
        return {QVariant(stepped), changed, reason};
    }
    case ParameterType::IntRange: {
        double min = def.constraint.minValue;
        double max = def.constraint.maxValue;
        double step = def.constraint.step > 0 ? def.constraint.step : 1.0;
        double rawVal = raw.toDouble();
        double bounded = qBound(min, rawVal, max);
        double stepped = min + qRound((bounded - min) / step) * step;
        if (stepped > max) stepped = max;
        bool changed = (rawVal != stepped);
        QString reason;
        if (changed) {
            if (rawVal < min)      reason = QString("clamped from %1 to %2").arg(rawVal).arg(min);
            else if (rawVal > max) reason = QString("clamped from %1 to %2").arg(rawVal).arg(max);
            else                   reason = QString("stepped from %1 to %2 (step=%3)")
                                              .arg(rawVal).arg(stepped).arg(step);
        }
        return {QVariant(static_cast<int>(stepped)), changed, reason};
    }
    case ParameterType::FloatCollection: {
        double val = raw.toDouble();
        for (const QVariant &v : def.constraint.validValues) {
            if (qAbs(v.toDouble() - val) < 0.0001)
                return {raw, false, QString()};
        }
        QStringList opts;
        for (const QVariant &v : def.constraint.validValues)
            opts << QString::number(v.toDouble());
        return {QVariant(), false,
                QString("invalid value %1; must be one of [%2]")
                    .arg(val).arg(opts.join(", "))};
    }
    case ParameterType::IntCollection: {
        int val = raw.toInt();
        for (const QVariant &v : def.constraint.validValues) {
            if (v.toInt() == val)
                return {raw, false, QString()};
        }
        QStringList opts;
        for (const QVariant &v : def.constraint.validValues)
            opts << QString::number(v.toInt());
        return {QVariant(), false,
                QString("invalid value %1; must be one of [%2]")
                    .arg(val).arg(opts.join(", "))};
    }
    case ParameterType::StringCollection: {
        QString val = raw.toString();
        for (const QVariant &v : def.constraint.validValues) {
            if (v.toString() == val)
                return {raw, false, QString()};
        }
        QStringList opts;
        for (const QVariant &v : def.constraint.validValues)
            opts << v.toString();
        return {QVariant(), false,
                QString("invalid value '%1'; must be one of [%2]")
                    .arg(val).arg(opts.join(", "))};
    }
    // Boolean and String cases handled in Task 4.
    default:
        return {raw, false, QString()};
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
    QStringList failedParameters;  ///< Names of parameters that failed during a batched set/commit
    QString description;
    bool recoverable = true;

    bool hasError() const { return code != Code::None; }

    static CameraError success() { return CameraError(); }

    static CameraError makeError(Code errorCode, const QString &errorDescription,
                                 Severity errorSeverity = Severity::Error,
                                 QStringList failedParameters = {})
    {
        CameraError error;
        error.code = errorCode;
        error.description = errorDescription;
        error.severity = errorSeverity;
        error.failedParameters = std::move(failedParameters);
        return error;
    }

    bool operator==(const CameraError &other) const
    {
        return code == other.code && severity == other.severity &&
               failedParameters == other.failedParameters && description == other.description &&
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
    QVector<quint64> spectrum;  ///< Full-precision vertical binning sums (1D spectrum)
    quint64 timestamp = 0;
    int frameNumber = 0;
    QString cameraId;
    QVariantMap parameters;
    QVariantMap softwareSettings;  ///< Software-side state at save time (vertical binning, row range, etc.)

    bool isValid() const { return !image.isNull() && timestamp > 0; }
    bool hasOriginal() const { return !originalImage.isNull(); }
};

//==============================================================================
// MetaType Declarations (for Qt signals/slots)
//==============================================================================

Q_DECLARE_METATYPE(ParameterType)
Q_DECLARE_METATYPE(ParameterCategory)
Q_DECLARE_METATYPE(ParameterConstraint)
Q_DECLARE_METATYPE(ParameterDefinition)
Q_DECLARE_METATYPE(CameraError)
Q_DECLARE_METATYPE(CameraError::Code)
Q_DECLARE_METATYPE(CameraError::Severity)
Q_DECLARE_METATYPE(CameraState)