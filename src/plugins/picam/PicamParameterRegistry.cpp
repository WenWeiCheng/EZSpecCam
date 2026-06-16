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
    {PicamParameter_ExposureTime,             "exposure",          "Exposure Time",              "Camera exposure time in seconds",             ParameterCategory::Core,     100.0f},
    {PicamParameter_AdcBitDepth,              "adc_bit_depth",     "ADC Bit Depth",              "ADC conversion bit depth",                    ParameterCategory::Core,     250.0f},
    {PicamParameter_AdcAnalogGain,            "analog_gain",       "Analog Gain",                "Camera analog gain setting",                  ParameterCategory::Core,     300.0f},
    {PicamParameter_AdcSpeed,                 "adc_speed",         "ADC Speed",                  "ADC readout speed",                           ParameterCategory::Core,     400.0f},
    {PicamParameter_AdcQuality,               "adc_quality",       "ADC Quality",                "ADC quality vs speed tradeoff",               ParameterCategory::Core,     500.0f},
    {PicamParameter_PixelFormat,              "pixel_format",      "Pixel Format",               "Image pixel format",                          ParameterCategory::Core,     600.0f},
    {PicamParameter_PixelBitDepth,            "bit_depth",         "Pixel Bit Depth",            "Image bit depth",                             ParameterCategory::Info,     68.0f},
    {PicamParameter_SensorTemperatureReading, "sensor_temperature","Sensor Temperature",         "Current sensor temperature in Celsius",       ParameterCategory::Cooling,  800.0f},
    {PicamParameter_SensorTemperatureSetPoint,"temperature_setpoint","Temperature Setpoint",       "Target sensor temperature in Celsius",        ParameterCategory::Cooling,  810.0f},
    {PicamParameter_SensorTemperatureStatus,  "temperature_status","Temperature Status",         "Cooler status",                               ParameterCategory::Info,     820.0f},
    {PicamParameter_SensorActiveWidth,        "sensor_width",      "Sensor Width",               "Sensor width in pixels",                      ParameterCategory::Info,     50.0f},
    {PicamParameter_SensorActiveHeight,       "sensor_height",     "Sensor Height",              "Sensor height in pixels",                     ParameterCategory::Info,     51.0f},
    {PicamParameter_SensorActiveExtendedHeight,"sensor_extended_height","Sensor Extended Height","Sensor active extended height in pixels",      ParameterCategory::Info,     52.0f},
    {PicamParameter_SensorSecondaryActiveHeight,"sensor_secondary_height","Sensor Secondary Height","Sensor secondary active height in pixels",  ParameterCategory::Info,     53.0f},
    {PicamParameter_SensorActiveLeftMargin,   "sensor_left_margin","Sensor Left Margin",         "Sensor active left margin in pixels",         ParameterCategory::Info,     54.0f},
    {PicamParameter_SensorActiveRightMargin,  "sensor_right_margin","Sensor Right Margin",       "Sensor active right margin in pixels",        ParameterCategory::Info,     55.0f},
    {PicamParameter_SensorActiveTopMargin,    "sensor_top_margin", "Sensor Top Margin",          "Sensor active top margin in pixels",          ParameterCategory::Info,     56.0f},
    {PicamParameter_SensorActiveBottomMargin, "sensor_bottom_margin","Sensor Bottom Margin",     "Sensor active bottom margin in pixels",       ParameterCategory::Info,     57.0f},
    {PicamParameter_SensorMaskedHeight,       "sensor_masked_height","Sensor Masked Height",     "Sensor masked height in pixels",              ParameterCategory::Info,     58.0f},
    {PicamParameter_SensorMaskedTopMargin,    "sensor_masked_top", "Sensor Masked Top",          "Sensor masked top margin in pixels",          ParameterCategory::Info,     59.0f},
    {PicamParameter_SensorMaskedBottomMargin, "sensor_masked_bottom","Sensor Masked Bottom",     "Sensor masked bottom margin in pixels",       ParameterCategory::Info,     60.0f},
    {PicamParameter_SensorSecondaryMaskedHeight,"sensor_secondary_masked_height","Sensor Secondary Masked Height","Sensor secondary masked height in pixels",ParameterCategory::Info,61.0f},
    {PicamParameter_SensorType,               "sensor_type",       "Sensor Type",                "Camera sensor type",                          ParameterCategory::Info,     62.0f},
    {PicamParameter_CcdCharacteristics,       "ccd_chars",         "CCD Characteristics",        "CCD sensor characteristics",                  ParameterCategory::Info,     63.0f},
    {PicamParameter_Orientation,              "orientation",       "Orientation",                "Physical sensor orientation",                 ParameterCategory::Info,     64.0f},
    {PicamParameter_ReadoutOrientation,       "readout_orientation","Readout Orientation",       "Readout port orientation",                    ParameterCategory::Info,     65.0f},
    {PicamParameter_PixelWidth,               "pixel_width",       "Pixel Width",                "Pixel width in microns",                      ParameterCategory::Info,     66.0f},
    {PicamParameter_PixelHeight,              "pixel_height",      "Pixel Height",               "Pixel height in microns",                     ParameterCategory::Info,     67.0f},
    {PicamParameter_ReadoutControlMode,       "readout_mode",      "Readout Mode",               "Camera readout mode",                         ParameterCategory::Core,     800.0f},
    {PicamParameter_TriggerResponse,          "trigger_response",  "Trigger Response",           "Trigger response mode",                       ParameterCategory::Core,     900.0f},
    {PicamParameter_TriggerDetermination,     "trigger_determination","Trigger Determination",   "Trigger signal polarity",                     ParameterCategory::Core,     910.0f},
    {PicamParameter_OutputSignal,             "output_signal",     "Output Signal",              "Output signal selection",                     ParameterCategory::Core,     920.0f},
    {PicamParameter_ShutterTimingMode,        "shutter_mode",      "Shutter Mode",               "Shutter timing mode",                         ParameterCategory::Advanced, 1010.0f},
    {PicamParameter_ShutterClosingDelay,      "shutter_delay",     "Shutter Delay",              "Shutter closing delay in ms",                 ParameterCategory::Advanced, 1020.0f},
    {PicamParameter_VerticalShiftRate,        "vertical_shift_rate","Vertical Shift Rate",       "Vertical shift speed in us/row",              ParameterCategory::Advanced, 1030.0f},
    {PicamParameter_ActiveWidth,              "active_width",      "Active Width",               "Sensor readout active width in pixels",       ParameterCategory::Advanced, 1040.0f},
    {PicamParameter_ActiveHeight,             "active_height",     "Active Height",              "Sensor readout active height in pixels",      ParameterCategory::Advanced, 1050.0f},
    {PicamParameter_ActiveLeftMargin,         "active_left",       "Active Left",                "Active area left margin in pixels",           ParameterCategory::Advanced, 1060.0f},
    {PicamParameter_ActiveRightMargin,        "active_right",      "Active Right",               "Active area right margin in pixels",          ParameterCategory::Advanced, 1070.0f},
    {PicamParameter_ActiveTopMargin,          "active_top",        "Active Top",                 "Active area top margin in pixels",            ParameterCategory::Advanced, 1080.0f},
    {PicamParameter_ActiveBottomMargin,       "active_bottom",     "Active Bottom",              "Active area bottom margin in pixels",         ParameterCategory::Advanced, 1090.0f},
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
