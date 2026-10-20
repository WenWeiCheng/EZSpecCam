#include <QImage>
#include <QObject>
#include <QTest>

#include "PostProcess.h"

namespace {

PostProcess::DespikeParams params(double primaryThreshold = 3.5)
{
    PostProcess::DespikeParams p;
    p.primaryThreshold = primaryThreshold;
    return p;
}

// 确定性伪随机噪声：不用 rand()，每次跑出来的图一模一样
QImage noisyGray16Image(int width, int height, int level, double amplitude, unsigned seed)
{
    QImage image(width, height, QImage::Format_Grayscale16);
    unsigned state = seed;
    for (int y = 0; y < height; ++y) {
        ushort *line = reinterpret_cast<ushort *>(image.scanLine(y));
        for (int x = 0; x < width; ++x) {
            state = state * 1664525u + 1013904223u;
            const double unit = static_cast<double>(state >> 8) / 16777216.0;
            line[x] = static_cast<ushort>(level + (unit - 0.5) * 2.0 * amplitude);
        }
    }
    return image;
}

QImage flatGray16Image(int width, int height, ushort level)
{
    QImage image(width, height, QImage::Format_Grayscale16);
    for (int y = 0; y < height; ++y) {
        ushort *line = reinterpret_cast<ushort *>(image.scanLine(y));
        for (int x = 0; x < width; ++x) {
            line[x] = level;
        }
    }
    return image;
}

ushort pixel16(const QImage &image, int x, int y)
{
    const ushort *line = reinterpret_cast<const ushort *>(image.constScanLine(y));
    return line[x];
}

void setPixel16(QImage &image, int x, int y, ushort value)
{
    ushort *line = reinterpret_cast<ushort *>(image.scanLine(y));
    line[x] = value;
}

} // namespace

class TestDespike : public QObject
{
    Q_OBJECT

private slots:
    void test_image_single_spike_replaced();
    void test_image_noise_untouched();
    void test_image_vertical_line_preserved();
    void test_image_two_pixel_blob_preserved();
    void test_image_ramp_untouched();
    void test_image_border_untouched();
    void test_image_grayscale8_spike_replaced();
    void test_image_rgb_spike_replaced();
    void test_image_short_side_untouched();
    void test_image_row_range_only_processes_inside();
    void test_image_row_range_uses_outside_neighbours();
    void test_image_row_range_clamps_and_accepts_last_row_marker();
    void test_image_row_range_empty();
};

void TestDespike::test_image_single_spike_replaced()
{
    constexpr int width = 32;
    constexpr int height = 32;
    constexpr int spikeX = 16;
    constexpr int spikeY = 16;

    QImage image = flatGray16Image(width, height, 1000);
    setPixel16(image, spikeX, spikeY, 6000);

    PostProcess::ImageDespikeResult result;
    const QImage output = PostProcess::despikeImage(image, params(), &result);

    QCOMPARE(result.markedCount, 1);
    QCOMPARE(result.sampleCount, (width - 2) * (height - 2));
    // 平坦背景的噪声尺度为 0，替换值就是四个直接邻居的中值 1000
    QCOMPARE(pixel16(output, spikeX, spikeY), static_cast<ushort>(1000));
    QCOMPARE(pixel16(output, spikeX - 1, spikeY), static_cast<ushort>(1000));
    QCOMPARE(output.size(), image.size());
    QCOMPARE(output.format(), image.format());

    // 输入不能被就地改写
    QCOMPARE(pixel16(image, spikeX, spikeY), static_cast<ushort>(6000));
}

void TestDespike::test_image_noise_untouched()
{
    constexpr int width = 64;
    constexpr int height = 48;

    // 均匀噪声的最大可能跳变 2×amplitude 小于 3.5 倍行噪声尺度，一个点都不该动
    const QImage image = noisyGray16Image(width, height, 1000, 10.0, 2026u);

    PostProcess::ImageDespikeResult result;
    const QImage output = PostProcess::despikeImage(image, params(), &result);

    QCOMPARE(result.markedCount, 0);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            QCOMPARE(pixel16(output, x, y), pixel16(image, x, y));
        }
    }
}

