#include "CameraConfigDialog.h"
#include "CameraConfigDialogUi.h"
#include "CameraTab.h"
#include "PluginTab.h"
#include "../AppController.h"

#include <QShowEvent>
#include <QHash>
#include <QVariant>
#include <QPushButton>
#include <QTimer>
#include <QMetaObject>
#include <QObject>

class CameraWorker;

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
    }
    QDialog::showEvent(event);
}

void CameraConfigDialog::setAppController(AppController *controller)
{
    if (ui) {
        ui->setAppController(controller);
    }

    if (controller && ui && ui->cameraTab) {
        CameraWorker *worker = new CameraWorker(controller, nullptr);
        worker->moveToThread(m_workerThread);
        connect(worker, &CameraWorker::parametersCommitted,
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

void CameraConfigDialog::on_buttonBox_accepted()
{
    if (!ui || !ui->cameraTab) {
        accept();
        return;
    }

    ui->cameraTab->updateBufferedConfigFromWidgets();
    QVariantMap bufferedConfig = ui->cameraTab->getBufferedConfig();

    AppController *controller = ui->cameraTab->appController();
    if (controller && controller->isConnected() && m_workerThread && m_workerThread->isRunning()) {
        if (!bufferedConfig.isEmpty()) {
            ui->showLoading("Applying parameters...");
            m_pendingConfig.clear();
            for (auto it = bufferedConfig.constBegin(); it != bufferedConfig.constEnd(); ++it) {
                m_pendingConfig.insert(it.key(), it.value());
            }
            m_acceptAfterCommit = true;

            CameraWorker *worker = m_workerThread->findChild<CameraWorker*>();
            if (worker) {
                QMetaObject::invokeMethod(worker, "doSetParameters",
                    Qt::QueuedConnection,
                    Q_ARG(QVariantMap, bufferedConfig));
            } else {
                controller->setParameters(bufferedConfig);
                controller->commitParameters();
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
        QVariantMap bufferedConfig = ui->cameraTab->getBufferedConfig();

        AppController *controller = ui->cameraTab->appController();
        if (controller && controller->isConnected() && m_workerThread && m_workerThread->isRunning()) {
            if (!bufferedConfig.isEmpty()) {
                ui->showLoading("Applying parameters...");
                m_pendingConfig.clear();
                for (auto it = bufferedConfig.constBegin(); it != bufferedConfig.constEnd(); ++it) {
                    m_pendingConfig.insert(it.key(), it.value());
                }
                m_acceptAfterCommit = false;

                CameraWorker *worker = m_workerThread->findChild<CameraWorker*>();
                if (worker) {
                    QMetaObject::invokeMethod(worker, "doSetParameters",
                        Qt::QueuedConnection,
                        Q_ARG(QVariantMap, bufferedConfig));
                } else {
                    controller->setParameters(bufferedConfig);
                    controller->commitParameters();
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
    Q_UNUSED(error);
    ui->hideLoading();
    m_pendingConfig.clear();

    if (m_acceptAfterCommit) {
        m_acceptAfterCommit = false;
        accept();
    }
}