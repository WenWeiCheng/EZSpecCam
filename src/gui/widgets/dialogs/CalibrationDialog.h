#ifndef CALIBRATIONDIALOG_H
#define CALIBRATIONDIALOG_H

#include <QCheckBox>
#include <QDialog>
#include <QLineEdit>
#include <QVector>

#include "Calibration.h"

class QComboBox;
class QDoubleSpinBox;
class QFormLayout;
class QLabel;
class QListWidget;
class QPushButton;
class QSpinBox;
class QStackedWidget;
class QTabWidget;

/**
 * @brief Dark-frame and flat-field calibration configuration window.
 *
 * The window is a pure editor: reference images live in MainWindow, and the
 * dialog only shows their state and asks for the manual exposure times. Two
 * methods per correction are available, mirroring the reference scripts:
 *
 *  - Dark: matched exposure subtraction, or bias + long dark scaled by t/t0.
 *  - Flat: classic mean normalization, or variable-exposure per-pixel fit.
 *
 * Every reference has an Acquire button (uses the current camera lighting)
 * and a Browse button (loads a saved TIFF/CSV image). Acquired references stay
 * in memory until the application exits; use Save Frame to persist them.
 */
class CalibrationDialog : public QDialog
{
    Q_OBJECT

public:
    //! Number of frames one burst may acquire. Shared with MainWindow's clamp.
    static constexpr int kMaxFrameCount = 1000;

    //! Which reference slot an Acquire click is filling.
    enum class AcquireKind
    {
        MatchedDark = static_cast<int>(Calibration::ReferenceKind::MatchedDark),
        Bias = static_cast<int>(Calibration::ReferenceKind::Bias),
        LongDark = static_cast<int>(Calibration::ReferenceKind::LongDark),
        Flat = static_cast<int>(Calibration::ReferenceKind::Flat),
        VffEntry = static_cast<int>(Calibration::ReferenceKind::VffEntry)
    };

    explicit CalibrationDialog(QWidget *parent = nullptr);
    ~CalibrationDialog() override;

    // ----- Dark-frame configuration -------------------------------------
    void setDarkFrameEnabled(bool enabled);
    bool darkFrameEnabled() const;

    void setDarkFramePath(const QString &path);
    QString darkFramePath() const;

    //! Legacy helper: shows the in-memory marker without writing a path.
    void setInMemoryDarkFrameUsed(bool used, int frameCount = 0);

    void setCustomBias(int bias);
    int customBias() const;

    void setDarkMethod(Calibration::DarkMethod method);
    Calibration::DarkMethod darkMethod() const;

    void setMatchedDarkExposure(double seconds);
    double matchedDarkExposure() const;

    void setLongDarkExposure(double seconds);
    double longDarkExposure() const;

    //! Exposure of the signal being corrected, entered by the user.
    void setTargetExposure(double seconds);
    double targetExposure() const;

    // ----- Flat-field configuration -------------------------------------
    void setFlatEnabled(bool enabled);
    bool flatEnabled() const;

    void setFlatMethod(Calibration::FlatMethod method);
    Calibration::FlatMethod flatMethod() const;

    void setFlatExposure(double seconds);
    double flatExposure() const;

    //! Exposure used when the VFF Acquire/Load button is pressed.
    void setVffExposure(double seconds);
    double vffExposure() const;

    // ----- Reference status ---------------------------------------------
    //! Update one source row. @p path is shown only when !inMemory.
    void setReferenceStatus(Calibration::ReferenceKind kind, bool valid, bool inMemory,
                            int frameCount, const QString &path);
    //! Rebuild the variable-exposure list from the current references.
    void setVffEntries(const QVector<Calibration::ReferenceFrame> &entries);
    //! Diagnostic line shown under the flat method selector.
    void setFlatFieldStatus(const QString &text, bool ok);

    // ----- Acquisition / apply ------------------------------------------
    int frameCount() const;
    void setFrameCount(int n);