void TestDespike::test_image_vertical_line_preserved()
{
    constexpr int width = 32;
    constexpr int height = 32;

    // 一像素宽的亮线（真实的发射线在二维图上就长这样）不是孤立点：
    // 每个像素的上下邻居和它一样亮，判据不成立
    QImage image = flatGray16Image(width, height, 1000);
    for (int y = 0; y < height; ++y) {
        setPixel16(image, 16, y, 6000);
    }

    PostProcess::ImageDespikeResult result;
    const QImage output = PostProcess::despikeImage(image, params(), &result);

    QCOMPARE(result.markedCount, 0);
    for (int y = 0; y < height; ++y) {
        QCOMPARE(pixel16(output, 16, y), static_cast<ushort>(6000));
    }
}

void TestDespike::test_image_two_pixel_blob_preserved()
{
    constexpr int width = 32;
    constexpr int height = 32;

    // 两像素宽的亮斑各有一个同样亮的邻居，不是孤立点
    QImage image = flatGray16Image(width, height, 1000);
    setPixel16(image, 15, 16, 6000);
    setPixel16(image, 16, 16, 6000);

    PostProcess::ImageDespikeResult result;
    const QImage output = PostProcess::despikeImage(image, params(), &result);

    QCOMPARE(result.markedCount, 0);
    QCOMPARE(pixel16(output, 15, 16), static_cast<ushort>(6000));
    QCOMPARE(pixel16(output, 16, 16), static_cast<ushort>(6000));
}

void TestDespike::test_image_ramp_untouched()
{
    constexpr int width = 40;
    constexpr int height = 30;

    // 严格线性斜坡：每个点都低于右侧和下方的邻居，本来就不满足「比 8 个邻居都高」
    QImage image(width, height, QImage::Format_Grayscale16);
    for (int y = 0; y < height; ++y) {
        ushort *line = reinterpret_cast<ushort *>(image.scanLine(y));
        for (int x = 0; x < width; ++x) {
            line[x] = static_cast<ushort>(1000 + 2 * x + 3 * y);
        }
    }

    PostProcess::ImageDespikeResult result;
    const QImage output = PostProcess::despikeImage(image, params(), &result);

    QCOMPARE(result.markedCount, 0);
    QVERIFY(output == image);
}

void TestDespike::test_image_border_untouched()
{
    constexpr int width = 16;
    constexpr int height = 16;

    QImage image = flatGray16Image(width, height, 1000);
    setPixel16(image, 0, 0, 6000);
    setPixel16(image, width - 1, height - 1, 6000);
    setPixel16(image, 8, 8, 6000);

    PostProcess::ImageDespikeResult result;
    const QImage output = PostProcess::despikeImage(image, params(), &result);

    // 只有内部那个孤立点被处理，边界一圈原样保留
    QCOMPARE(result.markedCount, 1);
    QCOMPARE(pixel16(output, 0, 0), static_cast<ushort>(6000));
    QCOMPARE(pixel16(output, width - 1, height - 1), static_cast<ushort>(6000));
    QCOMPARE(pixel16(output, 8, 8), static_cast<ushort>(1000));
}

void TestDespike::test_image_grayscale8_spike_replaced()
{
    constexpr int width = 32;
    constexpr int height = 32;

    QImage image(width, height, QImage::Format_Grayscale8);
    image.fill(100);
    image.scanLine(16)[16] = 250;

    PostProcess::ImageDespikeResult result;
    const QImage output = PostProcess::despikeImage(image, params(), &result);

    QCOMPARE(result.markedCount, 1);
    QCOMPARE(static_cast<int>(output.constScanLine(16)[16]), 100);
}

void TestDespike::test_image_rgb_spike_replaced()
{
    constexpr int width = 32;
    constexpr int height = 32;

    QImage image(width, height, QImage::Format_RGB888);
    image.fill(QColor(100, 100, 100));

    // 只有红通道被宇宙线打中；同一像素只算一次，但只有红通道被改写
    uchar *line = image.scanLine(16);
    line[16 * 3 + 0] = 250;

    PostProcess::ImageDespikeResult result;
    const QImage output = PostProcess::despikeImage(image, params(), &result);

    QCOMPARE(result.markedCount, 1);
    const uchar *outLine = output.constScanLine(16);
    QCOMPARE(static_cast<int>(outLine[16 * 3 + 0]), 100);
    QCOMPARE(static_cast<int>(outLine[16 * 3 + 1]), 100);
    QCOMPARE(static_cast<int>(outLine[16 * 3 + 2]), 100);
}

