#pragma once

#include <QImage>
#include <QVector>

struct ImageData;

namespace PostProcess
{

void verticalBinning(ImageData &frame, int startRow, int endRow);
void applyDarkCalibration(ImageData &frame, const QImage *darkFrame, int customBias);
} // namespace PostProcess
