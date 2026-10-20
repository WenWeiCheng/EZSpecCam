// 去尖峰管线的集成测试。
//
// 去尖峰不再是显示层的小把戏：它在 MainWindow 里、verticalBinning 之前执行，
// 所以谱线看到的必须是去尖峰之后的二维图；行范围与 vbin 共用；关掉开关后要能
// 从校正后的原图恢复。这些行为只有把真实的 MainWindow 跑起来才看得到。
#include <QAction>
#include <QApplication>
#include <QDateTime>
#include <QFile>
#include <QImage>
#include <QJsonDocument>
#include <QJsonObject>
#include <QObject>
#include <QTemporaryDir>
#include <QTest>
#include <QTimer>

#include "CameraTypes.h"
#include "widgets/MainWindow.h"
#include "widgets/display/ImageViewWidget.h"
#include "widgets/display/SpectrumViewWidget.h"
#include "widgets/dialogs/RowRangeDialog.h"

namespace {

constexpr int kWidth = 64;
constexpr int kHeight = 48;
constexpr int kBackground = 1000;
constexpr int kSpikeValue = 6000;
constexpr int kInsideSpikeRow = 24;
constexpr int kOutsideSpikeRow = 5;
constexpr int kSpikeColumn = 32;

QImage frameWithSpikes()
{
    QImage image(kWidth, kHeight, QImage::Format_Grayscale16);
    for (int y = 0; y < kHeight; ++y) {
        ushort *line = reinterpret_cast<ushort *>(image.scanLine(y));
        for (int x = 0; x < kWidth; ++x) {
            line[x] = static_cast<ushort>(kBackground);
        }
    }
    reinterpret_cast<ushort *>(image.scanLine(kInsideSpikeRow))[kSpikeColumn] =
        static_cast<ushort>(kSpikeValue);
    reinterpret_cast<ushort *>(image.scanLine(kOutsideSpikeRow))[kSpikeColumn] =
        static_cast<ushort>(kSpikeValue);
    return image;
}

void deliverFrame(MainWindow &window, const QImage &image)
{
    ImageData frame;
    frame.image = image;
    frame.timestamp = QDateTime::currentMSecsSinceEpoch();
    frame.frameNumber = 1;

    QVERIFY(QMetaObject::invokeMethod(&window, "onCameraFrameReady",
                                      Qt::DirectConnection, Q_ARG(ImageData, frame)));
}

QAction *findAction(MainWindow &window, const QString &plainText)
{
    for (QAction *action : window.findChildren<QAction *>()) {
        QString text = action->text();
        text.remove(QLatin1Char('&'));
        if (text == plainText) {
            return action;
        }
    }
    return nullptr;
}

} // namespace

class TestDespikePipeline : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase() {}

    void test_toggle_rebuilds_image_and_spectrum();
    void test_row_range_limits_despike();
    void test_save_includes_despike();

private:
    QTimer *m_modalDismiss = nullptr;
};

void TestDespikePipeline::initTestCase()
{
    // No camera is connected, so AppController reports an error and MainWindow
    // answers with a modal message box. Nothing would ever dismiss it.
    m_modalDismiss = new QTimer(this);
    m_modalDismiss->setInterval(50);
    connect(m_modalDismiss, &QTimer::timeout, this, [] {
        if (auto *dialog = QApplication::activeModalWidget()) {
            dialog->close();
        }
    });
    m_modalDismiss->start();
}

void TestDespikePipeline::test_toggle_rebuilds_image_and_spectrum()
{
    MainWindow window;
    window.resize(1130, 870);
    window.show();
    QCoreApplication::processEvents();

    // MainWindow 的 updateDisplay() 有 33 ms 的节流，构造函数里刚启动计时器，
    // 第一帧来太快要先等它过期
    QTest::qWait(50);

    auto *imageView = window.findChild<ImageViewWidget *>();
    auto *spectrumView = window.findChild<SpectrumViewWidget *>();
    QAction *despikeAction = findAction(window, QStringLiteral("Despike"));
    QAction *vbinAction = findAction(window, QStringLiteral("Software Vertical Binning"));
    QVERIFY(imageView);
    QVERIFY(spectrumView);
    QVERIFY(despikeAction);
    QVERIFY(vbinAction);

    deliverFrame(window, frameWithSpikes());
    QTest::qWait(50);

    // 去尖峰关闭：二维图保留两个孤立尖峰
    QCOMPARE(imageView->pixelValue(kSpikeColumn, kInsideSpikeRow), kSpikeValue);
    QCOMPARE(imageView->pixelValue(kSpikeColumn, kOutsideSpikeRow), kSpikeValue);

    // 打开去尖峰：默认行范围是整幅图，两个尖峰都该被替换
    despikeAction->setChecked(true);
    QCoreApplication::processEvents();
    QCOMPARE(imageView->pixelValue(kSpikeColumn, kInsideSpikeRow), kBackground);
    QCOMPARE(imageView->pixelValue(kSpikeColumn, kOutsideSpikeRow), kBackground);

    // 再打开 vbin：谱线必须来自去尖峰之后的二维图，尖峰列不再多出 2×5000
    vbinAction->setChecked(true);
    QCoreApplication::processEvents();
    QVERIFY(spectrumView->hasData());
    QCOMPARE(spectrumView->yData().size(), kWidth);
    QCOMPARE(spectrumView->yData()[kSpikeColumn],
             static_cast<double>(kBackground * kHeight));

    // 关掉去尖峰，同一帧重新 binning 后尖峰应该回来
    despikeAction->setChecked(false);
    QCoreApplication::processEvents();
    QCOMPARE(spectrumView->yData()[kSpikeColumn],
             static_cast<double>(kBackground * kHeight + 2 * (kSpikeValue - kBackground)));
}

