#ifndef CAMERATABUI_H
#define CAMERATABUI_H

#include <QObject>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QComboBox>
#include <QSpinBox>
#include <QScrollArea>
#include <QGroupBox>
#include <QPushButton>
#include <QLabel>

#include "../widgets/config/LoadingIndicator.h"

class CameraTab;

class CameraTabUi : public QObject
{
    Q_OBJECT
public:
    explicit CameraTabUi(QObject *parent = nullptr);
    ~CameraTabUi();

    void setupUi(CameraTab *tab);

    QPushButton *scanButton = nullptr;
    QPushButton *connectButton = nullptr;
    QPushButton *disconnectButton = nullptr;
    QComboBox *cameraComboBox = nullptr;
    QComboBox *captureModeComboBox = nullptr;
    QSpinBox *captureCountSpinBox = nullptr;
    QFormLayout *formLayout = nullptr;
    int m_countRow = -1;
    QGroupBox *parameterGroup = nullptr;

    QLabel *m_statusLabel = nullptr;
    LoadingIndicator *m_loadingIndicator = nullptr;
    QLabel *m_scanStatusLabel = nullptr;
    QVBoxLayout *m_dynamicParametersLayout = nullptr;
    QScrollArea *m_scrollArea = nullptr;
};

#endif // CAMERATABUI_H