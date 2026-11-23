#ifndef FILENAMING_H
#define FILENAMING_H

#include <QDateTime>
#include <QString>

#include "Calibration.h"

//! Pure file-name builders shared by the Save As / auto-save / reference paths.
namespace FileNaming
{

//! "prefix_img_<yyyyMMdd_hhmmss_zzz>suffix.ext"; empty prefix/suffix are
//! skipped. @p imageFormat is the format name shown in CameraConfigDialog ("TIFF"/"CSV").
QString frameFileName(const QString &prefix, const QString &suffix,
                      const QString &imageFormat, const QDateTime &timestamp);

//! Stable ASCII stem used for reference file names and save metadata.
QString referenceStem(Calibration::ReferenceKind kind);

//! "<stem>_<exposure>s_avg<N>_<yyyyMMdd_hhmmss>".
QString referenceFileName(const QString &stem,
                          const Calibration::ReferenceFrame &reference,
                          const QDateTime &timestamp);
} // namespace FileNaming

#endif // FILENAMING_H
