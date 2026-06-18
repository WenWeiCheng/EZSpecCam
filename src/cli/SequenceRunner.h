#pragma once

#include <QString>
#include <QVector>
#include <QVariantMap>

struct SequenceStep
{
    enum Type { Configure, Capture, WaitStable };
    Type type = Configure;

    QVariantMap parameters;
    int frames = 1;

    QString stableParam;
    double stableTarget = 0.0;
    double stableTolerance = 0.1;
    double stableWindowSec = 10.0;
    double stableTimeoutSec = 300.0;

    QString outputDir;
    QString format;
    QString prefix;
    QString suffix;
};

/**
 * @brief Parses a JSON event sequence file.
 *
 * Expected JSON format:
 * @code
 * {
 *   "settings": { "output": "./", "format": "tiff", ... },
 *   "steps": [
 *     { "configure": { "exposure": 100 } },
 *     { "capture": { "frames": 10 } },
 *     { "wait_stable": { "parameter": "cooling_sensor_temp", "target": -20, ... } },
 *     { "capture": { "frames": 5 } }
 *   ]
 * }
 * @endcode
 *
 * 注：save_metadata 字段已废弃（被忽略），metadata 总是随帧一起保存。
 */
class SequenceRunner
{
public:
    bool loadFromFile(const QString &filePath);

    const QVector<SequenceStep> &steps() const { return m_steps; }
    QString errorString() const { return m_error; }

    QString defaultOutputDir;
    QString defaultFormat;
    QString defaultPrefix;
    QString defaultSuffix;

private:
    QVector<SequenceStep> m_steps;
    QString m_error;
};
