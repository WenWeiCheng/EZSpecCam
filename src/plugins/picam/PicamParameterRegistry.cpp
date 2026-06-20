#include "PicamParameterRegistry.h"

namespace {

struct RawEntry {
    PicamParameter param;
    const char *name;
    const char *displayName;
    const char *description;
    ParameterCategory category;
    const QVariant &fallbackDefault() const {
        static const QVariant kNone;
        static const QVariant kZeroI(0);
        static const QVariant kZeroD(0.0);
        static const QVariant kPoint1(0.1);
        static const QVariant kNeg75(-75.0);
        static const QVariant kOne(1);
        static const QVariant kTrue(true);
        switch (param) {
        case PicamParameter_ExposureTime:             return kPoint1;
        case PicamParameter_SensorTemperatureReading: return kZeroD;
        case PicamParameter_SensorTemperatureSetPoint:return kNeg75;
        case PicamParameter_ShutterClosingDelay:      return kZeroD;
        case PicamParameter_VerticalShiftRate:        return kZeroD;
        case PicamParameter_PixelWidth:               return kZeroD;
        case PicamParameter_PixelHeight:              return kZeroD;
        case PicamParameter_CleanCycleCount:          return kOne;
        case PicamParameter_CleanSerialRegister:      return kTrue;
        case PicamParameter_CleanBeforeExposure:      return kTrue;
        default:
            if (category == ParameterCategory::Core
                || category == ParameterCategory::Advanced) {
                return kZeroI;
            }
            return kNone;
        }
    }
    float order;
};

constexpr RawEntry kRawEntries[] = {
    {PicamParameter_ExposureTime,             "exposure",          "Exposure Time",              "Controls the time the sensor is exposed in milliseconds (mS).",             ParameterCategory::Core,     100.0f},
    {PicamParameter_AdcBitDepth,              "adc_bit_depth",     "ADC Bit Depth",              "Controls the resolution of the pixel digitization in bits-per-pixel.",                    ParameterCategory::Core,     250.0f},
    {PicamParameter_AdcAnalogGain,            "analog_gain",       "Analog Gain",                "Controls the electronic gain of the pixel digitization via the PicamAdcAnalogGain data enumeration.",                  ParameterCategory::Core,     300.0f},
    {PicamParameter_AdcSpeed,                 "adc_speed",         "ADC Speed (Mhz)",                  "Controls the rate pixels are digitized, in MHz.",                           ParameterCategory::Core,     400.0f},
    {PicamParameter_AdcQuality,               "adc_quality",       "ADC Quality",                "Controls the nature of pixel digitization via the PicamAdcQuality data enumeration.",               ParameterCategory::Core,     500.0f},
    {PicamParameter_PixelFormat,              "pixel_format",      "Pixel Format",               "Controls the format of a data pixel via the PicamPixelFormat data enumeration.",                          ParameterCategory::Core,     600.0f},
    {PicamParameter_PixelBitDepth,            "bit_depth",         "Pixel Bit Depth",            "Reports the size of a data pixel in bits-per-pixel.",                             ParameterCategory::Info,     68.0f},
    {PicamParameter_SensorTemperatureReading, "sensor_temperature","Sensor Temperature",         "Reports the temperature of the sensor in degrees C.",       ParameterCategory::Cooling,  800.0f},
    {PicamParameter_SensorTemperatureSetPoint,"temperature_setpoint","Temperature Setpoint",       "Controls the target temperature for the sensor in degrees C.",        ParameterCategory::Cooling,  810.0f},
    {PicamParameter_SensorTemperatureStatus,  "temperature_status","Temperature Status",         "Reports the status of the sensor temperature via the PicamSensorTemperatureStatus data enumeration.",                               ParameterCategory::Cooling,     820.0f},
    {PicamParameter_SensorActiveWidth,        "sensor_width",      "Sensor Width",               "Reports the active number of columns.",                      ParameterCategory::Info,     50.0f},
    {PicamParameter_SensorActiveHeight,       "sensor_height",     "Sensor Height",              "Reports the active number of rows.",                     ParameterCategory::Info,     51.0f},
    {PicamParameter_SensorType,               "sensor_type",       "Sensor Type",                "Reports the kind of sensor being used via the PicamSensorType data enumeration.",                          ParameterCategory::Info,     62.0f},
    {PicamParameter_CcdCharacteristics,       "ccd_chars",         "CCD Characteristics",        "Reports characteristics of a CCD sensor via the PicamCcdCharacteristicsMask data enumeration.",                  ParameterCategory::Info,     63.0f},
    {PicamParameter_PixelWidth,               "pixel_width",       "Pixel Width (um)",                "Reports the pixel width, in microns.",                      ParameterCategory::Info,     66.0f},
    {PicamParameter_PixelHeight,              "pixel_height",      "Pixel Height (um)",               "Reports the pixel height, in microns.",                     ParameterCategory::Info,     67.0f},
    {PicamParameter_CleanCycleCount,          "clean_cycle_count", "Clean Cycle Count",          "Controls the number of clean cycles to run before acquisition begins.", ParameterCategory::Advanced, 310.0f},
    {PicamParameter_CleanSerialRegister,      "clean_serial_register", "Clean Serial Register",      "Controls the cleaning of the serial register itself.",                    ParameterCategory::Advanced, 320.0f},
    {PicamParameter_CleanBeforeExposure,      "clean_before_exposure", "Clean Before Exposure",      "Controls cleaning before each exposure.",                                ParameterCategory::Advanced, 330.0f},
};

QHash<PicamParameter, PicamParameterRecord> buildRegistry()
{
    QHash<PicamParameter, PicamParameterRecord> reg;
    for (const auto &e : kRawEntries) {
        PicamParameterRecord rec;
        rec.param = e.param;
        rec.name = QString::fromLatin1(e.name);
        rec.displayName = QString::fromLatin1(e.displayName);
        rec.description = QString::fromLatin1(e.description);
        rec.category = e.category;
        rec.fallbackDefault = e.fallbackDefault();
        rec.order = e.order;
        reg.insert(e.param, rec);
    }
    return reg;
}

QHash<QString, PicamParameter> buildReverseMap()
{
    QHash<QString, PicamParameter> map;
    for (const auto &e : kRawEntries) {
        map.insert(QString::fromLatin1(e.name), e.param);
    }
    return map;
}

} // anonymous namespace

