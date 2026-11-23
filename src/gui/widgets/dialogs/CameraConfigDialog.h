#ifndef CAMERACONFIGDIALOG_H
#define CAMERACONFIGDIALOG_H

#include <QDialog>
#include <QHash>
#include <QMap>
#include <QStringList>
#include <QVariantMap>

#include "CameraTypes.h"

class QAbstractButton;
class QCheckBox;
class QComboBox;
class QDialogButtonBox;
class QEvent;
class QFormLayout;
class QGroupBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QShowEvent;
class QSpinBox;
class QTableWidget;
class QTabWidget;
class QTimer;
class QToolButton;
class QVBoxLayout;

class AppController;
class LoadingIndicator;
class ParameterHistoryStore;
class ParameterHistoryWindow;

//! Camera configuration window.
//!
//! This is intentionally a single class/file, like CalibrationDialog: the
//! Camera/Data/Plugins pages are private sections of one dialog, not separate
//! tab classes. The setupUi() helper builds the whole window; the setup*Page()
//! helpers keep each tab's construction in its own function.
//!
//! The window is a view + parameter staging area. Camera lifecycle, plugin
//! loading and parameter commits stay in AppController.
class CameraConfigDialog : public QDialog
{
    Q_OBJECT

public:
    explicit CameraConfigDialog(QWidget *parent = nullptr);
    ~CameraConfigDialog() override;

    void setAppController(AppController *controller);
    AppController *appController() const { return m_appController; }

    int getCaptureCount() const;
    QString getCaptureMode() const;
    void setCaptureMode(const QString &mode);

    //! Auto-save editor API used by MainWindow's File menu actions.
    bool isAutoSaveEnabled() const;
    void setAutoSaveEnabled(bool enabled);
    QString autoSaveDirectory() const;
    void setAutoSaveDirectory(const QString &directory);

    //! Build the dynamic parameter panel from the controller's definitions and
    //! make it visible. Called from showEvent(), because the dialog may open
    //! long after the camera connected.
    void showParameterPanel();

signals:
    void captureModeChanged(const QString &mode);
    void autoSaveToggled(bool enabled);

protected:
    void showEvent(QShowEvent *event) override;
    bool eventFilter(QObject *obj, QEvent *event) override;

private slots:
    // ----- dialog buttons / parameter commit ---------------------------
    void on_buttonBox_accepted();
    void on_buttonBox_rejected();
    void on_buttonBox_clicked(QAbstractButton *button);
    void on_restoreButton_clicked();
    void onSetParametersFinished(const QStringList &failedParameters);
    void onCommitParametersFinished(const QStringList &failedParameters);

    // ----- Camera page --------------------------------------------------
    void onScanButtonClicked();
    void onConnectButtonClicked();
    void onDisconnectButtonClicked();
    void onCameraSelected(int index);
    void onCameraStateChanged(CameraState state);
    void onCaptureModeChanged(int index);
    void onConnectCameraFinished(const QString &cameraId, bool success,
                                 const QString &error);
    void onDisconnectCameraFinished(const QString &cameraId);
    void onTelemetryTimerTimeout();

    // ----- Plugins page / shared scan status ----------------------------
    void onScanPluginsClicked();
    void onScanStarted();
    void onScanProgress(int current, int total, const QString &currentFile);
    void onScanCompleted(int totalPlugins, int loadedPlugins);
    void onPluginLoadFailed(const QString &filePath, const QString &error);
    void onBrowsePluginDirectoryClicked();

    // ----- Data page -----------------------------------------------------
    void onBrowseDirectoryClicked();
    void onAutoSaveToggled(bool checked);
    void onImageFormatChanged(const QString &format);
    void onPrefixChanged(const QString &prefix);
    void onSuffixChanged(const QString &suffix);

private:
    //! createParameterField() 的产物。曲线按钮和值标签只在数值型遥测参数上存在。
    struct ParameterField
    {
        QWidget *widget = nullptr;
        QLabel *label = nullptr;
        QToolButton *button = nullptr;
    };

    // ----- layout --------------------------------------------------------
    void setupUi();
    void setupCameraPage();
    void setupDataPage();
    void setupPluginsPage();
    void setupButtonBox();
    void setupLoadingRow(QVBoxLayout *root);

