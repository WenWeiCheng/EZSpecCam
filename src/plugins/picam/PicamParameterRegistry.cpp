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
        switch (param) {
        case PicamParameter_ExposureTime:             return kPoint1;
        case PicamParameter_SensorTemperatureReading: return kZeroD;
        case PicamParameter_SensorTemperatureSetPoint:return kNeg75;
        case PicamParameter_ShutterClosingDelay:      return kZeroD;
        case PicamParameter_VerticalShiftRate:        return kZeroD;
        case PicamParameter_PixelWidth:               return kZeroD;
        case PicamParameter_PixelHeight:              return kZeroD;
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
    {PicamParameter_AdcSpeed,                 "adc_speed",         "ADC Speed",                  "Controls the rate pixels are digitized, in MHz.",                           ParameterCategory::Core,     400.0f},
    {PicamParameter_AdcQuality,               "adc_quality",       "ADC Quality",                "Controls the nature of pixel digitization via the PicamAdcQuality data enumeration.",               ParameterCategory::Core,     500.0f},
    {PicamParameter_PixelFormat,              "pixel_format",      "Pixel Format",               "Controls the format of a data pixel via the PicamPixelFormat data enumeration.",                          ParameterCategory::Core,     600.0f},
    {PicamParameter_PixelBitDepth,            "bit_depth",         "Pixel Bit Depth",            "Reports the size of a data pixel in bits-per-pixel.",                             ParameterCategory::Info,     68.0f},
    {PicamParameter_SensorTemperatureReading, "sensor_temperature","Sensor Temperature",         "Reports the temperature of the sensor in degrees C.",       ParameterCategory::Cooling,  800.0f},
    {PicamParameter_SensorTemperatureSetPoint,"temperature_setpoint","Temperature Setpoint",       "Controls the target temperature for the sensor in degrees C.",        ParameterCategory::Cooling,  810.0f},
    {PicamParameter_SensorTemperatureStatus,  "temperature_status","Temperature Status",         "Reports the status of the sensor temperature via the PicamSensorTemperatureStatus data enumeration.",                               ParameterCategory::Info,     820.0f},
    {PicamParameter_SensorActiveWidth,        "sensor_width",      "Sensor Width",               "Reports the active number of columns.",                      ParameterCategory::Info,     50.0f},
    {PicamParameter_SensorActiveHeight,       "sensor_height",     "Sensor Height",              "Reports the active number of rows.",                     ParameterCategory::Info,     51.0f},
    {PicamParameter_SensorActiveExtendedHeight,"sensor_extended_height","Sensor Extended Height","Reports the number of additional active rows that can be used for storage. NOTE: These rows cannot be imaged directly.",      ParameterCategory::Info,     52.0f},
    {PicamParameter_SensorSecondaryActiveHeight,"sensor_secondary_height","Sensor Secondary Height","Reports the number of secondary active rows.",  ParameterCategory::Info,     53.0f},
    {PicamParameter_SensorActiveLeftMargin,   "sensor_left_margin","Sensor Left Margin",         "Reports the inactive columns on the left.",         ParameterCategory::Info,     54.0f},
    {PicamParameter_SensorActiveRightMargin,  "sensor_right_margin","Sensor Right Margin",       "Reports the inactive columns on the right.",        ParameterCategory::Info,     55.0f},
    {PicamParameter_SensorActiveTopMargin,    "sensor_top_margin", "Sensor Top Margin",          "Reports the inactive rows on the top.",          ParameterCategory::Info,     56.0f},
    {PicamParameter_SensorActiveBottomMargin, "sensor_bottom_margin","Sensor Bottom Margin",     "Reports the inactive rows on the bottom.",       ParameterCategory::Info,     57.0f},
    {PicamParameter_SensorMaskedHeight,       "sensor_masked_height","Sensor Masked Height",     "Reports the number of masked rows akin to active height.",              ParameterCategory::Info,     58.0f},
    {PicamParameter_SensorMaskedTopMargin,    "sensor_masked_top", "Sensor Masked Top",          "Reports the number of masked rows akin to active top margin.",          ParameterCategory::Info,     59.0f},
    {PicamParameter_SensorMaskedBottomMargin, "sensor_masked_bottom","Sensor Masked Bottom",     "Reports the number of masked rows akin to active bottom margin.",       ParameterCategory::Info,     60.0f},
    {PicamParameter_SensorSecondaryMaskedHeight,"sensor_secondary_masked_height","Sensor Secondary Masked Height","Reports the number of secondary masked rows.",ParameterCategory::Info,61.0f},
    {PicamParameter_SensorType,               "sensor_type",       "Sensor Type",                "Reports the kind of sensor being used via the PicamSensorType data enumeration.",                          ParameterCategory::Info,     62.0f},
    {PicamParameter_CcdCharacteristics,       "ccd_chars",         "CCD Characteristics",        "Reports characteristics of a CCD sensor via the PicamCcdCharacteristicsMask data enumeration.",                  ParameterCategory::Info,     63.0f},
    {PicamParameter_Orientation,              "orientation",       "Orientation",                "Reports the orientation of the data via the PicamOrientationMask data enumeration.",                 ParameterCategory::Info,     64.0f},
    {PicamParameter_ReadoutOrientation,       "readout_orientation","Readout Orientation",       "Reports the orientation of the data due to readout port location via the PicamOrientationMask data enumeration.",                    ParameterCategory::Info,     65.0f},
    {PicamParameter_PixelWidth,               "pixel_width",       "Pixel Width",                "Reports the pixel width, in microns.",                      ParameterCategory::Info,     66.0f},
    {PicamParameter_PixelHeight,              "pixel_height",      "Pixel Height",               "Reports the pixel height, in microns.",                     ParameterCategory::Info,     67.0f},
    {PicamParameter_ReadoutControlMode,       "readout_mode",      "Readout Mode",               "Controls how the sensor is read out via the PicamReadoutControlMode data enumeration.",                         ParameterCategory::Core,     800.0f},
    {PicamParameter_TriggerResponse,          "trigger_response",  "Trigger Response",           "Controls the hardware's behavior in response to a trigger via the PicamTriggerResponse data enumeration.",                       ParameterCategory::Core,     900.0f},
    {PicamParameter_TriggerDetermination,     "trigger_determination","Trigger Determination",   "Controls what the hardware recognizes as an external trigger via the PicamTriggerDetermination data enumeration.",                     ParameterCategory::Core,     910.0f},
    {PicamParameter_OutputSignal,             "output_signal",     "Output Signal",              "Controls what timing signal is issued from the hardware monitor via the PicamOutputSignal data enumeration.",                     ParameterCategory::Core,     920.0f},
    {PicamParameter_ShutterTimingMode,        "shutter_mode",      "Shutter Mode",               "Controls the behavior of the shutter during acquisition.",                         ParameterCategory::Advanced, 1010.0f},
    {PicamParameter_ShutterClosingDelay,      "shutter_delay",     "Shutter Delay",              "Controls the duration of time the hardware waits for the shutter to close before reading out. The time unit depends on PicamParameter_ShutterDelayResolution.",                 ParameterCategory::Advanced, 1020.0f},
    {PicamParameter_VerticalShiftRate,        "vertical_shift_rate","Vertical Shift Rate",       "Controls the rate to shift one row towards the serial register in a CCD in microseconds (us).",              ParameterCategory::Advanced, 1030.0f},
    {PicamParameter_ActiveWidth,              "active_width",      "Active Width",               "Controls the active number of columns.",       ParameterCategory::Advanced, 1040.0f},
    {PicamParameter_ActiveHeight,             "active_height",     "Active Height",              "Controls the active number of rows.",      ParameterCategory::Advanced, 1050.0f},
    {PicamParameter_ActiveLeftMargin,         "active_left",       "Active Left",                "Controls the inactive number of columns on the left.",           ParameterCategory::Advanced, 1060.0f},
    {PicamParameter_ActiveRightMargin,        "active_right",      "Active Right",               "Controls the inactive number of columns on the right.",          ParameterCategory::Advanced, 1070.0f},
    {PicamParameter_ActiveTopMargin,          "active_top",        "Active Top",                 "Controls the inactive number of rows on the top.",            ParameterCategory::Advanced, 1080.0f},
    {PicamParameter_ActiveBottomMargin,       "active_bottom",     "Active Bottom",              "Controls the inactive number of rows on the bottom.",         ParameterCategory::Advanced, 1090.0f},
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
