#pragma once

#include <QString>
#include <QVariantMap>
#include <QVector>

#include "SequenceRunner.h"

namespace app
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

}