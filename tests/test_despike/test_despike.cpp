#include <QObject>
#include <QTest>
#include <QVector>
#include <cmath>

#include "PostProcess.h"

namespace {

// 确定性伪随机噪声：不用 rand()，每次跑出来的谱一模一样。
//
// 均匀噪声的一次差分是三角分布，|Z| 最大约 2.3，碰不到主阈值 3.5，所以
// 「干净谱线一个点都不该动」才是能稳定断言的；同理它也不会误触次阈值 2.5
QVector<double> noisyBaseline(int size, double level, double amplitude, unsigned seed)
{
    QVector<double> y(size);
    unsigned state = seed;
    for (int i = 0; i < size; ++i) {
        state = state * 1664525u + 1013904223u;
        const double unit = static_cast<double>(state >> 8) / 16777216.0;
        y[i] = level + (unit - 0.5) * 2.0 * amplitude;
    }
    return y;
}

PostProcess::DespikeParams params(double primaryThreshold, double secondaryThreshold,
                                 int windowHalfWidth, int passes)
{
    PostProcess::DespikeParams p;
    p.primaryThreshold = primaryThreshold;
    p.secondaryThreshold = secondaryThreshold;
    p.windowHalfWidth = windowHalfWidth;
    p.passes = passes;
    return p;
}

int countMarked(const QVector<bool> &marked)
{
    int count = 0;
    for (const bool value : marked) {
        if (value) {
            ++count;
        }
    }
    return count;
}

} // namespace

class TestDespike : public QObject
{
    Q_OBJECT

private slots:
    void test_clean_spectrum_untouched();
    void test_single_spike_replaced_by_unmarked_mean();
    void test_two_pixel_blob_not_marked();
    void test_negative_dip_not_marked();
    void test_short_spectrum_untouched();
    void test_constant_spectrum_untouched();
    void test_z_scores_follow_input();
    void test_marked_count_matches_mask();
    void test_secondary_threshold_blocks_supported_spike();
    void test_flat_baseline_spike_replaced();
    void test_zero_passes_behaves_like_one();
    void test_narrow_window_has_unmarked_neighbour();
};

void TestDespike::test_clean_spectrum_untouched()
{
    QVector<double> y = noisyBaseline(512, 1000.0, 20.0, 12345u);
    // 一条宽谱带：上升沿的斜率远小于噪声的一次差分，不该被当成尖峰
    for (int i = 0; i < y.size(); ++i) {
        const double t = (i - 300) / 15.0;
        y[i] += 200.0 * std::exp(-0.5 * t * t);
    }

    const PostProcess::DespikeResult result = PostProcess::despike(y, params(3.5, 2.5, 5, 2));

    QCOMPARE(result.markedCount, 0);
    QCOMPARE(result.marked.size(), y.size());
    QCOMPARE(result.values, y);
    for (const bool marked : result.marked) {
        QVERIFY(!marked);
    }
}

void TestDespike::test_single_spike_replaced_by_unmarked_mean()
{
    constexpr int n = 256;
    constexpr int halfWidth = 5;
    constexpr int spikeIndex = 50;

    QVector<double> y = noisyBaseline(n, 1000.0, 20.0, 777u);
    y[spikeIndex] += 4000.0;

    const PostProcess::DespikeResult result =
        PostProcess::despike(y, params(3.5, 2.5, halfWidth, 1));

    QCOMPARE(result.markedCount, 1);
    QVERIFY(result.marked[spikeIndex]);

    // 替换值是窗口内未标记点的均值（窗口含两端，越界向内收）
    double sum = 0.0;
    int count = 0;
    for (int i = spikeIndex - halfWidth; i <= spikeIndex + halfWidth; ++i) {
        if (i < 0 || i >= n || result.marked[i]) {
            continue;
        }
        sum += y[i];
        ++count;
    }
    QVERIFY(count > 0);
    const double expected = sum / count;
    QVERIFY2(std::abs(result.values[spikeIndex] - expected) < 1e-6,
             qPrintable(QStringLiteral("replacement %1 != expected %2")
                            .arg(result.values[spikeIndex])
                            .arg(expected)));

    // 其余点逐个与输入相同：只改写被标记的点，不做任何平滑
    for (int i = 0; i < n; ++i) {
        if (i == spikeIndex) {
            continue;
        }
        QCOMPARE(result.values[i], y[i]);
    }
}

