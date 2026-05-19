#include "CameraConfigDialog.h"
#include "../../ui/CameraConfigDialogUi.h"
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
#include <qcontainerfwd.h>
#include <qvariant.h>

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
}

CameraConfigDialog::~CameraConfigDialog()
{
}

void CameraConfigDialog::showEvent(QShowEvent *event)
{
    if (event->type() == QEvent::Show && ui && ui->cameraTab) {
        if (ui->cameraTab->ui->parameterGroup) {
            AppController *controller = ui->cameraTab->appController();
            QVariantMap currentParams = controller->allParameters();

            ui->cameraTab->setBufferedConfig(currentParams);
            ui->cameraTab->buildDynamicParameterPanel();
            ui->cameraTab->ui->parameterGroup->setVisible(true);
        }
    }
    QDialog::showEvent(event);
}

void CameraConfigDialog::setAppController(AppController *controller)
{
    if (ui) {
        ui->setAppController(controller);
    }

    if (controller) {
        connect(controller, &AppController::setParametersFinished,
                this, &CameraConfigDialog::onSetParametersFinished);
        connect(controller, &AppController::commitParametersFinished,
                this, &CameraConfigDialog::onCommitParametersFinished);
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
    QVariantMap bufferedConfig = ui->cameraTab->getBufferedConfig();

    AppController *controller = ui->cameraTab->appController();
    if (controller && controller->isConnected()) {
        QVariantMap driverParams = filterDriverParams(bufferedConfig);
        if (!driverParams.isEmpty()) {
            ui->showLoading("Applying parameters...");
            m_pendingConfig = driverParams;
            m_acceptAfterCommit = true;

            QMetaObject::invokeMethod(controller, "setParameters",
                                     Qt::QueuedConnection,
                                     Q_ARG(QVariantMap, driverParams));
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
        if (controller && controller->isConnected()) {
            QVariantMap driverParams = filterDriverParams(bufferedConfig);
            if (!driverParams.isEmpty()) {
                ui->showLoading("Applying parameters...");
                m_pendingConfig = driverParams;
                m_acceptAfterCommit = false;

                QMetaObject::invokeMethod(controller, "setParameters",
                                         Qt::QueuedConnection,
                                         Q_ARG(QVariantMap, driverParams));
            }
        }
    }
}

void CameraConfigDialog::on_restoreButton_clicked()
{
    if (ui && ui->cameraTab) {
        AppController *controller = ui->cameraTab->appController();
        if (controller && controller->isConnected()) {
            QVariantMap defaultParams;
            QStringList paramNames = controller->parameterNames();

            for (const QString &paramName : paramNames) {
                ParameterDefinition def = controller->parameter(paramName);
                defaultParams[paramName] = def.defaultValue;
            }

            ui->cameraTab->setBufferedConfig(defaultParams);
            ui->cameraTab->buildDynamicParameterPanel();
        }
    }
}

void CameraConfigDialog::onSetParametersFinished(bool success)
{
    AppController *controller = appController();
    if (!controller || !ui) {
        ui->hideLoading();
        return;
    }

    if (!success) {
        qWarning() << "CameraConfigDialog: setParameters failed";
        ui->hideLoading();
        m_pendingConfig.clear();
        if (m_acceptAfterCommit) {
            m_acceptAfterCommit = false;
            accept();
        }
        return;
    }

    QMetaObject::invokeMethod(controller, "commitParameters",
                             Qt::QueuedConnection);
}

void CameraConfigDialog::onCommitParametersFinished(bool success)
{
    if (!ui) {
        return;
    }

    if (!success) {
        qWarning() << "CameraConfigDialog: commitParameters failed";
    }

    ui->hideLoading();
    m_pendingConfig.clear();

    if (m_acceptAfterCommit) {
        m_acceptAfterCommit = false;
        accept();
    }
}
