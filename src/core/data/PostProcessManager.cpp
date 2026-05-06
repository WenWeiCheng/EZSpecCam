#include "PostProcessManager.h"

#include <QDebug>
#include <QVector>
#include <algorithm>

PostProcessManager::PostProcessManager(QObject *parent)
    : QObject(parent)
    , m_enabled(false)
    , m_operations(None)
    , m_vBinStartRow(0)
    , m_vBinEndRow(-1)
{
}

PostProcessManager::~PostProcessManager()
{
}

void PostProcessManager::setEnabled(bool enabled)
{
    if (m_enabled != enabled) {
        m_enabled = enabled;
        emit enabledChanged(enabled);
    }
}

bool PostProcessManager::isEnabled() const
{
    return m_enabled;
}

void PostProcessManager::setOperationEnabled(Operation operation, bool enabled)
{
    bool wasEnabled = m_operations.testFlag(operation);
    if (enabled) {
        m_operations |= operation;
    } else {
        m_operations &= ~operation;
    }

    if (wasEnabled != enabled) {
        emit operationEnabledChanged(operation, enabled);
    }
}

bool PostProcessManager::isOperationEnabled(Operation operation) const
{
    return m_operations.testFlag(operation);
}

void PostProcessManager::setOperations(Operations operations)
{
    m_operations = operations;
}

PostProcessManager::Operations PostProcessManager::operations() const
{
    return m_operations;
}

void PostProcessManager::setVerticalBinningRowRange(int start, int end)
{
    if (m_vBinStartRow != start || m_vBinEndRow != end) {
        m_vBinStartRow = start;
        m_vBinEndRow = end;
        emit verticalBinningRowRangeChanged(start, end);
    }
}

QPair<int, int> PostProcessManager::verticalBinningRowRange() const
{
    return qMakePair(m_vBinStartRow, m_vBinEndRow);
}

void PostProcessManager::processFrame(ImageData &frame)
{
    if (!m_enabled) {
        return;
    }

    if (!frame.hasOriginal()) {
        frame.originalImage = frame.image;
    }

    if (m_operations.testFlag(VerticalBinning)) {
        int startRow = m_vBinStartRow;
        int endRow = m_vBinEndRow;

        if (endRow < 0 || endRow >= frame.originalImage.height()) {
            endRow = frame.originalImage.height() - 1;
        }

        if (startRow < 0) {
            startRow = 0;
        }

        if (startRow <= endRow) {
            frame.image = applyVerticalBinning(frame.originalImage, startRow, endRow);
        }
    }

    if (m_operations.testFlag(DarkFrameSubtraction) && hasDarkFrame()) {
        frame.image = applyDarkFrameSubtraction(frame.image, m_darkFrame);
    }

    if (m_operations.testFlag(FlatFieldCorrection) && hasFlatField()) {
        frame.image = applyFlatFieldCorrection(frame.image, m_flatField);
    }
}

void PostProcessManager::setDarkFrame(const QImage &darkFrame)
{
    m_darkFrame = darkFrame;
}

void PostProcessManager::clearDarkFrame()
{
    m_darkFrame = QImage();
}

bool PostProcessManager::hasDarkFrame() const
{
    return !m_darkFrame.isNull();
}

void PostProcessManager::setFlatField(const QImage &flatField)
{
    m_flatField = flatField;
}

void PostProcessManager::clearFlatField()
{
    m_flatField = QImage();
}

bool PostProcessManager::hasFlatField() const
{
    return !m_flatField.isNull();
}

