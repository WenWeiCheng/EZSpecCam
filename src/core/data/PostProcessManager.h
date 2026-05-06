#ifndef POSTPROCESSMANAGER_H
#define POSTPROCESSMANAGER_H

#include <QObject>
#include <QImage>
#include <QPair>

#include "interfaces/CameraTypes.h"

class PostProcessManager : public QObject
{
    Q_OBJECT

public:
    enum Operation {
        None = 0x00,
        VerticalBinning = 0x01,
        DarkFrameSubtraction = 0x02,
        FlatFieldCorrection = 0x04,
        CosmicRayRemoval = 0x08
    };
    Q_ENUM(Operation)
    Q_DECLARE_FLAGS(Operations, Operation)

    explicit PostProcessManager(QObject *parent = nullptr);
    ~PostProcessManager() override;

    void setEnabled(bool enabled);
    bool isEnabled() const;

    void setOperationEnabled(Operation operation, bool enabled);
    bool isOperationEnabled(Operation operation) const;

    void setOperations(Operations operations);
    Operations operations() const;

    void setVerticalBinningRowRange(int start, int end);
    QPair<int, int> verticalBinningRowRange() const;

    void processFrame(ImageData &frame);

    void setDarkFrame(const QImage &darkFrame);
    void clearDarkFrame();
    bool hasDarkFrame() const;

    void setFlatField(const QImage &flatField);
    void clearFlatField();
    bool hasFlatField() const;

signals:
    void enabledChanged(bool enabled);
    void operationEnabledChanged(Operation operation, bool enabled);
    void verticalBinningRowRangeChanged(int start, int end);

private:
    QImage applyVerticalBinning(const QImage &image, int startRow, int endRow);
    QImage applyDarkFrameSubtraction(const QImage &image, const QImage &darkFrame);
    QImage applyFlatFieldCorrection(const QImage &image, const QImage &flatField);

    bool m_enabled;
    Operations m_operations;
    int m_vBinStartRow;
    int m_vBinEndRow;
    QImage m_darkFrame;
    QImage m_flatField;
};

Q_DECLARE_OPERATORS_FOR_FLAGS(PostProcessManager::Operations)

#endif