void TestDespikePipeline::test_row_range_limits_despike()
{
    MainWindow window;
    window.resize(1130, 870);
    window.show();
    QCoreApplication::processEvents();

    // MainWindow 的 updateDisplay() 有 33 ms 的节流，构造函数里刚启动计时器，
    // 第一帧来太快要先等它过期
    QTest::qWait(50);

    auto *imageView = window.findChild<ImageViewWidget *>();
    auto *spectrumView = window.findChild<SpectrumViewWidget *>();
    QAction *despikeAction = findAction(window, QStringLiteral("Despike"));
    QAction *vbinAction = findAction(window, QStringLiteral("Software Vertical Binning"));
    QVERIFY(imageView);
    QVERIFY(spectrumView);
    QVERIFY(despikeAction);
    QVERIFY(vbinAction);

    deliverFrame(window, frameWithSpikes());
    QTest::qWait(50);

    despikeAction->setChecked(true);
    QCoreApplication::processEvents();

    // 通过真实的 Row Range 对话框把范围设成第 20..27 行（0-based）
    QVERIFY(QMetaObject::invokeMethod(&window, "on_rowRange_triggered"));
    QCoreApplication::processEvents();
    auto *rangeDialog = window.findChild<RowRangeDialog *>();
    QVERIFY(rangeDialog);
    rangeDialog->setRange(20, 27);
    QVERIFY(QMetaObject::invokeMethod(rangeDialog, "onApplyClicked"));
    QCoreApplication::processEvents();

    // Apply 会自动打开 vbin；谱线只累加这 8 行，范围内的尖峰已被摘掉
    QVERIFY(spectrumView->hasData());
    QCOMPARE(spectrumView->yData()[kSpikeColumn],
             static_cast<double>(kBackground * 8));

    // 关掉 vbin 看二维图：范围内被替换，范围外原样保留
    vbinAction->setChecked(false);
    QCoreApplication::processEvents();
    QCOMPARE(imageView->pixelValue(kSpikeColumn, kInsideSpikeRow), kBackground);
    QCOMPARE(imageView->pixelValue(kSpikeColumn, kOutsideSpikeRow), kSpikeValue);
}

void TestDespikePipeline::test_save_includes_despike()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    MainWindow window;
    window.resize(1130, 870);
    window.show();
    QCoreApplication::processEvents();
    QTest::qWait(50);

    QAction *despikeAction = findAction(window, QStringLiteral("Despike"));
    QVERIFY(despikeAction);

    deliverFrame(window, frameWithSpikes());
    QTest::qWait(50);

    despikeAction->setChecked(true);
    QCoreApplication::processEvents();

    // 部分行范围 + vbin：实时管线只处理 ROI，完整的二维图在保存时才补齐
    QVERIFY(QMetaObject::invokeMethod(&window, "on_rowRange_triggered"));
    QCoreApplication::processEvents();
    auto *rangeDialog = window.findChild<RowRangeDialog *>();
    QVERIFY(rangeDialog);
    rangeDialog->setRange(20, 27);
    QVERIFY(QMetaObject::invokeMethod(rangeDialog, "onApplyClicked"));
    QCoreApplication::processEvents();

    const QString csvPath = dir.filePath(QStringLiteral("frame.csv"));
    QVERIFY(QMetaObject::invokeMethod(&window, "saveFrameToFile",
                                      Qt::DirectConnection,
                                      Q_ARG(QString, csvPath)));

    QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(csvPath), 5000);
    const QString metadataPath = dir.filePath(QStringLiteral("frame_metadata.json"));
    QTRY_VERIFY_WITH_TIMEOUT(QFile::exists(metadataPath), 5000);

    // 保存的是完整二维图：范围内被替换，范围外保持原值
    QFile csv(csvPath);
    QVERIFY(csv.open(QIODevice::ReadOnly | QIODevice::Text));
    const QList<QByteArray> lines = csv.readAll().split('\n');
    QVERIFY2(lines.size() > kHeight, "saved CSV does not have enough rows");

    const QList<QByteArray> insideRow = lines[kInsideSpikeRow].split(',');
    const QList<QByteArray> outsideRow = lines[kOutsideSpikeRow].split(',');
    QCOMPARE(insideRow.size(), kWidth);
    QCOMPARE(outsideRow.size(), kWidth);
    QCOMPARE(insideRow[kSpikeColumn].toInt(), kBackground);
    QCOMPARE(outsideRow[kSpikeColumn].toInt(), kSpikeValue);

    // metadata 记录的是实际生效的行范围（1-based）和阈值
    QFile metadata(metadataPath);
    QVERIFY(metadata.open(QIODevice::ReadOnly | QIODevice::Text));
    const QJsonObject root = QJsonDocument::fromJson(metadata.readAll()).object();
    const QJsonObject software = root.value(QStringLiteral("softwareSettings")).toObject();
    QCOMPARE(software.value(QStringLiteral("despikeApplied")).toBool(), true);
    QCOMPARE(software.value(QStringLiteral("despikeStartRow")).toInt(), 21);
    QCOMPARE(software.value(QStringLiteral("despikeEndRow")).toInt(), 28);
    QCOMPARE(software.value(QStringLiteral("despikePrimaryThreshold")).toDouble(), 3.5);
}

QTEST_MAIN(TestDespikePipeline)

#include "test_despike_pipeline.moc"
