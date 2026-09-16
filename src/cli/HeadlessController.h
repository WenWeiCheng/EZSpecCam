#pragma once

#include <QString>
#include <QVariantMap>
#include <QVector>

#include "SequenceRunner.h"

class ICameraDriver;

namespace cli
{

struct HeadlessOptions
{
    bool listCameras = false;
    bool listParams = false;
    QString cameraId;
    QVariantMap setParameters;
    int frames = 1;
    QString outputDir = ".";
    QString outputExtension = "tiff";   ///< file extension ("tiff"/"csv"), already translated by caller
    QString prefix;
    QString suffix;
    QVector<SequenceStep> sequence;
};

/// Run a headless session. Returns 0 on success, 1 on user error, -1 on internal error.
int run(const HeadlessOptions &opts);

/**
 * @brief Capture @p frameCount frames from @p driver into @p outputDir.
 *
 * Drives startCapture(), blocks on a QEventLoop until all frames arrive or an
 * error/early-stop interrupts acquisition, then calls stopCapture(). All signal
 * connections use the local QEventLoop as their context object so they are
 * torn down automatically when this function returns — this prevents stale
 * connections from previous calls racing into the next capture.
 *
 * @return Number of frames captured on success; negative on startup failure.
 */
int captureFrames(ICameraDriver *driver, int frameCount,
                  const QString &outputDir, const QString &outputExtension,
                  const QString &prefix, const QString &suffix);

}