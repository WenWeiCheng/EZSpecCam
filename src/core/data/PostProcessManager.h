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

    struct ProcessConfig {
        bool enabled = false;
        Operations operations = None;
        int vBinStartRow = 0;
        int vBinEndRow = -1;
        QImage darkFrame;
        QImage flatField;

        bool hasDarkFrame() const { return !darkFrame.isNull(); }
        bool hasFlatField() const { return !flatField.isNull(); }
    };

    explicit PostProcessManager(QObject *parent = nullptr);
    ~PostProcessManager() override;

    void processFrame(ImageData &frame, const ProcessConfig &config);

private:
    static QImage applyVerticalBinning(const QImage &image, int startRow, int endRow);
    static QImage applyDarkFrameSubtraction(const QImage &image, const QImage &darkFrame);
    static QImage applyFlatFieldCorrection(const QImage &image, const QImage &flatField);
};

Q_DECLARE_OPERATORS_FOR_FLAGS(PostProcessManager::Operations)

#endif