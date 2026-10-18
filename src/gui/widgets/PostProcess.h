#pragma once

#include <QImage>
#include <QVector>

struct ImageData;

namespace PostProcess
{

void verticalBinning(ImageData &frame, int startRow, int endRow);
void applyDarkCalibration(ImageData &frame, const QImage *darkFrame, int customBias);

//! 去尖峰（despike）的参数。只有光谱线用得到，图像不经过这条路
//!
//! 判据只看一次差分的形状，因此只识别「先升后降」的正向单点脉冲：向下的
//! 凹陷与两像素以上的宽特征都不会被改写。谱线一半以上的差分恰好相等时
//! （平坦基线）MAD 退化，此时任何非零差分都算离群，单点尖峰仍会被替换
struct DespikeParams
{
    //! 主阈值：一次差分修正 Z 分数的门限，超过它才算尖峰候选
    double primaryThreshold = 3.5;
    //! 次阈值：尖峰外侧两个差分的上限；<= 0 表示关闭这条判据
    double secondaryThreshold = 2.5;
    //! 均值窗口半宽：替换值取 [i-m, i+m] 内未标记点的均值
    int windowHalfWidth = 5;
    //! 迭代遍数：每遍都在修补后的数据上重算差分
    int passes = 2;
};

//! 去尖峰的结果。只有被标记的点被改写，其余点与输入逐位相同，不做任何平滑
struct DespikeResult
{
    QVector<double> values;
    QVector<bool> marked;
    //! 输入数据一次差分的修正 Z 分数，第 0 个点没有差分项，记为 NaN；
    //! MAD 退化为 0 时非零差分记作 ±inf
    QVector<double> zScores;
    int markedCount = 0;
    //! 邻域内全是标记点、取不到均值的次数
    int emptyWindowCount = 0;
};

DespikeResult despike(const QVector<double> &y, const DespikeParams &params = DespikeParams());
} // namespace PostProcess
