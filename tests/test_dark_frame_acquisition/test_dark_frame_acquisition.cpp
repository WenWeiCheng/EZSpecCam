// Guards the acquire-dark-frame burst path in MainWindow.
//
// The burst accumulator used to be initialised with
//     m_darkAccumSum.fill(elementCount, 0u)
// but QList::fill is fill(value, size), so that sized the accumulator to zero
// elements and the very next statement indexed it out of bounds. Nothing
// covered this path, so it shipped. These tests drive the real burst and check
// both that it survives and that the running average is correct.
#include <QObject>
#include <QApplication>
#include <QTest>
#include <QImage>
#include <QTimer>
#include <QDialog>
#include <QDateTime>
#include <QDebug>

#include "widgets/MainWindow.h"
#include "widgets/display/ImageViewWidget.h"
#include "CameraTypes.h"

class TestDarkFrameAcquisition : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void test_burst_does_not_crash();
    void test_burst_averages_pixels();
    void test_burst_handles_odd_and_even_widths();

private:
    // Runs a full burst of `frames` Grayscale16 frames whose every pixel is
    // (baseValue + i) for frame i, and returns the running average as shown by
    // the image view. Returns a null image if the view never received one.
    QImage runBurst(MainWindow &window, int w, int h, int frames, int baseValue);

    QTimer *m_modalDismiss = nullptr;
};

void TestDarkFrameAcquisition::initTestCase()
{
    // No camera is connected here, so AppController reports an error and
    // MainWindow answers with a modal message box. Nothing would ever dismiss
    // it, so do it from the harness.
    m_modalDismiss = new QTimer(this);
    m_modalDismiss->setInterval(50);
    connect(m_modalDismiss, &QTimer::timeout, this, [] {
        if (auto *dialog = QApplication::activeModalWidget()) {
            dialog->close();
        }
    });
    m_modalDismiss->start();
}

void TestDarkFrameAcquisition::cleanupTestCase()
{
}

void TestDarkFrameAcquisition::init()
{
}

void TestDarkFrameAcquisition::cleanup()
{
}

QImage TestDarkFrameAcquisition::runBurst(MainWindow &window, int w, int h,
                                          int frames, int baseValue)
{
    // Same entry point as clicking "Start" in AcquireDarkFrameDialog.
    const bool armed = QMetaObject::invokeMethod(&window,
                                                 "onAcquireDarkFrameStartRequested",
                                                 Qt::DirectConnection,
                                                 Q_ARG(int, frames));
    if (!armed) {
        return QImage();
    }

    for (int i = 0; i < frames; ++i) {
        QImage img(w, h, QImage::Format_Grayscale16);
        for (int y = 0; y < h; ++y) {
            auto *row = reinterpret_cast<quint16 *>(img.scanLine(y));
            for (int x = 0; x < w; ++x) {
                row[x] = static_cast<quint16>(baseValue + i);
            }
        }

        ImageData frame;
        frame.image = img;
        frame.timestamp = QDateTime::currentMSecsSinceEpoch();
        frame.frameNumber = i;

        QMetaObject::invokeMethod(&window, "onCameraFrameReady",
                                  Qt::DirectConnection, Q_ARG(ImageData, frame));

        // updateDisplay() throttles to one frame per MIN_FRAME_INTERVAL_MS, so
        // the view only picks up every few frames without this.
        QTest::qWait(40);
    }

    auto *view = window.findChild<ImageViewWidget *>();
    return view ? view->image() : QImage();
}

void TestDarkFrameAcquisition::test_burst_does_not_crash()
{
    // Before the fix this aborted inside the accumulator loop on the first
    // frame (Q_ASSERT in a debug build, a null-pointer write in a release one).
    MainWindow window;
    window.resize(1130, 870);
    window.show();
    QCoreApplication::processEvents();

    const QImage shown = runBurst(window, 64, 48, 10, 1000);

    QVERIFY2(!shown.isNull(), "image view never received the averaged frame");
    QCOMPARE(shown.width(), 64);
    QCOMPARE(shown.height(), 48);
}

void TestDarkFrameAcquisition::test_burst_averages_pixels()
{
    MainWindow window;
    window.resize(1130, 870);
    window.show();
    QCoreApplication::processEvents();

    // Frames hold 1000..1009, so the mean is 1004.5 and the code rounds half up.
    constexpr int kFrames = 10;
    constexpr int kExpected = 1005;

    const QImage shown = runBurst(window, 64, 48, kFrames, 1000);
    QVERIFY(!shown.isNull());

    for (int y = 0; y < shown.height(); ++y) {
        const auto *row = reinterpret_cast<const quint16 *>(shown.constScanLine(y));
        for (int x = 0; x < shown.width(); ++x) {
            QCOMPARE(int(row[x]), kExpected);
        }
    }
}

void TestDarkFrameAcquisition::test_burst_handles_odd_and_even_widths()
{
    // QImage pads Grayscale16 rows to a 4-byte boundary, so odd widths have a
    // row stride wider than width*2. The burst must survive both; this asserts
    // the crash is gone, not the pixel values (see the row-stride note in
    // MainWindow::onCameraFrameReady).
    for (int width : {64, 63, 641}) {
        MainWindow window;
        window.resize(1130, 870);
        window.show();
        QCoreApplication::processEvents();

        const QImage shown = runBurst(window, width, 48, 5, 500);
        QVERIFY2(!shown.isNull(),
                 qPrintable(QStringLiteral("no frame for width %1").arg(width)));
        QCOMPARE(shown.width(), width);
    }
}

QTEST_MAIN(TestDarkFrameAcquisition)
#include "test_dark_frame_acquisition.moc"
