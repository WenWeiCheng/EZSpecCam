#include "HistoryCsvExporter.h"

#include <QDateTime>
#include <QFile>
#include <QTextStream>

namespace HistoryCsvExporter
{

bool write(const QString &filePath, const QVector<ParameterSample> &samples,
           qint64 originTimestampMs, QString *error)
{
    if (samples.isEmpty()) {
        if (error) {
            *error = QStringLiteral("There are no samples to export.");
        }
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) {
            *error = QStringLiteral("Cannot open %1 for writing: %2")
                         .arg(filePath, file.errorString());
        }
        return false;
    }

    QTextStream stream(&file);
    stream << "elapsed_s,timestamp,value\n";

    for (const ParameterSample &sample : samples) {
        // The 1 Hz sampling timer makes the millisecond part jitter (656.001 /
        // 656.999); one decimal is what the curve actually guarantees. The exact
        // instant is in the timestamp column.
        stream << QString::number((sample.timestampMs - originTimestampMs) / 1000.0, 'f', 1)
               << ','
               << QDateTime::fromMSecsSinceEpoch(sample.timestampMs).toString(Qt::ISODateWithMs)
               << ','
               << QString::number(sample.value, 'g', 12)
               << '\n';
    }

    stream.flush();
    if (file.error() != QFileDevice::NoError) {
        if (error) {
            *error = QStringLiteral("Failed to write %1: %2")
                         .arg(filePath, file.errorString());
        }
        return false;
    }

    return true;
}

} // namespace HistoryCsvExporter
