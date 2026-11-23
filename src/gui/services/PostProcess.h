#pragma once

#include <QImage>
#include <QVector>

struct ImageData;

namespace PostProcess
{

void verticalBinning(ImageData &frame, int startRow, int endRow);
void applyDarkCalibration(ImageData &frame, const QImage *darkFrame, int customBias);

//! 去尖峰（despike）的参数。谱线和图像现在走同一个二维算法，只剩主阈值
struct DespikeParams
{
    //! 主阈值：像素比周围 8 个邻居中的最大值高出多少个噪声尺度才算尖峰
    double primaryThreshold = 3.5;
};

//! 图像去尖峰的结果
struct ImageDespikeResult
{
    //! 被改写的像素个数（RGB888 下同一像素多个通道被改写也只算一次）
    int markedCount = 0;
    //! 参与检测的像素个数，不含边界一圈；格式不支持、图太小或行范围为空时为 0
    int sampleCount = 0;
};

//! 3×3 孤立像素去尖峰，返回处理后的副本，入参不动。
//!
//! 判据：一个像素比周围 8 个邻居全都高出 primaryThreshold 个噪声尺度才算尖峰，
//! 替换值取上下左右 4 个邻居的中值。要求「比 8 个邻居都高」是为了只摘掉真正
//! 孤立的单像素脉冲：竖直/水平/斜向的一像素宽亮线、两像素宽的亮斑、成片的
//! 亮区都不会被改写。
//!
//! 噪声尺度按行估计（相邻像素差分绝对值的中位数），因此亮谱带里被散粒噪声
//! 抬高的行会用自己的尺度，不会被暗背景的尺度误判。
//!
//! firstRow / lastRow 是 0-based 闭区间的行范围，只有范围内的行参与检测与改写；
//! lastRow < 0 表示最后一行。范围以外的行原样复制，不补做去尖峰，也不参与
//! sampleCount。范围的首尾行仍然使用范围外的相邻行做 3×3 邻域，所以边界上的
//! 孤立点不会被漏掉。图像上下边界（第 0 行和最后一行）没有完整邻域，始终不改写。
//!
//! 支持 Grayscale8 / Grayscale16 / RGB888，其余格式原样返回。
QImage despikeImage(const QImage &image, const DespikeParams &params = DespikeParams(),
                    ImageDespikeResult *result = nullptr,
                    int firstRow = 0, int lastRow = -1);
} // namespace PostProcess