void TestDespike::test_two_pixel_blob_not_marked()
{
    QVector<double> y = noisyBaseline(256, 1000.0, 20.0, 99u);
    // 两像素宽的亮斑：差分的段跨度是 3，不是单像素尖峰的 2
    y[60] += 4000.0;
    y[61] += 4000.0;

    const PostProcess::DespikeResult result = PostProcess::despike(y, params(3.5, 2.5, 5, 2));

    QCOMPARE(result.markedCount, 0);
    QCOMPARE(result.values, y);
}

void TestDespike::test_negative_dip_not_marked()
{
    // 参考实现只识别「先向上再向下」的正向脉冲（差分形状 0 +A -A 0）；
    // 向下的凹陷形状正好相反。这里如实沿用参考实现的行为，免得移植时
    // 悄悄换掉判据
    QVector<double> y = noisyBaseline(256, 1000.0, 20.0, 4242u);
    y[80] -= 4000.0;

    const PostProcess::DespikeResult result = PostProcess::despike(y, params(3.5, 2.5, 5, 2));

    QCOMPARE(result.markedCount, 0);
    QCOMPARE(result.values, y);
}

void TestDespike::test_short_spectrum_untouched()
{
    for (const int size : {0, 1, 2}) {
        QVector<double> y = noisyBaseline(size, 1000.0, 20.0, 1u);
        if (size >= 2) {
            // 就算有尖峰，少于 3 个点也凑不出「一正一负」的差分对
            y[size - 1] += 5000.0;
        }

        const PostProcess::DespikeResult result = PostProcess::despike(y, params(3.5, 2.5, 5, 2));

        QCOMPARE(result.values.size(), size);
        QCOMPARE(result.markedCount, 0);
        QCOMPARE(result.values, y);
    }
}

void TestDespike::test_constant_spectrum_untouched()
{
    // 差分 MAD 为 0：整条 Z 分数是 NaN，一个点都不该标记，也不能除零
    QVector<double> y(128, 2500.0);

    const PostProcess::DespikeResult result = PostProcess::despike(y, params(3.5, 2.5, 5, 2));

    QCOMPARE(result.markedCount, 0);
    QCOMPARE(result.values, y);
    QCOMPARE(result.zScores.size(), y.size());
    for (const double z : result.zScores) {
        QVERIFY(qIsNaN(z));
    }
}

void TestDespike::test_z_scores_follow_input()
{
    QVector<double> y = noisyBaseline(64, 1000.0, 20.0, 5u);
    y[32] += 4000.0;

    const PostProcess::DespikeResult result = PostProcess::despike(y, params(3.5, 2.5, 5, 2));

    QCOMPARE(result.zScores.size(), y.size());
    // 第 0 个点没有差分项
    QVERIFY(qIsNaN(result.zScores[0]));
    // 尖峰的两个差分是全谱最大的两次跳变，一正一负
    QVERIFY(result.zScores[32] > 3.5);
    QVERIFY(result.zScores[33] < -3.5);
}

void TestDespike::test_marked_count_matches_mask()
{
    QVector<double> y = noisyBaseline(256, 1000.0, 20.0, 31337u);
    y[40] += 4000.0;
    y[120] += 3000.0;

    const PostProcess::DespikeResult result = PostProcess::despike(y, params(3.5, 2.5, 5, 2));

    QCOMPARE(result.markedCount, countMarked(result.marked));
    QCOMPARE(result.markedCount, 2);
    QVERIFY(result.marked[40]);
    QVERIFY(result.marked[120]);
}

