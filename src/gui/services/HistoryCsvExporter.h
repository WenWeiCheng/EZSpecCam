#ifndef HISTORYCSVEXPORTER_H
#define HISTORYCSVEXPORTER_H

#include <QString>
#include <QVector>

#include "ParameterHistoryStore.h"

//! Write a ParameterHistoryStore sample window to CSV.
//!
//! Kept out of ParameterHistoryWindow so the file format can be tested without
//! constructing a QDialog.
namespace HistoryCsvExporter
{

//! @p originTimestampMs is the timestamp of the first sample still in the
//! window; the elapsed_s column is relative to it. Returns false and fills
//! @p error on failure.
bool write(const QString &filePath, const QVector<ParameterSample> &samples,
           qint64 originTimestampMs, QString *error = nullptr);

} // namespace HistoryCsvExporter

#endif // HISTORYCSVEXPORTER_H