QImage PostProcessManager::applyVerticalBinning(const QImage &image, int startRow, int endRow)
{
    const int width = image.width();
    const int height = image.height();

    if (width <= 0 || height <= 0) {
        return image;
    }

    if (startRow < 0) startRow = 0;
    if (endRow >= height) endRow = height - 1;
    if (startRow > endRow) {
        return image;
    }

    int rowCount = endRow - startRow + 1;

    QImage binnedImage(width, 1, image.format());

    if (image.format() == QImage::Format_Grayscale16) {
        const ushort *srcData = reinterpret_cast<const ushort *>(image.bits());
        ushort *dstData = reinterpret_cast<ushort *>(binnedImage.bits());

        static quint64 sumsBuffer[16384];
        quint64 *sums = sumsBuffer;

        for (int x = 0; x < width; x++) {
            sums[x] = 0;
        }

        for (int y = startRow; y <= endRow; y++) {
            const ushort *row = srcData + y * width;
            for (int x = 0; x < width; x++) {
                sums[x] += row[x];
            }
        }

        for (int x = 0; x < width; x++) {
            dstData[x] = static_cast<ushort>(sums[x] / rowCount);
        }
    } else if (image.format() == QImage::Format_Grayscale8) {
        const uchar *srcData = image.bits();
        uchar *dstData = binnedImage.bits();

        static quint32 sumsBuffer[16384];
        quint32 *sums = sumsBuffer;

        for (int x = 0; x < width; x++) {
            sums[x] = 0;
        }

        for (int y = startRow; y <= endRow; y++) {
            const uchar *row = srcData + y * width;
            for (int x = 0; x < width; x++) {
                sums[x] += row[x];
            }
        }

        for (int x = 0; x < width; x++) {
            dstData[x] = static_cast<uchar>(sums[x] / rowCount);
        }
    } else if (image.format() == QImage::Format_RGB888) {
        const uchar *srcData = image.bits();
        uchar *dstData = binnedImage.bits();

        const int rowStride = width * 3;
        static quint32 sumsRBuffer[16384], sumsGBuffer[16384], sumsBBuffer[16384];
        quint32 *sumsR = sumsRBuffer;
        quint32 *sumsG = sumsGBuffer;
        quint32 *sumsB = sumsBBuffer;

        for (int x = 0; x < width; x++) {
            sumsR[x] = sumsG[x] = sumsB[x] = 0;
        }

        for (int y = startRow; y <= endRow; y++) {
            const uchar *row = srcData + y * rowStride;
            for (int x = 0; x < width; x++) {
                int idx = x * 3;
                sumsR[x] += row[idx];
                sumsG[x] += row[idx + 1];
                sumsB[x] += row[idx + 2];
            }
        }

        for (int x = 0; x < width; x++) {
            dstData[x * 3] = static_cast<uchar>(sumsR[x] / rowCount);
            dstData[x * 3 + 1] = static_cast<uchar>(sumsG[x] / rowCount);
            dstData[x * 3 + 2] = static_cast<uchar>(sumsB[x] / rowCount);
        }
    } else {
        qWarning() << "PostProcessManager: Unsupported image format for vertical binning";
        return image;
    }

    qDebug() << "PostProcessManager: Applied vertical binning:" << width << "x" << rowCount << "->" << width << "x1";
    return binnedImage;
}

QImage PostProcessManager::applyDarkFrameSubtraction(const QImage &image, const QImage &darkFrame)
{
    if (image.size() != darkFrame.size() || image.format() != darkFrame.format()) {
        qWarning() << "PostProcessManager: Dark frame size/format mismatch";
        return image;
    }

    QImage result = image.copy();

    if (image.format() == QImage::Format_Grayscale16) {
        const ushort *srcData = reinterpret_cast<const ushort *>(image.bits());
        const ushort *darkData = reinterpret_cast<const ushort *>(darkFrame.bits());
        ushort *dstData = reinterpret_cast<ushort *>(result.bits());
        int pixelCount = image.width() * image.height();

        for (int i = 0; i < pixelCount; i++) {
            int diff = static_cast<int>(srcData[i]) - static_cast<int>(darkData[i]);
            dstData[i] = static_cast<ushort>(qMax(0, diff));
        }
    } else if (image.format() == QImage::Format_Grayscale8) {
        const uchar *srcData = image.bits();
        const uchar *darkData = darkFrame.bits();
        uchar *dstData = result.bits();
        int pixelCount = image.width() * image.height();

        for (int i = 0; i < pixelCount; i++) {
            int diff = static_cast<int>(srcData[i]) - static_cast<int>(darkData[i]);
            dstData[i] = static_cast<uchar>(qMax(0, diff));
        }
    }

    return result;
}

QImage PostProcessManager::applyFlatFieldCorrection(const QImage &image, const QImage &flatField)
{
    if (image.size() != flatField.size() || image.format() != flatField.format()) {
        qWarning() << "PostProcessManager: Flat field size/format mismatch";
        return image;
    }

    QImage result = image.copy();

    if (image.format() == QImage::Format_Grayscale16) {
        const ushort *srcData = reinterpret_cast<const ushort *>(image.bits());
        const ushort *flatData = reinterpret_cast<const ushort *>(flatField.bits());
        ushort *dstData = reinterpret_cast<ushort *>(result.bits());
        int pixelCount = image.width() * image.height();

        for (int i = 0; i < pixelCount; i++) {
            if (flatData[i] > 0) {
                double corrected = (static_cast<double>(srcData[i]) / static_cast<double>(flatData[i])) * 65535.0;
                dstData[i] = static_cast<ushort>(qMin(static_cast<int>(corrected), 65535));
            } else {
                dstData[i] = 0;
            }
        }
    } else if (image.format() == QImage::Format_Grayscale8) {
        const uchar *srcData = image.bits();
        const uchar *flatData = flatField.bits();
        uchar *dstData = result.bits();
        int pixelCount = image.width() * image.height();

        for (int i = 0; i < pixelCount; i++) {
            if (flatData[i] > 0) {
                double corrected = (static_cast<double>(srcData[i]) / static_cast<double>(flatData[i])) * 255.0;
                dstData[i] = static_cast<uchar>(qMin(static_cast<int>(corrected), 255));
            } else {
                dstData[i] = 0;
            }
        }
    }

    return result;
}