    //! Which slot the most recent Acquire click belongs to.
    AcquireKind acquisitionKind() const { return m_pendingKind; }
    //! Exposure associated with that slot (manual input).
    double acquisitionExposure() const;

    void setAcquireInProgress(bool busy);
    void setAcquireEnabled(bool enabled);

signals:
    //! Legacy signal kept for existing callers/tests: enabled, path, bias.
    void applied(bool enabled, const QString &filePath, int customBias);
    //! Emitted by any Acquire button. Query acquisitionKind()/acquisitionExposure().
    void acquireRequested(int frameCount);
    //! Emitted after the user picks a saved reference image.
    void referenceFileSelected(int kind, const QString &path);
    void vffRemoveRequested(int index);
    void vffClearRequested();

private slots:
    void onOkClicked();
    void onApplyClicked();
    void onBrowseClicked();

private:
    void setupUi();
    void setupDarkTab(QTabWidget *tabs);
    void setupFlatTab(QTabWidget *tabs);
    void wireAcquire(QPushButton *button, AcquireKind kind);
    void wireBrowse(QPushButton *button, AcquireKind kind);
    void beginAcquisition(AcquireKind kind);
    QLineEdit *lineEditForKind(Calibration::ReferenceKind kind) const;
    QPushButton *acquireButtonForKind(Calibration::ReferenceKind kind) const;
    QDoubleSpinBox *exposureSpinForKind(Calibration::ReferenceKind kind) const;
    QString inMemoryPlaceholder(Calibration::ReferenceKind kind, int frameCount) const;

    QTabWidget *m_tabs = nullptr;

    // Dark tab
    QCheckBox *m_enableCheckBox = nullptr;
    QComboBox *m_darkMethodCombo = nullptr;
    QStackedWidget *m_darkMethodStack = nullptr;

    QLineEdit *m_matchedDarkPath = nullptr;
    QLineEdit *m_biasPath = nullptr;
    QLineEdit *m_longDarkPath = nullptr;
    QPushButton *m_matchedBrowseButton = nullptr;
    QPushButton *m_matchedAcquireButton = nullptr;
    QPushButton *m_biasBrowseButton = nullptr;
    QPushButton *m_biasAcquireButton = nullptr;
    QPushButton *m_longDarkBrowseButton = nullptr;
    QPushButton *m_longDarkAcquireButton = nullptr;
    QDoubleSpinBox *m_matchedExposureSpin = nullptr;
    QDoubleSpinBox *m_longDarkExposureSpin = nullptr;
    QDoubleSpinBox *m_targetExposureSpin = nullptr;

    QSpinBox *m_biasSpinBox = nullptr;
    QSpinBox *m_frameCountSpinBox = nullptr;

    // Flat tab
    QCheckBox *m_flatEnableCheckBox = nullptr;
    QComboBox *m_flatMethodCombo = nullptr;
    QStackedWidget *m_flatMethodStack = nullptr;
    QLineEdit *m_flatPath = nullptr;
    QPushButton *m_flatBrowseButton = nullptr;
    QPushButton *m_flatAcquireButton = nullptr;
    QDoubleSpinBox *m_flatExposureSpin = nullptr;
    QDoubleSpinBox *m_vffExposureSpin = nullptr;
    QPushButton *m_vffBrowseButton = nullptr;
    QPushButton *m_vffAcquireButton = nullptr;
    QListWidget *m_vffList = nullptr;
    QPushButton *m_vffRemoveButton = nullptr;
    QPushButton *m_vffClearButton = nullptr;
    QLabel *m_flatStatusLabel = nullptr;

    QPushButton *m_okButton = nullptr;
    QPushButton *m_applyButton = nullptr;
    QPushButton *m_cancelButton = nullptr;

    AcquireKind m_pendingKind = AcquireKind::MatchedDark;
    bool m_acquireInProgress = false;
    bool m_acquireEnabled = true;
};

#endif // CALIBRATIONDIALOG_H
