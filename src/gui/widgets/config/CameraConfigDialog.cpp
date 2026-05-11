#include "CameraConfigDialog.h"
#include "CameraConfigDialogUi.h"
#include "CameraTab.h"
#include "PluginTab.h"
#include "../../AppController.h"

#include <QShowEvent>
#include <QHash>
#include <QVariant>
#include <QPushButton>
#include <QTimer>
#include <QMetaObject>
#include <QObject>
#include <QDebug>

class CameraWorker;

// Filter out non-driver params (captureCount, captureMode are UI-layer concepts)
static QVariantMap filterDriverParams(const QVariantMap &params)
{
    QVariantMap filtered;
    for (auto it = params.constBegin(); it != params.constEnd(); ++it) {
        if (it.key() == QStringLiteral("captureCount") ||
            it.key() == QStringLiteral("captureMode")) {
            continue;
        }
        filtered.insert(it.key(), it.value());
    }
    return filtered;
}

CameraConfigDialog::CameraConfigDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new CameraConfigDialogUi(this))
{
    ui->setupUi(this);
    m_workerThread = new QThread(this);
}

CameraConfigDialog::~CameraConfigDialog()
{
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait();
    }
}

void CameraConfigDialog::showEvent(QShowEvent *event)
{
    if (event->type() == QEvent::Show && ui && ui->cameraTab) {
        if (ui->cameraTab->parameterGroup) {
            ui->cameraTab->parameterGroup->setVisible(true);
        }
        ui->cameraTab->loadCameraMetadata();
    }
    QDialog::showEvent(event);
}

void CameraConfigDialog::setAppController(AppController *controller)
{
    if (ui) {
        ui->setAppController(controller);
    }

    if (controller && ui && ui->cameraTab) {
        m_worker = new CameraWorker(controller, nullptr);
        m_worker->moveToThread(m_workerThread);
        connect(m_worker, &CameraWorker::parametersCommitted,
                this, &CameraConfigDialog::onWorkerParametersCommitted,
                Qt::QueuedConnection);
        m_workerThread->start();
    }
}

AppController *CameraConfigDialog::appController() const
{
    if (ui && ui->cameraTab) {
        return ui->cameraTab->appController();
    }
    return nullptr;
}

int CameraConfigDialog::getCaptureCount() const
{
    if (ui && ui->cameraTab) {
        return ui->cameraTab->getCaptureCount();
    }
    return 0;
}

void CameraConfigDialog::on_buttonBox_accepted()
{
    if (!ui || !ui->cameraTab) {
        accept();
        return;
    }

    ui->cameraTab->updateBufferedConfigFromWidgets();
    ui->cameraTab->saveCameraMetadata();
    QVariantMap bufferedConfig = ui->cameraTab->getBufferedConfig();

    AppController *controller = ui->cameraTab->appController();
    if (controller && controller->isConnected() && m_workerThread && m_workerThread->isRunning()) {
        QVariantMap driverParams = filterDriverParams(bufferedConfig);
        if (!driverParams.isEmpty()) {
            ui->showLoading("Applying parameters...");
            m_pendingConfig.clear();
            for (auto it = driverParams.constBegin(); it != driverParams.constEnd(); ++it) {
                m_pendingConfig.insert(it.key(), it.value());
            }
            m_acceptAfterCommit = true;

            if (m_worker) {
                QMetaObject::invokeMethod(m_worker, "doSetParameters",
                    Qt::QueuedConnection,
                    Q_ARG(QVariantMap, driverParams));
            } else {
                ui->hideLoading();
                accept();
            }
            return;
        }
    }

    accept();
}

void CameraConfigDialog::on_buttonBox_rejected()
{
    reject();
}

void CameraConfigDialog::on_buttonBox_clicked(QAbstractButton *button)
{
    if (!button || !ui) {
        return;
    }

    QDialogButtonBox::ButtonRole role = ui->buttonBox->buttonRole(button);
    if (role == QDialogButtonBox::ApplyRole) {
        if (!ui->cameraTab) {
            return;
        }

        ui->cameraTab->updateBufferedConfigFromWidgets();
        ui->cameraTab->saveCameraMetadata();
        QVariantMap bufferedConfig = ui->cameraTab->getBufferedConfig();

        AppController *controller = ui->cameraTab->appController();
        if (controller && controller->isConnected() && m_workerThread && m_workerThread->isRunning()) {
            QVariantMap driverParams = filterDriverParams(bufferedConfig);
            if (!driverParams.isEmpty()) {
                ui->showLoading("Applying parameters...");
                m_pendingConfig.clear();
                for (auto it = driverParams.constBegin(); it != driverParams.constEnd(); ++it) {
                    m_pendingConfig.insert(it.key(), it.value());
                }
                m_acceptAfterCommit = false;

                if (m_worker) {
                    QMetaObject::invokeMethod(m_worker, "doSetParameters",
                        Qt::QueuedConnection,
                        Q_ARG(QVariantMap, driverParams));
                } else {
                    ui->hideLoading();
                }
            }
        }
    }
}

void CameraConfigDialog::on_restoreButton_clicked()
{
    if (ui && ui->cameraTab) {
        AppController *controller = ui->cameraTab->appController();
        if (controller && controller->isConnected()) {
            QVariantMap currentParams = controller->allParameters();
            ui->cameraTab->setBufferedConfig(currentParams);
            ui->cameraTab->buildDynamicParameterPanel();
        }
    }
}

void CameraConfigDialog::onWorkerParametersCommitted(bool success, const QString &error)
{
    if (!success) {
        qWarning() << "CameraConfigDialog: parameter commit failed:" << error;
    }
    ui->hideLoading();
    m_pendingConfig.clear();

    if (m_acceptAfterCommit) {
        m_acceptAfterCommit = false;
        accept();
    }
}
