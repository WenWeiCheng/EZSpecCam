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
    QString format = "tiff";
    QString prefix;
    QString suffix;
    QVector<SequenceStep> sequence;
};

/// Run a headless session. Returns 0 on success, 1 on user error, -1 on internal error.
int run(const HeadlessOptions &opts);

}
