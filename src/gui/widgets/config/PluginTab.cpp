#include "PluginTab.h"
#include "CameraTab.h"
#include "../../AppController.h"

#include <QFileInfo>
#include <QDir>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QFileDialog>
#include <QSettings>
#include <QHeaderView>
#include <QAbstractItemView>
#include <qobjectdefs.h>

PluginTab::PluginTab(QWidget *parent)
    : QWidget(parent)
    , m_appController(nullptr)
    , m_cameraTab(nullptr)
    , pluginDirectoryLineEdit(nullptr)
    , browsePluginDirectoryButton(nullptr)
    , scanPluginsButton(nullptr)
    , pluginsTableWidget(nullptr)
{
    setupUi();
}

PluginTab::~PluginTab()
{
}

void PluginTab::setCameraTab(CameraTab *cameraTab)
{
    m_cameraTab = cameraTab;
}

void PluginTab::setAppController(AppController *controller)
{
    m_appController = controller;

    if (controller) {
        QSettings settings;
        QString pluginDir = settings.value("plugins/pluginDirectory", "").toString();
        if (!pluginDir.isEmpty()) {
            pluginDirectoryLineEdit->setText(pluginDir);
        }

        connect(controller, &AppController::pluginScanCompleted,
                this, &PluginTab::onScanCompleted);
        connect(controller, &AppController::pluginLoadFailed,
                this, &PluginTab::onPluginLoadFailed);

        if (!pluginDir.isEmpty() && QDir(pluginDir).exists()) {
            QMetaObject::invokeMethod(controller, &AppController::scanPlugins, Qt::QueuedConnection);
        }
    }
}

AppController *PluginTab::appController() const
{
    return m_appController;
}

void PluginTab::setupUi()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    QGroupBox *pluginDirectoryGroup = new QGroupBox("Plugin Directory", this);

    QHBoxLayout *directoryLayout = new QHBoxLayout();
    pluginDirectoryLineEdit = new QLineEdit(this);
    pluginDirectoryLineEdit->setPlaceholderText("Select plugin directory...");
    browsePluginDirectoryButton = new QPushButton("Browse...", this);
    scanPluginsButton = new QPushButton("Scan Plugins", this);
    directoryLayout->addWidget(pluginDirectoryLineEdit);
    directoryLayout->addWidget(browsePluginDirectoryButton);
    directoryLayout->addWidget(scanPluginsButton);

    pluginDirectoryGroup->setLayout(directoryLayout);
    mainLayout->addWidget(pluginDirectoryGroup);

    QGroupBox *loadedPluginsGroup = new QGroupBox("Available Cameras", this);
    QVBoxLayout *pluginsLayout = new QVBoxLayout();

    pluginsTableWidget = new QTableWidget(this);
    pluginsTableWidget->setColumnCount(2);
    pluginsTableWidget->setHorizontalHeaderLabels({"Plugin", "Camera ID"});
    pluginsTableWidget->horizontalHeader()->setStretchLastSection(true);
    pluginsTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    pluginsTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);

    pluginsLayout->addWidget(pluginsTableWidget);
    loadedPluginsGroup->setLayout(pluginsLayout);
    mainLayout->addWidget(loadedPluginsGroup);

    QSettings settings;
    QString pluginDir = settings.value("plugins/pluginDirectory", "").toString();
    if (!pluginDir.isEmpty()) {
        pluginDirectoryLineEdit->setText(pluginDir);
    }

    connect(browsePluginDirectoryButton, &QPushButton::clicked,
            this, &PluginTab::onBrowseClicked);
    connect(scanPluginsButton, &QPushButton::clicked,
            this, &PluginTab::onScanClicked);
    connect(pluginDirectoryLineEdit, &QLineEdit::returnPressed,
            this, &PluginTab::onScanClicked);
}

void PluginTab::onBrowseClicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Select Plugin Directory",
                                                    pluginDirectoryLineEdit->text());
    if (!dir.isEmpty()) {
        pluginDirectoryLineEdit->setText(dir);
        QSettings settings;
        settings.setValue("plugins/pluginDirectory", dir);
    }
}

void PluginTab::onScanClicked()
{
    if (!m_appController) {
        return;
    }

    QString pluginDir = pluginDirectoryLineEdit->text();
    if (!pluginDir.isEmpty()) {
        QSettings settings;
        settings.setValue("plugins/pluginDirectory", pluginDir);
    }

    QMetaObject::invokeMethod(m_appController, &AppController::scanPlugins, Qt::QueuedConnection);
}

void PluginTab::onScanCompleted(int totalPlugins, int loadedPlugins)
{
    Q_UNUSED(totalPlugins);
    Q_UNUSED(loadedPlugins);
    updatePluginsTable();

    if (m_cameraTab) {
        m_cameraTab->refreshCameraList();
    }
}

void PluginTab::onPluginLoadFailed(const QString &filePath, const QString &error)
{
    Q_UNUSED(filePath);
    Q_UNUSED(error);
    updatePluginsTable();
}

void PluginTab::updatePluginsTable()
{
    pluginsTableWidget->setRowCount(0);

    if (!m_appController) {
        return;
    }

    QStringList cameras = m_appController->availableCameras();
    if (cameras.isEmpty()) {
        return;
    }

    QMap<QString, QStringList> pluginCameras;
    for (const QString &cameraId : cameras) {
        int dashIndex = cameraId.lastIndexOf('-');
        QString pluginName = (dashIndex > 0) ? cameraId.left(dashIndex) : cameraId;
        pluginCameras[pluginName].append(cameraId);
    }

    int row = 0;
    for (auto it = pluginCameras.constBegin(); it != pluginCameras.constEnd(); ++it) {
        pluginsTableWidget->insertRow(row);
        QTableWidgetItem *pluginItem = new QTableWidgetItem(it.key());
        QTableWidgetItem *camerasItem = new QTableWidgetItem(it.value().join(", "));
        pluginsTableWidget->setItem(row, 0, pluginItem);
        pluginsTableWidget->setItem(row, 1, camerasItem);
        ++row;
    }
}