void TestDespike::test_image_short_side_untouched()
{
    for (const QSize size : {QSize(0, 0), QSize(1, 1), QSize(2, 8), QSize(8, 2)}) {
        QImage image(size, QImage::Format_Grayscale16);
        image.fill(0);

        PostProcess::ImageDespikeResult result;
        const QImage output = PostProcess::despikeImage(image, params(), &result);

        QCOMPARE(result.markedCount, 0);
        QCOMPARE(result.sampleCount, 0);
        QCOMPARE(output.size(), image.size());
    }
}

void TestDespike::test_image_row_range_only_processes_inside()
{
    constexpr int width = 32;
    constexpr int height = 32;

    QImage image = flatGray16Image(width, height, 1000);
    setPixel16(image, 16, 10, 6000); // 范围之前
    setPixel16(image, 16, 20, 6000); // 范围内（范围的最后一行）
    setPixel16(image, 16, 30, 6000); // 范围之后

    PostProcess::ImageDespikeResult result;
    const QImage output = PostProcess::despikeImage(image, params(), &result, 20, 25);

    QCOMPARE(result.markedCount, 1);
    QCOMPARE(result.sampleCount, (width - 2) * 6);
    QCOMPARE(pixel16(output, 16, 20), static_cast<ushort>(1000));
    QCOMPARE(pixel16(output, 16, 10), static_cast<ushort>(6000));
    QCOMPARE(pixel16(output, 16, 30), static_cast<ushort>(6000));
}

void TestDespike::test_image_row_range_uses_outside_neighbours()
{
    constexpr int width = 32;
    constexpr int height = 32;

    // 尖峰恰好在范围的第一行：它仍然应该用范围外的上一行做 3×3 邻域，
    // 不能把范围边界当成图像边界
    QImage image = flatGray16Image(width, height, 1000);
    setPixel16(image, 16, 20, 6000);
    setPixel16(image, 16, 19, 5000); // 上一行也有一个尖峰，但它在范围外，不该动

    PostProcess::ImageDespikeResult result;
    const QImage output = PostProcess::despikeImage(image, params(), &result, 20, 25);

    QCOMPARE(result.markedCount, 1);
    QCOMPARE(pixel16(output, 16, 20), static_cast<ushort>(1000));
    QCOMPARE(pixel16(output, 16, 19), static_cast<ushort>(5000));
}

void TestDespike::test_image_row_range_clamps_and_accepts_last_row_marker()
{
    constexpr int width = 24;
    constexpr int height = 16;

    QImage image = flatGray16Image(width, height, 1000);
    setPixel16(image, 10, 12, 6000);
    setPixel16(image, 10, 14, 6000);

    // lastRow = -1 表示最后一行；第 15 行是图像边界不能改，所以只有第 12 行
    // 参与检测（第 13、14 行也在范围内，但 14 是最后一个内部行，范围会覆盖到它）
    PostProcess::ImageDespikeResult result;
    const QImage output = PostProcess::despikeImage(image, params(), &result, 12, -1);

    QCOMPARE(result.markedCount, 2);
    QCOMPARE(result.sampleCount, (width - 2) * 3); // 第 12、13、14 行
    QCOMPARE(pixel16(output, 10, 12), static_cast<ushort>(1000));
    QCOMPARE(pixel16(output, 10, 14), static_cast<ushort>(1000));
}

void TestDespike::test_image_row_range_empty()
{
    constexpr int width = 32;
    constexpr int height = 32;

    QImage image = flatGray16Image(width, height, 1000);
    setPixel16(image, 16, 16, 6000);

    PostProcess::ImageDespikeResult result;
    const QImage output = PostProcess::despikeImage(image, params(), &result, 20, 10);

    QCOMPARE(result.markedCount, 0);
    QCOMPARE(result.sampleCount, 0);
    QVERIFY(output == image);
}

QTEST_APPLESS_MAIN(TestDespike)

#include "test_despike.moc"
