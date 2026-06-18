#include <QApplication>
#include <QTest>
#include <QSignalSpy>
#include <QImage>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>

#include "widgets/dialogs/RowRangeDialog.h"
#include "widgets/PostProcess.h"
#include "workers/FileLoaderWorker.h"
#include "workers/formats/TiffFormatHandler.h"
#include "workers/SaveTypes.h"

class TestRowRangeDialogEdgeCases : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void test_setImageHeight_zero_does_not_crash();
    void test_setRange_after_zero_height_does_not_crash();
    void test_setImageHeight_one_works();
    void test_setImageHeight_normal_works();
    void test_setImageHeight_after_zero_set_to_normal_recovers();
    void test_loaded_2d_image_has_valid_dimensions_for_row_range();
    void test_spectrum_view_with_original_allows_row_range_dialog();

private:
    QImage makeImage(int w, int h) const;
    QImage make2DImage(int w, int h) const;
    QString writeTiff(const QString &dir, const QString &name, const QImage &img) const;
};

void TestRowRangeDialogEdgeCases::initTestCase()
{
}

void TestRowRangeDialogEdgeCases::cleanupTestCase()
{
}

void TestRowRangeDialogEdgeCases::init()
{
}

void TestRowRangeDialogEdgeCases::cleanup()
{
}

QImage TestRowRangeDialogEdgeCases::makeImage(int w, int h) const
{
    return make2DImage(w, h);
}

QImage TestRowRangeDialogEdgeCases::make2DImage(int w, int h) const
{
    QImage img(w, h, QImage::Format_Grayscale16);
    for (int y = 0; y < h; ++y) {
        ushort *row = reinterpret_cast<ushort *>(img.scanLine(y));
        for (int x = 0; x < w; ++x) {
            row[x] = static_cast<ushort>((x * 100 + y * 50) & 0xFFFF);
        }
    }
    return img;
}

QString TestRowRangeDialogEdgeCases::writeTiff(const QString &dir, const QString &name, const QImage &img) const
{
    QString path = dir + "/" + name;
    img.save(path, "TIFF");
    return path;
}

void TestRowRangeDialogEdgeCases::test_setImageHeight_zero_does_not_crash()
{
    RowRangeDialog dialog;
    dialog.setImageHeight(0);
    QVERIFY(dialog.startRow() >= 0);
    QVERIFY(dialog.endRow() >= 0);
}

void TestRowRangeDialogEdgeCases::test_setRange_after_zero_height_does_not_crash()
{
    RowRangeDialog dialog;
    dialog.setImageHeight(0);
    dialog.setRange(0, 0);
    QVERIFY(dialog.startRow() >= 0);
    QVERIFY(dialog.endRow() >= 0);
}

void TestRowRangeDialogEdgeCases::test_setImageHeight_one_works()
{
    RowRangeDialog dialog;
    dialog.setImageHeight(1);
    dialog.setRange(0, 0);
    QCOMPARE(dialog.startRow(), 1);
    QCOMPARE(dialog.endRow(), 1);
}

void TestRowRangeDialogEdgeCases::test_setImageHeight_normal_works()
{
    RowRangeDialog dialog;
    dialog.setImageHeight(1024);
    dialog.setRange(1020, 1023);
    QCOMPARE(dialog.startRow(), 1021);
    QCOMPARE(dialog.endRow(), 1024);
}

void TestRowRangeDialogEdgeCases::test_setImageHeight_after_zero_set_to_normal_recovers()
{
    RowRangeDialog dialog;
    dialog.setImageHeight(0);
    dialog.setImageHeight(1024);
    dialog.setRange(1020, 1023);
    QCOMPARE(dialog.startRow(), 1021);
    QCOMPARE(dialog.endRow(), 1024);
}

void TestRowRangeDialogEdgeCases::test_loaded_2d_image_has_valid_dimensions_for_row_range()
{
    // 新格式：主图是 2D original；通过 RowRangeDialog 设置行范围
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    QString originalPath = writeTiff(tmp.path(), "original.tiff", make2DImage(2048, 1024));
    QVERIFY(QFile::exists(originalPath));

    FileLoaderWorker worker;
    QSignalSpy okSpy(&worker, &FileLoaderWorker::frameLoaded);
    QSignalSpy failSpy(&worker, &FileLoaderWorker::loadFailed);

    worker.loadFrame(originalPath);
    QCOMPARE(failSpy.count(), 0);
    QCOMPARE(okSpy.count(), 1);

    LoadResult res = qvariant_cast<LoadResult>(okSpy.takeFirst().at(0));
    QVERIFY(res.success);

    QCOMPARE(res.frame.image.height(), 1024);
    QCOMPARE(res.frame.originalImage.height(), 0);  // 不再加载 _original 边车

    RowRangeDialog dialog;
    dialog.setImageHeight(res.frame.image.height());
    dialog.setRange(1020, 1023);
    QCOMPARE(dialog.startRow(), 1021);
    QCOMPARE(dialog.endRow(), 1024);
}

void TestRowRangeDialogEdgeCases::test_spectrum_view_with_original_allows_row_range_dialog()
{
    // 模拟 spectrumView 状态：image=1 行, originalImage=2D。
    // 验证使用 originalImage.height() 作为对话框高度时，
    // spinbox 范围是 [1, h]，不是 [1, 1]。
    QTemporaryDir tmp;
    QVERIFY(tmp.isValid());

    QString originalPath = writeTiff(tmp.path(), "rt.tiff", make2DImage(2048, 1024));
    QVERIFY(QFile::exists(originalPath));

    FileLoaderWorker worker;
    QSignalSpy okSpy(&worker, &FileLoaderWorker::frameLoaded);
    QSignalSpy failSpy(&worker, &FileLoaderWorker::loadFailed);
    worker.loadFrame(originalPath);
    QCOMPARE(failSpy.count(), 0);
    QCOMPARE(okSpy.count(), 1);

    LoadResult res = qvariant_cast<LoadResult>(okSpy.takeFirst().at(0));
    QVERIFY(res.success);

    // 模拟自动恢复 / live vbin 后的状态
    PostProcess::verticalBinning(res.frame, 1, 1024);
    QCOMPARE(res.frame.image.height(), 1);
    QVERIFY(res.frame.hasOriginal());
    QCOMPARE(res.frame.originalImage.height(), 1024);

    // MainWindow 修复后的逻辑：使用 originalImage.height()
    const int dialogHeight = res.frame.hasOriginal()
        ? res.frame.originalImage.height()
        : res.frame.image.height();
    QCOMPARE(dialogHeight, 1024);

    RowRangeDialog dialog;
    dialog.setImageHeight(dialogHeight);
    // 验证 spinbox 上限确实是 1024（而不是 1）
    QCOMPARE(dialog.startRow(), 1);
    QCOMPARE(dialog.endRow(), 1024);

    // 可以设置任意子范围
    dialog.setRange(100, 200);
    QCOMPARE(dialog.startRow(), 101);
    QCOMPARE(dialog.endRow(), 201);
}

QTEST_MAIN(TestRowRangeDialogEdgeCases)
#include "test_row_range_dialog.moc"
