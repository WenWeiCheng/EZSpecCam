// Guards the image view's colour scale against collapsing to a hairline.
//
// QCustomPlot hands the tick labels their room before the gradient gets any, so
// a fixed strip width loses the bar as soon as the labels outgrow it. The strip
// is now sized from the font, and these tests pin that down: the gradient must
// stay visible at large application font sizes, and the strip must actually
// grow with the font instead of staying at a hard-coded constant.
#include <QObject>
#include <QApplication>
#include <QTest>
#include <QFont>
#include <QImage>
#include <QDebug>

#include "widgets/display/ImageViewWidget.h"
#include "qcustomplot.h"

namespace {

// The colour-scale plot is the QCustomPlot child that owns no QCPColorMap.
QCustomPlot *colorScalePlotOf(ImageViewWidget *view)
{
    for (QObject *child : view->children()) {
        auto *plot = qobject_cast<QCustomPlot *>(child);
        if (plot && !plot->findChild<QCPColorMap *>()) {
            return plot;
        }
    }
    return nullptr;
}

// Renders the strip and counts how many pixel columns the gradient covers.
// The background is forced to magenta so "not background" means gradient or
// text, and a gradient column is painted over its full height while a text
// column only picks up a few rows of glyphs.
int gradientColumnCount(QCustomPlot *strip)
{
    const int w = strip->width();
    const int h = strip->height();
    if (w <= 0 || h <= 0) {
        return -1;
    }

    QImage canvas(w, h, QImage::Format_ARGB32);
    canvas.fill(Qt::magenta);
    strip->setBackground(Qt::magenta);
    // QCP caches the gradient as an image sized when it was last laid out, and
    // scales it to whatever rect it is given. Repaint so the measurement sees
    // the gradient the current geometry actually produces.
    strip->replot();
    strip->render(&canvas, QPoint(0, 0), QRegion(0, 0, w, h));

    int columns = 0;
    for (int x = 0; x < w; ++x) {
        int painted = 0;
        for (int y = 0; y < h; ++y) {
            if (canvas.pixelColor(x, y) != QColor(Qt::magenta)) {
                ++painted;
            }
        }
        if (painted > h / 2) {
            ++columns;
        }
    }
    return columns;
}

QImage makeGrayscale16(int w, int h)
{
    QImage img(w, h, QImage::Format_Grayscale16);
    for (int y = 0; y < h; ++y) {
        auto *row = reinterpret_cast<quint16 *>(img.scanLine(y));
        for (int x = 0; x < w; ++x) {
            row[x] = static_cast<quint16>((x * 61 + y * 13) & 0xFFFF);
        }
    }
    return img;
}

} // namespace

class TestImageColorbar : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase();
    void cleanupTestCase();
    void init();
    void cleanup();

    void test_gradient_survives_large_font();
    void test_strip_width_grows_with_font();

private:
    // Builds a loaded view at the given application font point size and reports
    // the strip geometry.
    QCustomPlot *loadViewAtPointSize(ImageViewWidget **view, int pointSize);

    QFont m_originalFont;
};

void TestImageColorbar::initTestCase()
{
    m_originalFont = QApplication::font();
}

void TestImageColorbar::cleanupTestCase()
{
    QApplication::setFont(m_originalFont);
}

void TestImageColorbar::init()
{
}

void TestImageColorbar::cleanup()
{
}

QCustomPlot *TestImageColorbar::loadViewAtPointSize(ImageViewWidget **view, int pointSize)
{
    QFont font = m_originalFont;
    font.setPointSize(pointSize);
    QApplication::setFont(font);

    auto *w = new ImageViewWidget;
    w->resize(1000, 800);
    w->show();
    w->setImage(makeGrayscale16(1024, 768));
    QCoreApplication::processEvents();
    QCoreApplication::processEvents();

    *view = w;
    return colorScalePlotOf(w);
}

void TestImageColorbar::test_gradient_survives_large_font()
{
    // A fixed 60px strip loses the gradient to the tick labels as the font
    // grows (measured: 18/16/12/8/4/2 columns from 8pt to 13pt), which is the
    // hairline the colour scale used to collapse to. The strip is now sized
    // from the font, so the gradient keeps its width at every size.
    for (int pointSize : {9, 11, 12, 13, 16, 20}) {
        ImageViewWidget *view = nullptr;
        QCustomPlot *strip = loadViewAtPointSize(&view, pointSize);
        QVERIFY2(strip != nullptr, "colour scale plot not found on ImageViewWidget");

        const int columns = gradientColumnCount(strip);
        qInfo() << pointSize << "pt: strip width" << strip->width()
                << "gradient columns" << columns;

        QVERIFY2(columns >= 10,
                 qPrintable(QStringLiteral("gradient collapsed to %1 columns at %2pt")
                                .arg(columns).arg(pointSize)));

        delete view;
    }
}

void TestImageColorbar::test_strip_width_grows_with_font()
{
    ImageViewWidget *small = nullptr;
    QCustomPlot *smallStrip = loadViewAtPointSize(&small, 9);
    QVERIFY(smallStrip != nullptr);
    const int smallWidth = smallStrip->width();

    ImageViewWidget *large = nullptr;
    QCustomPlot *largeStrip = loadViewAtPointSize(&large, 18);
    QVERIFY(largeStrip != nullptr);
    const int largeWidth = largeStrip->width();

    qInfo() << "9pt strip width" << smallWidth << "18pt strip width" << largeWidth;

    // A hard-coded strip width fails this; the strip has to track the font.
    QVERIFY2(largeWidth > smallWidth,
             "colour scale width did not grow with the font");

    delete small;
    delete large;
}

QTEST_MAIN(TestImageColorbar)
#include "test_image_colorbar.moc"