    // ----- camera / plugin state ----------------------------------------
    void updateConnectionState();
    void refreshCameraList();
    void startPluginScan();
    void updatePluginsTable();

    // ----- dynamic parameter panel --------------------------------------
    void setBufferedConfig(const QVariantMap &config);
    QVariantMap getBufferedConfig() const;
    void updateBufferedConfigFromWidgets();
    void buildDynamicParameterPanel();
    void clearDynamicParameterPanel();
    void rebuildParameterWidget(const QString &paramName);
    ParameterField createParameterField(const QString &paramName,
                                        const ParameterDefinition &def);
    void registerParameterField(const QString &paramName, const ParameterField &field);
    void unregisterParameterField(const QString &paramName);
    void restoreWidgetsFromConfig(const QVariantMap &config,
                                  const QStringList &onlyNames = QStringList());
    void refreshCommittedConfigFromController();
    void updatePlotButtonIcons();
    void openHistoryWindow(const QString &paramName);
    void onParametersCommitted();

    // ----- data page -----------------------------------------------------
    void loadDataSettings();

    // ----- dialog helpers ------------------------------------------------
    void showDialogLoading(const QString &text);
    void hideDialogLoading();

    AppController *m_appController = nullptr;

    // ----- dialog chrome -------------------------------------------------
    QTabWidget *m_tabs = nullptr;
    QDialogButtonBox *m_buttonBox = nullptr;
    QPushButton *m_restoreButton = nullptr;
    LoadingIndicator *m_dialogLoadingIndicator = nullptr;
    QLabel *m_loadingStatusLabel = nullptr;

    // ----- Camera page ---------------------------------------------------
    QComboBox *m_cameraComboBox = nullptr;
    QPushButton *m_scanButton = nullptr;
    QPushButton *m_connectButton = nullptr;
    QPushButton *m_disconnectButton = nullptr;
    QLabel *m_statusLabel = nullptr;
    LoadingIndicator *m_connectLoadingIndicator = nullptr;
    QLabel *m_scanStatusLabel = nullptr;
    QComboBox *m_captureModeComboBox = nullptr;
    QSpinBox *m_captureCountSpinBox = nullptr;
    QFormLayout *m_cameraForm = nullptr;
    int m_countRow = -1;
    QGroupBox *m_parameterGroup = nullptr;
    QVBoxLayout *m_dynamicParametersLayout = nullptr;

    // ----- Data page -----------------------------------------------------
    QLineEdit *m_autoSaveDirectoryLineEdit = nullptr;
    QPushButton *m_browseDirectoryButton = nullptr;
    QCheckBox *m_autoSaveEnabledCheckBox = nullptr;
    QComboBox *m_imageFormatComboBox = nullptr;
    QLineEdit *m_prefixLineEdit = nullptr;
    QLineEdit *m_suffixLineEdit = nullptr;

    // ----- Plugins page --------------------------------------------------
    QLineEdit *m_pluginDirectoryLineEdit = nullptr;
    QPushButton *m_browsePluginDirectoryButton = nullptr;
    QPushButton *m_scanPluginsButton = nullptr;
    QTableWidget *m_pluginsTableWidget = nullptr;
    QLabel *m_pluginScanStatusLabel = nullptr;

    // ----- camera / parameter state -------------------------------------
    QVariantMap m_bufferedConfig;
    QVariantMap m_committedConfig;
    QHash<QString, QWidget *> m_parameterWidgets;
    QMap<ParameterCategory, QGroupBox *> m_categoryGroups;
    QHash<QString, ParameterDefinition> m_parameterDefinitions;
    QTimer *m_telemetryTimer = nullptr;
    ParameterHistoryStore *m_history = nullptr;
    QHash<QString, QLabel *> m_telemetryLabels;
    QHash<QString, QToolButton *> m_plotButtons;
    QHash<QString, ParameterHistoryWindow *> m_historyWindows;
    int m_lastScanFailed = 0;

    // ----- parameter commit state ---------------------------------------
    bool m_acceptAfterCommit = false;
};

#endif // CAMERACONFIGDIALOG_H