const QHash<PicamParameter, PicamParameterRecord> &picamParameterRegistry()
{
    static const QHash<PicamParameter, PicamParameterRecord> s_reg = buildRegistry();
    return s_reg;
}

const QHash<QString, PicamParameter> &picamReverseNameMap()
{
    static const QHash<QString, PicamParameter> s_map = buildReverseMap();
    return s_map;
}

const PicamParameterRecord *findByPicamParam(PicamParameter param)
{
    const auto &reg = picamParameterRegistry();
    auto it = reg.constFind(param);
    return it != reg.constEnd() ? &it.value() : nullptr;
}

const PicamParameterRecord *findByName(const QString &name)
{
    const auto &reverseMap = picamReverseNameMap();
    auto rit = reverseMap.constFind(name);
    if (rit == reverseMap.constEnd()) {
        return nullptr;
    }
    return findByPicamParam(rit.value());
}

QString picamParamName(PicamParameter param)
{
    const auto *rec = findByPicamParam(param);
    return rec ? rec->name : QString();
}

PicamParameter picamParamFromName(const QString &name)
{
    const auto &reverseMap = picamReverseNameMap();
    auto it = reverseMap.constFind(name);
    return it != reverseMap.constEnd() ? it.value() : static_cast<PicamParameter>(-1);
}

ParameterCategory picamParamCategory(PicamParameter param)
{
    const auto *rec = findByPicamParam(param);
    return rec ? rec->category : ParameterCategory::Core;
}