void TestDespike::test_secondary_threshold_blocks_supported_spike()
{
    constexpr int n = 256;
    constexpr int spikeIndex = 50;
    // 主阈值 3.5 / 次阈值 2.5 是算法默认值。噪声幅度 20 对应差分 MAD 约 11.7，
    // 台阶 B = 52 的一次差分 Z 分数约 3.0：够不着主阈值，却超过次阈值
    constexpr double stepHeight = 52.0;

    QVector<double> y = noisyBaseline(n, 1000.0, 20.0, 2024u);

    // 尖峰左右 4 个点写成精确台阶，免得更靠近的两侧噪声把这一段的差分
    // 推过主阈值（那会改变差分段跨度，测的就不是次阈值这条判据了）
    y[47] = 1000.0;
    y[48] = 1000.0;
    y[49] = 1000.0 + stepHeight;                 // 差分 +B：尖峰外侧的第一个差分
    y[50] = 1000.0 + stepHeight + 4000.0;        // 差分 +A：尖峰上升
    y[51] = 1000.0 + stepHeight;                 // 差分 -A：尖峰回落
    y[52] = 1000.0 + stepHeight;                 // 差分 0：外侧的第二个差分
    y[53] = 1000.0;                              // 差分 -B
    y[54] = 1000.0;

    const PostProcess::DespikeResult blocked =
        PostProcess::despike(y, params(3.5, 2.5, 5, 1));
    QCOMPARE(blocked.markedCount, 0);

    // 次阈值 0 = 关掉这条判据，同一个尖峰就会被替换
    const PostProcess::DespikeResult unchecked =
        PostProcess::despike(y, params(3.5, 0.0, 5, 1));
    QCOMPARE(unchecked.markedCount, 1);
    QVERIFY(unchecked.marked[spikeIndex]);
}

void TestDespike::test_flat_baseline_spike_replaced()
{
    // 平坦基线上超过一半的差分恰好是 0，MAD 退化为 0：这时没有可用的尺度
    // 估计，按参考实现的语义把非零差分记作 ±inf。任何非零跳变都算离群，
    // 单点尖峰仍然要被摘掉——不然一个没噪声的合成谱上开了去尖峰毫无反应
    QVector<double> y(128, 1000.0);
    y[64] = 6000.0;

    const PostProcess::DespikeResult result = PostProcess::despike(y, params(3.5, 2.5, 5, 2));

    QCOMPARE(result.markedCount, 1);
    QVERIFY(result.marked[64]);
    // 窗口内未标记点全是 1000，替换值就是 1000
    QCOMPARE(result.values[64], 1000.0);
    QVERIFY(std::isinf(result.zScores[64]));
    QVERIFY(std::isinf(result.zScores[65]));
    QVERIFY(result.zScores[65] < 0.0);
    for (int i = 0; i < y.size(); ++i) {
        if (i == 64) {
            continue;
        }
        QCOMPARE(result.values[i], y[i]);
    }
}

void TestDespike::test_zero_passes_behaves_like_one()
{
    // passes < 1 会被夹到 1：不处理等于什么都没做，但也不能死循环
    QVector<double> y = noisyBaseline(256, 1000.0, 20.0, 777u);
    y[50] += 4000.0;

    const PostProcess::DespikeResult one = PostProcess::despike(y, params(3.5, 2.5, 5, 1));
    const PostProcess::DespikeResult zero = PostProcess::despike(y, params(3.5, 2.5, 5, 0));

    QCOMPARE(zero.markedCount, one.markedCount);
    QCOMPARE(zero.values, one.values);
    QCOMPARE(zero.marked, one.marked);
}

void TestDespike::test_narrow_window_has_unmarked_neighbour()
{
    // 窗口半宽 1 时窗口里只有左右邻居和它自己；标记点不会彼此相邻，所以
    // 至少还有一个未标记点，替换值就是两侧的均值。emptyWindowCount 那条
    // 防御路径（邻域内全是标记点）在正常输入下走不到
    QVector<double> y = noisyBaseline(64, 1000.0, 20.0, 8u);
    y[32] += 4000.0;

    const PostProcess::DespikeResult result = PostProcess::despike(y, params(3.5, 2.5, 1, 1));

    QCOMPARE(result.markedCount, 1);
    QCOMPARE(result.emptyWindowCount, 0);
    QVERIFY(std::isfinite(result.values[32]));
    QCOMPARE(result.values[32], 0.5 * (y[31] + y[33]));
}

QTEST_APPLESS_MAIN(TestDespike)

#include "test_despike.moc"
