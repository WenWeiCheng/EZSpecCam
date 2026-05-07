#ifndef POSTPROCESS_H
#define POSTPROCESS_H

#include <QImage>
#include <QFlags>

#include "CameraTypes.h"

namespace PostProcess {

enum Operation {
    None = 0x00,
    VerticalBinning = 0x01,
    DarkFrameSubtraction = 0x02,
    FlatFieldCorrection = 0x04,
    CosmicRayRemoval = 0x08
};
Q_DECLARE_FLAGS(Operations, Operation)

struct ProcessConfig {
    bool enabled = false;
    Operations operations = None;
    int vBinStartRow = 0;
    int vBinEndRow = -1;
    QImage darkFrame;
    QImage flatField;
};

void processFrame(ImageData &frame, const ProcessConfig &config);

} // namespace PostProcess

Q_DECLARE_OPERATORS_FOR_FLAGS(PostProcess::Operations)

#endif