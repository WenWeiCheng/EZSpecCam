#include "CameraConfigDialogUi.h"
#include "../widgets/config/CameraConfigDialog.h"
#include "../widgets/config/CameraTab.h"
#include "../widgets/config/DataTab.h"
#include "../widgets/config/PluginTab.h"
#include "../widgets/config/LoadingIndicator.h"
#include "../AppController.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>

CameraConfigDialogUi::CameraConfigDialogUi(QObject *parent)
    : m_parent(parent)
    , tabWidget(nullptr)
    , buttonBox(nullptr)
    , loadingIndicator(nullptr)
    , loadingStatusLabel(nullptr)
    , cameraTab(nullptr)
    , dataTab(nullptr)
    , pluginTab(nullptr)
{
}

CameraConfigDialogUi::~CameraConfigDialogUi()
{
}

void CameraConfigDialogUi::setAppController(AppController *controller)
{
    if (cameraTab) {
        cameraTab->setAppController(controller);
    }
    if (pluginTab) {
        pluginTab->setAppController(controller);
    }
}

void CameraConfigDialogUi::showLoading(const QString &statusText)
{
    if (loadingIndicator) {
        loadingIndicator->setVisible(true);
        loadingIndicator->startAnimation();
    }
    if (loadingStatusLabel) {
        loadingStatusLabel->setText(statusText);
        loadingStatusLabel->setVisible(true);
    }
    if (buttonBox) {
        buttonBox->setEnabled(false);
    }
}

void CameraConfigDialogUi::hideLoading()
{
    if (loadingIndicator) {
        loadingIndicator->stopAnimation();
        loadingIndicator->setVisible(false);
    }
    if (loadingStatusLabel) {
        loadingStatusLabel->setVisible(false);
    }
    if (buttonBox) {
        buttonBox->setEnabled(true);
    }
}

void CameraConfigDialogUi::setupUi(QDialog *dialog)
{
    dialog->setWindowTitle("Configuration");
    dialog->setMinimumSize(600, 500);

    createTabs(dialog);
    createButtonBox(dialog);

    loadingIndicator = new LoadingIndicator(dialog);
    loadingIndicator->setFixedSize(24, 24);
    loadingIndicator->setVisible(false);

    loadingStatusLabel = new QLabel(dialog);
    loadingStatusLabel->setVisible(false);

    QHBoxLayout *loadingLayout = new QHBoxLayout();
    loadingLayout->addWidget(loadingIndicator);
    loadingLayout->addWidget(loadingStatusLabel);
    loadingLayout->addStretch();

    QVBoxLayout *mainLayout = new QVBoxLayout();
    mainLayout->addWidget(tabWidget);
    mainLayout->addLayout(loadingLayout);
    mainLayout->addWidget(buttonBox);
    dialog->setLayout(mainLayout);
}

void CameraConfigDialogUi::createTabs(QDialog *dialog)
{
    Q_UNUSED(dialog);

    tabWidget = new QTabWidget();

    cameraTab = new CameraTab(tabWidget);
    dataTab = new DataTab(tabWidget);
    pluginTab = new PluginTab(tabWidget);
    pluginTab->setCameraTab(cameraTab);

    tabWidget->addTab(cameraTab, "Camera");
    tabWidget->addTab(dataTab, "Data");
    tabWidget->addTab(pluginTab, "Plugins");
}

void CameraConfigDialogUi::createButtonBox(QDialog *dialog)
{
    buttonBox = new QDialogButtonBox();
    buttonBox->setOrientation(Qt::Horizontal);

    QPushButton *applyButton = new QPushButton(QDialogButtonBox::tr("Apply"), buttonBox);
    QPushButton *okButton = new QPushButton(QDialogButtonBox::tr("OK"), buttonBox);
    QPushButton *cancelButton = new QPushButton(QDialogButtonBox::tr("Cancel"), buttonBox);
    QPushButton *restoreButton = new QPushButton("Restore", buttonBox);

    buttonBox->addButton(okButton, QDialogButtonBox::AcceptRole);
    buttonBox->addButton(cancelButton, QDialogButtonBox::RejectRole);
    buttonBox->addButton(applyButton, QDialogButtonBox::ApplyRole);
    buttonBox->addButton(restoreButton, QDialogButtonBox::ActionRole);

    CameraConfigDialog *configDialog = qobject_cast<CameraConfigDialog*>(dialog);
    if (configDialog) {
        connect(buttonBox, &QDialogButtonBox::accepted,
                configDialog, &CameraConfigDialog::on_buttonBox_accepted);
        connect(buttonBox, &QDialogButtonBox::rejected,
                configDialog, &CameraConfigDialog::on_buttonBox_rejected);
        connect(buttonBox, &QDialogButtonBox::clicked,
                configDialog, &CameraConfigDialog::on_buttonBox_clicked);
        connect(restoreButton, &QPushButton::clicked,
                configDialog, &CameraConfigDialog::on_restoreButton_clicked);
    }
}
