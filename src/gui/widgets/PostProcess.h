#pragma once

#include <QImage>
#include <QVector>

struct ImageData;

namespace PostProcess
{

void verticalBinning(ImageData &frame, int startRow, int endRow);

} // namespace PostProcess
