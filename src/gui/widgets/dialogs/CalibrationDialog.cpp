#include "CalibrationDialog.h"

#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTabWidget>
#include <QVBoxLayout>

#include "../../../gui/workers/FileLoaderWorker.h"

#include <algorithm>

namespace
{
// 留给 QLineEdit 边框和左右内边距的宽度。QLineEdit 的 sizeHint 不看 placeholder，
// 而这一行恰是采完参考帧之后用户第一眼要读的，所以要按当前字体量一次。
constexpr int kPlaceholderSlack = 16;

QString formatSeconds(double seconds)
{
    return QString::number(seconds, 'g', 6);
}
} // namespace

CalibrationDialog::CalibrationDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Calibration"));
    setMinimumWidth(560);
    // 采参考帧时主窗口还得能用（按 Stop、看实时画面），所以不能挡住主窗口
    setModal(false);

    setupUi();
}

CalibrationDialog::~CalibrationDialog() = default;

void CalibrationDialog::setupUi()
{
    QVBoxLayout *root = new QVBoxLayout(this);

    m_tabs = new QTabWidget(this);
    m_tabs->setObjectName(QStringLiteral("calibrationTabs"));
    setupDarkTab(m_tabs);
    setupFlatTab(m_tabs);
    root->addWidget(m_tabs, 1);

    // Frame count applies to every Acquire button, so it belongs outside the
    // Dark/Flat tabs instead of hiding on the dark page.
    QFormLayout *commonForm = new QFormLayout();
    m_frameCountSpinBox = new QSpinBox(this);
    m_frameCountSpinBox->setObjectName(QStringLiteral("frameCountSpinBox"));
    m_frameCountSpinBox->setMinimum(1);
    m_frameCountSpinBox->setMaximum(kMaxFrameCount);
    m_frameCountSpinBox->setValue(10);
    m_frameCountSpinBox->setSingleStep(1);
    m_frameCountSpinBox->setToolTip(
        tr("Every Acquire button averages this many frames. More frames lower the "
           "reference noise; use at least 5-10."));
    commonForm->addRow(tr("Frames to average:"), m_frameCountSpinBox);
    root->addLayout(commonForm);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    m_okButton = new QPushButton(tr("OK"), this);
    m_okButton->setObjectName(QStringLiteral("okButton"));
    m_applyButton = new QPushButton(tr("Apply"), this);
    m_applyButton->setObjectName(QStringLiteral("applyButton"));
    m_applyButton->setToolTip(
        tr("Apply the current calibration settings without closing this window."));
    m_cancelButton = new QPushButton(tr("Cancel"), this);
    m_cancelButton->setObjectName(QStringLiteral("cancelButton"));
    buttonLayout->addStretch(1);
    buttonLayout->addWidget(m_okButton);
    buttonLayout->addWidget(m_applyButton);
    buttonLayout->addWidget(m_cancelButton);
    root->addLayout(buttonLayout);

    connect(m_okButton, &QPushButton::clicked, this, &CalibrationDialog::onOkClicked);
    connect(m_applyButton, &QPushButton::clicked, this, &CalibrationDialog::onApplyClicked);
    connect(m_cancelButton, &QPushButton::clicked, this, &QDialog::reject);
}

void CalibrationDialog::setupDarkTab(QTabWidget *tabs)
{
    QWidget *page = new QWidget(tabs);
    QVBoxLayout *pageLayout = new QVBoxLayout(page);

    QFormLayout *form = new QFormLayout();

    m_enableCheckBox = new QCheckBox(tr("Enable Dark-Frame Calibration"), page);
    m_enableCheckBox->setObjectName(QStringLiteral("darkEnableCheckBox"));
    form->addRow(m_enableCheckBox);

    m_darkMethodCombo = new QComboBox(page);
    m_darkMethodCombo->setObjectName(QStringLiteral("darkMethodCombo"));
    m_darkMethodCombo->addItem(tr("Matched exposure dark frame"));
    m_darkMethodCombo->addItem(tr("Bias frame + scaled long dark"));
    m_darkMethodCombo->setToolTip(
        tr("Matched: subtract a dark frame taken at the signal exposure.\n"
           "Bias + scaled: B + (D(t0) - B) * t/t0; acquire the bias at the "
           "shortest exposure. Block all light when acquiring references."));
    form->addRow(tr("Dark method:"), m_darkMethodCombo);

    m_darkMethodStack = new QStackedWidget(page);
    m_darkMethodStack->setObjectName(QStringLiteral("darkMethodStack"));

    // ----- Method 1: matched exposure ---------------------------------
    QWidget *matchedPage = new QWidget(m_darkMethodStack);
    QFormLayout *matchedForm = new QFormLayout(matchedPage);
    matchedForm->setContentsMargins(0, 0, 0, 0);

    m_matchedDarkPath = new QLineEdit(matchedPage);
    m_matchedDarkPath->setObjectName(QStringLiteral("matchedDarkPathEdit"));
    m_matchedDarkPath->setReadOnly(true);
    m_matchedDarkPath->setPlaceholderText(tr("(no dark frame selected)"));
    const QString widestPlaceholder =
        inMemoryPlaceholder(Calibration::ReferenceKind::MatchedDark, kMaxFrameCount);
    m_matchedDarkPath->setMinimumWidth(
        m_matchedDarkPath->fontMetrics().horizontalAdvance(widestPlaceholder)
        + kPlaceholderSlack);
    m_matchedBrowseButton = new QPushButton(tr("Browse..."), matchedPage);
    m_matchedBrowseButton->setObjectName(QStringLiteral("browseButton"));
    m_matchedAcquireButton = new QPushButton(tr("Acquire"), matchedPage);
    m_matchedAcquireButton->setObjectName(QStringLiteral("acquireButton"));
    m_matchedSaveButton = new QPushButton(tr("Save As..."), matchedPage);
    m_matchedSaveButton->setObjectName(QStringLiteral("matchedDarkSaveButton"));
    m_matchedSaveButton->setToolTip(
        tr("Write the current dark frame (acquired in memory or loaded from a file) "
           "to a TIFF/CSV file so it can be reused next session."));
    m_matchedSaveButton->setEnabled(false);

    QHBoxLayout *matchedRow = new QHBoxLayout();
    matchedRow->addWidget(m_matchedDarkPath, 1);
    matchedRow->addWidget(m_matchedBrowseButton);
    matchedRow->addWidget(m_matchedAcquireButton);
    matchedRow->addWidget(m_matchedSaveButton);
    matchedForm->addRow(tr("Dark frame:"), matchedRow);

    m_matchedExposureSpin = new QDoubleSpinBox(matchedPage);
    m_matchedExposureSpin->setObjectName(QStringLiteral("matchedDarkExposureSpin"));
    m_matchedExposureSpin->setDecimals(6);
    m_matchedExposureSpin->setRange(0.0, 3600.0);
    m_matchedExposureSpin->setSingleStep(0.001);
    m_matchedExposureSpin->setSuffix(tr(" s"));
    m_matchedExposureSpin->setToolTip(
        tr("Exposure used when acquiring/loading this dark frame. It should match "
           "the signal exposure."));
    matchedForm->addRow(tr("Dark exposure:"), m_matchedExposureSpin);

    m_darkMethodStack->addWidget(matchedPage);

    // ----- Method 2: bias + scaled long dark --------------------------
    QWidget *biasPage = new QWidget(m_darkMethodStack);
    QFormLayout *biasForm = new QFormLayout(biasPage);
    biasForm->setContentsMargins(0, 0, 0, 0);

    m_biasPath = new QLineEdit(biasPage);
    m_biasPath->setObjectName(QStringLiteral("biasPathEdit"));
    m_biasPath->setReadOnly(true);
    m_biasPath->setPlaceholderText(tr("(no bias frame selected)"));
    m_biasBrowseButton = new QPushButton(tr("Browse..."), biasPage);
    m_biasBrowseButton->setObjectName(QStringLiteral("biasBrowseButton"));
    m_biasAcquireButton = new QPushButton(tr("Acquire"), biasPage);
    m_biasAcquireButton->setObjectName(QStringLiteral("biasAcquireButton"));
    m_biasSaveButton = new QPushButton(tr("Save As..."), biasPage);
    m_biasSaveButton->setObjectName(QStringLiteral("biasSaveButton"));
    m_biasSaveButton->setToolTip(
        tr("Write the current bias frame to a TIFF/CSV file so it can be reused next session."));
    m_biasSaveButton->setEnabled(false);
    QHBoxLayout *biasRow = new QHBoxLayout();
    biasRow->addWidget(m_biasPath, 1);
    biasRow->addWidget(m_biasBrowseButton);
    biasRow->addWidget(m_biasAcquireButton);
    biasRow->addWidget(m_biasSaveButton);
    biasForm->addRow(tr("Bias frame:"), biasRow);

    m_longDarkPath = new QLineEdit(biasPage);
    m_longDarkPath->setObjectName(QStringLiteral("longDarkPathEdit"));
    m_longDarkPath->setReadOnly(true);
    m_longDarkPath->setPlaceholderText(tr("(no long dark frame selected)"));
    m_longDarkBrowseButton = new QPushButton(tr("Browse..."), biasPage);
    m_longDarkBrowseButton->setObjectName(QStringLiteral("longDarkBrowseButton"));
    m_longDarkAcquireButton = new QPushButton(tr("Acquire"), biasPage);
    m_longDarkAcquireButton->setObjectName(QStringLiteral("longDarkAcquireButton"));
    m_longDarkSaveButton = new QPushButton(tr("Save As..."), biasPage);
    m_longDarkSaveButton->setObjectName(QStringLiteral("longDarkSaveButton"));
    m_longDarkSaveButton->setToolTip(
        tr("Write the current long dark frame to a TIFF/CSV file so it can be reused next session."));
    m_longDarkSaveButton->setEnabled(false);
    QHBoxLayout *longDarkRow = new QHBoxLayout();
    longDarkRow->addWidget(m_longDarkPath, 1);
    longDarkRow->addWidget(m_longDarkBrowseButton);
    longDarkRow->addWidget(m_longDarkAcquireButton);
    longDarkRow->addWidget(m_longDarkSaveButton);
    biasForm->addRow(tr("Long dark frame:"), longDarkRow);

    m_longDarkExposureSpin = new QDoubleSpinBox(biasPage);
    m_longDarkExposureSpin->setObjectName(QStringLiteral("longDarkExposureSpin"));
    m_longDarkExposureSpin->setDecimals(6);
    m_longDarkExposureSpin->setRange(0.0, 3600.0);
    m_longDarkExposureSpin->setSingleStep(0.001);
    m_longDarkExposureSpin->setSuffix(tr(" s"));
    m_longDarkExposureSpin->setToolTip(tr("Exposure of the long dark frame, t0."));
    biasForm->addRow(tr("Long dark exposure t0:"), m_longDarkExposureSpin);

    m_targetExposureSpin = new QDoubleSpinBox(biasPage);
    m_targetExposureSpin->setObjectName(QStringLiteral("targetExposureSpin"));
    m_targetExposureSpin->setDecimals(6);
    m_targetExposureSpin->setRange(0.0, 3600.0);
    m_targetExposureSpin->setSingleStep(0.001);
    m_targetExposureSpin->setSuffix(tr(" s"));
    m_targetExposureSpin->setToolTip(
        tr("Exposure of the frames being corrected, t. The dark estimate is "
           "B + (D(t0) - B) * t/t0. Enter it manually after changing the camera "
           "exposure."));
    biasForm->addRow(tr("Signal exposure t:"), m_targetExposureSpin);

    m_darkMethodStack->addWidget(biasPage);
    form->addRow(m_darkMethodStack);

    m_biasSpinBox = new QSpinBox(page);
    m_biasSpinBox->setObjectName(QStringLiteral("customBiasSpinBox"));
    m_biasSpinBox->setRange(-32768, 32767);
    m_biasSpinBox->setValue(0);
    m_biasSpinBox->setSingleStep(1);
    m_biasSpinBox->setToolTip(
        tr("Constant added after dark subtraction and flat multiplication, so the "
           "result does not clip at zero. It does not change the correction."));
    form->addRow(tr("Custom output offset:"), m_biasSpinBox);

    pageLayout->addLayout(form);
    pageLayout->addStretch(1);

    connect(m_darkMethodCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                m_darkMethodStack->setCurrentIndex(index == 0 ? 0 : 1);
            });

    wireAcquire(m_matchedAcquireButton, AcquireKind::MatchedDark);
    wireAcquire(m_biasAcquireButton, AcquireKind::Bias);
    wireAcquire(m_longDarkAcquireButton, AcquireKind::LongDark);
    wireBrowse(m_matchedBrowseButton, AcquireKind::MatchedDark);
    wireBrowse(m_biasBrowseButton, AcquireKind::Bias);
    wireBrowse(m_longDarkBrowseButton, AcquireKind::LongDark);
    wireSave(m_matchedSaveButton, Calibration::ReferenceKind::MatchedDark);
    wireSave(m_biasSaveButton, Calibration::ReferenceKind::Bias);
    wireSave(m_longDarkSaveButton, Calibration::ReferenceKind::LongDark);

    tabs->addTab(page, tr("Dark"));
}

void CalibrationDialog::setupFlatTab(QTabWidget *tabs)
{
    QWidget *page = new QWidget(tabs);
    QVBoxLayout *pageLayout = new QVBoxLayout(page);

    QFormLayout *form = new QFormLayout();

    m_flatEnableCheckBox = new QCheckBox(tr("Enable Flat-Field Calibration"), page);
    m_flatEnableCheckBox->setObjectName(QStringLiteral("flatEnableCheckBox"));
    form->addRow(m_flatEnableCheckBox);

    m_flatMethodCombo = new QComboBox(page);
    m_flatMethodCombo->setObjectName(QStringLiteral("flatMethodCombo"));
    m_flatMethodCombo->addItem(tr("Classic mean normalization"));
    m_flatMethodCombo->addItem(tr("Variable exposure (per-pixel fit)"));
    m_flatMethodCombo->setToolTip(
        tr("Classic: normalize one flat by F - D(t_flat).\n"
           "Variable exposure: fit F - D(t) over several exposures; keep the "
           "light source stable. Both reuse the Dark tab model."));
    form->addRow(tr("Flat method:"), m_flatMethodCombo);

    m_flatMethodStack = new QStackedWidget(page);
    m_flatMethodStack->setObjectName(QStringLiteral("flatMethodStack"));

    // ----- Classic -----------------------------------------------------
    QWidget *classicPage = new QWidget(m_flatMethodStack);
    QFormLayout *classicForm = new QFormLayout(classicPage);
    classicForm->setContentsMargins(0, 0, 0, 0);

    m_flatPath = new QLineEdit(classicPage);
    m_flatPath->setObjectName(QStringLiteral("flatPathEdit"));
    m_flatPath->setReadOnly(true);
    m_flatPath->setPlaceholderText(tr("(no flat frame selected)"));
    m_flatBrowseButton = new QPushButton(tr("Browse..."), classicPage);
    m_flatBrowseButton->setObjectName(QStringLiteral("flatBrowseButton"));
    m_flatAcquireButton = new QPushButton(tr("Acquire"), classicPage);
    m_flatAcquireButton->setObjectName(QStringLiteral("flatAcquireButton"));
    m_flatSaveButton = new QPushButton(tr("Save As..."), classicPage);
    m_flatSaveButton->setObjectName(QStringLiteral("flatSaveButton"));
    m_flatSaveButton->setToolTip(
        tr("Write the current flat frame to a TIFF/CSV file so it can be reused next session."));
    m_flatSaveButton->setEnabled(false);
    QHBoxLayout *flatRow = new QHBoxLayout();
    flatRow->addWidget(m_flatPath, 1);
    flatRow->addWidget(m_flatBrowseButton);
    flatRow->addWidget(m_flatAcquireButton);
    flatRow->addWidget(m_flatSaveButton);
    classicForm->addRow(tr("Flat frame:"), flatRow);

    m_flatExposureSpin = new QDoubleSpinBox(classicPage);
    m_flatExposureSpin->setObjectName(QStringLiteral("flatExposureSpin"));
    m_flatExposureSpin->setDecimals(6);
    m_flatExposureSpin->setRange(0.0, 3600.0);
    m_flatExposureSpin->setSingleStep(0.001);
    m_flatExposureSpin->setSuffix(tr(" s"));
    m_flatExposureSpin->setToolTip(
        tr("Exposure of this flat frame. It selects D(t) from the Dark tab for "
           "dark subtraction before normalization. Keep the signal well below "
           "saturation and well above the read noise."));
    classicForm->addRow(tr("Flat exposure:"), m_flatExposureSpin);

    m_flatMethodStack->addWidget(classicPage);

    // ----- Variable exposure ------------------------------------------
    QWidget *vffPage = new QWidget(m_flatMethodStack);
    QVBoxLayout *vffLayout = new QVBoxLayout(vffPage);
    vffLayout->setContentsMargins(0, 0, 0, 0);
    QFormLayout *vffForm = new QFormLayout();

    m_vffExposureSpin = new QDoubleSpinBox(vffPage);
    m_vffExposureSpin->setObjectName(QStringLiteral("vffExposureSpin"));
    m_vffExposureSpin->setDecimals(6);
    m_vffExposureSpin->setRange(0.0, 3600.0);
    m_vffExposureSpin->setSingleStep(0.001);
    m_vffExposureSpin->setSuffix(tr(" s"));
    m_vffExposureSpin->setToolTip(
        tr("Exposure to use for the next Acquire/Load. Repeat at several exposures "
           "(at least two, ideally four or five) across the usable dynamic range."));
    vffForm->addRow(tr("Exposure for next entry:"), m_vffExposureSpin);

    m_vffAcquireButton = new QPushButton(tr("Acquire"), vffPage);
    m_vffAcquireButton->setObjectName(QStringLiteral("vffAcquireButton"));
    m_vffBrowseButton = new QPushButton(tr("Load..."), vffPage);
    m_vffBrowseButton->setObjectName(QStringLiteral("vffBrowseButton"));
    QHBoxLayout *vffSourceRow = new QHBoxLayout();
    vffSourceRow->addWidget(m_vffAcquireButton);
    vffSourceRow->addWidget(m_vffBrowseButton);
    vffSourceRow->addStretch(1);
    vffForm->addRow(tr("Add exposure:"), vffSourceRow);
    vffLayout->addLayout(vffForm);

    m_vffList = new QListWidget(vffPage);
    m_vffList->setObjectName(QStringLiteral("vffList"));
    m_vffList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_vffList->setMinimumHeight(110);
    vffLayout->addWidget(m_vffList, 1);

    QHBoxLayout *vffButtons = new QHBoxLayout();
    m_vffSaveButton = new QPushButton(tr("Save Selected As..."), vffPage);
    m_vffSaveButton->setObjectName(QStringLiteral("vffSaveButton"));
    m_vffSaveButton->setToolTip(
        tr("Write the selected exposure's flat frame to a TIFF/CSV file so it can be "
           "reused next session."));
    m_vffSaveButton->setEnabled(false);
    m_vffRemoveButton = new QPushButton(tr("Remove selected"), vffPage);
    m_vffRemoveButton->setObjectName(QStringLiteral("vffRemoveButton"));
    m_vffClearButton = new QPushButton(tr("Clear all"), vffPage);
    m_vffClearButton->setObjectName(QStringLiteral("vffClearButton"));
    vffButtons->addWidget(m_vffSaveButton);
    vffButtons->addWidget(m_vffRemoveButton);
    vffButtons->addWidget(m_vffClearButton);
    vffButtons->addStretch(1);
    vffLayout->addLayout(vffButtons);

    m_flatMethodStack->addWidget(vffPage);
    form->addRow(m_flatMethodStack);

    m_flatStatusLabel = new QLabel(page);
    m_flatStatusLabel->setObjectName(QStringLiteral("flatStatusLabel"));
    m_flatStatusLabel->setWordWrap(true);
    // Only shown when flat calibration is enabled but the field could not be
    // built. The Enable checkbox already covers the disabled/ready states.
    m_flatStatusLabel->setVisible(false);
    form->addRow(m_flatStatusLabel);

    pageLayout->addLayout(form);
    pageLayout->addStretch(1);

    connect(m_flatMethodCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
                m_flatMethodStack->setCurrentIndex(index == 0 ? 0 : 1);
            });

    wireAcquire(m_flatAcquireButton, AcquireKind::Flat);
    wireAcquire(m_vffAcquireButton, AcquireKind::VffEntry);
    wireBrowse(m_flatBrowseButton, AcquireKind::Flat);
    wireBrowse(m_vffBrowseButton, AcquireKind::VffEntry);
    wireSave(m_flatSaveButton, Calibration::ReferenceKind::Flat);

    connect(m_vffSaveButton, &QPushButton::clicked, this, [this] {
        const int row = m_vffList->currentRow();
        if (row >= 0) {
            emit saveVffEntryRequested(row);
        }
    });
    connect(m_vffList, &QListWidget::currentRowChanged, this, [this](int row) {
        m_vffSaveButton->setEnabled(row >= 0);
    });

    connect(m_vffRemoveButton, &QPushButton::clicked, this, [this] {
        const int row = m_vffList->currentRow();
        if (row >= 0) {
            emit vffRemoveRequested(row);
        }
    });
    connect(m_vffClearButton, &QPushButton::clicked,
            this, &CalibrationDialog::vffClearRequested);

    tabs->addTab(page, tr("Flat"));
}

void CalibrationDialog::wireAcquire(QPushButton *button, AcquireKind kind)
{
    connect(button, &QPushButton::clicked, this, [this, kind] { beginAcquisition(kind); });
}

void CalibrationDialog::wireBrowse(QPushButton *button, AcquireKind kind)
{
    connect(button, &QPushButton::clicked, this, [this, kind] {
        m_pendingKind = kind;
        onBrowseClicked();
    });
}

void CalibrationDialog::wireSave(QPushButton *button, Calibration::ReferenceKind kind)
{
    connect(button, &QPushButton::clicked, this, [this, kind] {
        emit saveReferenceRequested(static_cast<int>(kind));
    });
}

void CalibrationDialog::beginAcquisition(AcquireKind kind)
{
    if (m_acquireInProgress || !m_acquireEnabled) {
        return;
    }
    m_pendingKind = kind;
    emit acquireRequested(frameCount());
    setAcquireInProgress(true);
}

QString CalibrationDialog::inMemoryPlaceholder(Calibration::ReferenceKind kind,
                                                int frameCount) const
{
    switch (kind) {
    case Calibration::ReferenceKind::MatchedDark:
        return tr("(in-memory dark frame, %1-frame average)").arg(frameCount);
    case Calibration::ReferenceKind::Bias:
        return tr("(in-memory bias frame, %1-frame average)").arg(frameCount);
    case Calibration::ReferenceKind::LongDark:
        return tr("(in-memory long dark frame, %1-frame average)").arg(frameCount);
    case Calibration::ReferenceKind::Flat:
        return tr("(in-memory flat frame, %1-frame average)").arg(frameCount);
    case Calibration::ReferenceKind::VffEntry:
        return tr("(in-memory flat frame, %1-frame average)").arg(frameCount);
    }
    return QString();
}

QLineEdit *CalibrationDialog::lineEditForKind(Calibration::ReferenceKind kind) const
{
    switch (kind) {
    case Calibration::ReferenceKind::MatchedDark:
        return m_matchedDarkPath;
    case Calibration::ReferenceKind::Bias:
        return m_biasPath;
    case Calibration::ReferenceKind::LongDark:
        return m_longDarkPath;
    case Calibration::ReferenceKind::Flat:
        return m_flatPath;
    case Calibration::ReferenceKind::VffEntry:
        return nullptr;
    }
    return nullptr;
}

QPushButton *CalibrationDialog::acquireButtonForKind(Calibration::ReferenceKind kind) const
{
    switch (kind) {
    case Calibration::ReferenceKind::MatchedDark:
        return m_matchedAcquireButton;
    case Calibration::ReferenceKind::Bias:
        return m_biasAcquireButton;
    case Calibration::ReferenceKind::LongDark:
        return m_longDarkAcquireButton;
    case Calibration::ReferenceKind::Flat:
        return m_flatAcquireButton;
    case Calibration::ReferenceKind::VffEntry:
        return m_vffAcquireButton;
    }
    return nullptr;
}

QPushButton *CalibrationDialog::saveButtonForKind(Calibration::ReferenceKind kind) const
{
    switch (kind) {
    case Calibration::ReferenceKind::MatchedDark:
        return m_matchedSaveButton;
    case Calibration::ReferenceKind::Bias:
        return m_biasSaveButton;
    case Calibration::ReferenceKind::LongDark:
        return m_longDarkSaveButton;
    case Calibration::ReferenceKind::Flat:
        return m_flatSaveButton;
    case Calibration::ReferenceKind::VffEntry:
        // The VFF rows live in a list and are saved through the selection.
        return nullptr;
    }
    return nullptr;
}

QDoubleSpinBox *CalibrationDialog::exposureSpinForKind(Calibration::ReferenceKind kind) const
{
    switch (kind) {
    case Calibration::ReferenceKind::MatchedDark:
        return m_matchedExposureSpin;
    case Calibration::ReferenceKind::Bias:
        // The bias frame is the zero-exposure reference; no time entry.
        return nullptr;
    case Calibration::ReferenceKind::LongDark:
        return m_longDarkExposureSpin;
    case Calibration::ReferenceKind::Flat:
        return m_flatExposureSpin;
    case Calibration::ReferenceKind::VffEntry:
        return m_vffExposureSpin;
    }
    return nullptr;
}

void CalibrationDialog::setDarkFrameEnabled(bool enabled)
{
    m_enableCheckBox->setChecked(enabled);
}

bool CalibrationDialog::darkFrameEnabled() const
{
    return m_enableCheckBox->isChecked();
}

void CalibrationDialog::setDarkFramePath(const QString &path)
{
    m_matchedDarkPath->setText(path);
}

QString CalibrationDialog::darkFramePath() const
{
    return m_matchedDarkPath->text();
}

void CalibrationDialog::setInMemoryDarkFrameUsed(bool used, int frameCount)
{
    if (used) {
        m_matchedDarkPath->clear();
        m_matchedDarkPath->setPlaceholderText(
            inMemoryPlaceholder(Calibration::ReferenceKind::MatchedDark, frameCount));
    } else if (m_matchedDarkPath->text().isEmpty()) {
        m_matchedDarkPath->setPlaceholderText(tr("(no dark frame selected)"));
    }
}

void CalibrationDialog::setCustomBias(int bias)
{
    m_biasSpinBox->setValue(bias);
}

int CalibrationDialog::customBias() const
{
    return m_biasSpinBox->value();
}

void CalibrationDialog::setDarkMethod(Calibration::DarkMethod method)
{
    const int index = (method == Calibration::DarkMethod::BiasScaled) ? 1 : 0;
    m_darkMethodCombo->setCurrentIndex(index);
    m_darkMethodStack->setCurrentIndex(index);
}

Calibration::DarkMethod CalibrationDialog::darkMethod() const
{
    return (m_darkMethodCombo->currentIndex() == 1)
        ? Calibration::DarkMethod::BiasScaled
        : Calibration::DarkMethod::MatchedDark;
}

void CalibrationDialog::setMatchedDarkExposure(double seconds)
{
    m_matchedExposureSpin->setValue(seconds);
}

double CalibrationDialog::matchedDarkExposure() const
{
    return m_matchedExposureSpin->value();
}

void CalibrationDialog::setLongDarkExposure(double seconds)
{
    m_longDarkExposureSpin->setValue(seconds);
}

double CalibrationDialog::longDarkExposure() const
{
    return m_longDarkExposureSpin->value();
}

void CalibrationDialog::setTargetExposure(double seconds)
{
    m_targetExposureSpin->setValue(seconds);
}

double CalibrationDialog::targetExposure() const
{
    return m_targetExposureSpin->value();
}

void CalibrationDialog::setFlatEnabled(bool enabled)
{
    m_flatEnableCheckBox->setChecked(enabled);
}

bool CalibrationDialog::flatEnabled() const
{
    return m_flatEnableCheckBox->isChecked();
}

void CalibrationDialog::setFlatMethod(Calibration::FlatMethod method)
{
    const int index = (method == Calibration::FlatMethod::VariableExposure) ? 1 : 0;
    m_flatMethodCombo->setCurrentIndex(index);
    m_flatMethodStack->setCurrentIndex(index);
}

Calibration::FlatMethod CalibrationDialog::flatMethod() const
{
    return (m_flatMethodCombo->currentIndex() == 1)
        ? Calibration::FlatMethod::VariableExposure
        : Calibration::FlatMethod::Classic;
}

void CalibrationDialog::setFlatExposure(double seconds)
{
    m_flatExposureSpin->setValue(seconds);
}

double CalibrationDialog::flatExposure() const
{
    return m_flatExposureSpin->value();
}

void CalibrationDialog::setVffExposure(double seconds)
{
    m_vffExposureSpin->setValue(seconds);
}

double CalibrationDialog::vffExposure() const
{
    return m_vffExposureSpin->value();
}

void CalibrationDialog::setReferenceStatus(Calibration::ReferenceKind kind, bool valid,
                                            bool inMemory, int frameCount,
                                            const QString &path)
{
    QLineEdit *edit = lineEditForKind(kind);
    if (!edit) {
        return;
    }

    // Save As only makes sense when there is a plane to write. A path typed by
    // an old caller without a loaded plane does not count.
    if (QPushButton *saveButton = saveButtonForKind(kind)) {
        saveButton->setEnabled(valid);
    }

    edit->clear();
    if (!valid) {
        if (kind == Calibration::ReferenceKind::MatchedDark) {
            edit->setPlaceholderText(tr("(no dark frame selected)"));
        } else {
            edit->setPlaceholderText(tr("(not set)"));
        }
        return;
    }

    if (inMemory) {
        edit->setPlaceholderText(inMemoryPlaceholder(kind, frameCount));
    } else if (!path.isEmpty()) {
        edit->setText(path);
        edit->setPlaceholderText(tr("(not set)"));
    } else {
        edit->setPlaceholderText(tr("(loaded reference)"));
    }
}

void CalibrationDialog::setVffEntries(const QVector<Calibration::ReferenceFrame> &entries)
{
    m_vffList->clear();
    QVector<Calibration::ReferenceFrame> sorted = entries;
    std::sort(sorted.begin(), sorted.end(), [](const Calibration::ReferenceFrame &a,
                                                const Calibration::ReferenceFrame &b) {
        return a.exposure < b.exposure;
    });
    for (const Calibration::ReferenceFrame &entry : sorted) {
        if (entry.isNull()) {
            continue;
        }
        QString text;
        if (entry.isInMemory()) {
            text = tr("%1 s  —  in-memory, %2-frame average")
                       .arg(formatSeconds(entry.exposure))
                       .arg(entry.frameCount);
        } else {
            text = tr("%1 s  —  %2")
                       .arg(formatSeconds(entry.exposure),
                            QFileInfo(entry.source).fileName());
        }
        QListWidgetItem *item = new QListWidgetItem(text, m_vffList);
        item->setToolTip(entry.source);
    }
    // Rebuilding the list drops the selection; "Save Selected As..." must not
    // keep pointing at a row that no longer exists.
    m_vffList->setCurrentRow(-1);
    m_vffSaveButton->setEnabled(false);
}

void CalibrationDialog::setFlatFieldStatus(const QString &text, bool ok)
{
    m_flatStatusLabel->setText(text);
    // The checkbox only says "enabled"; when the field could not be built the
    // reason must be visible in the dialog, not hidden in a tooltip.
    m_flatStatusLabel->setVisible(!ok && !text.isEmpty());
}

int CalibrationDialog::frameCount() const
{
    return m_frameCountSpinBox->value();
}

void CalibrationDialog::setFrameCount(int n)
{
    m_frameCountSpinBox->setValue(n);
}

double CalibrationDialog::acquisitionExposure() const
{
    const QDoubleSpinBox *spin =
        exposureSpinForKind(static_cast<Calibration::ReferenceKind>(m_pendingKind));
    return spin ? spin->value() : 0.0;
}

void CalibrationDialog::setAcquireInProgress(bool busy)
{
    m_acquireInProgress = busy;
    m_frameCountSpinBox->setEnabled(!busy);
    // Exposure and method describe the burst that is currently running. Lock
    // them so the number finally stored with the reference cannot drift while
    // frames are still arriving.
    m_darkMethodCombo->setEnabled(!busy);
    m_flatMethodCombo->setEnabled(!busy);
    for (Calibration::ReferenceKind kind : {
             Calibration::ReferenceKind::MatchedDark,
             Calibration::ReferenceKind::Bias,
             Calibration::ReferenceKind::LongDark,
             Calibration::ReferenceKind::Flat,
             Calibration::ReferenceKind::VffEntry }) {
        if (QDoubleSpinBox *spin = exposureSpinForKind(kind)) {
            spin->setEnabled(!busy);
        }
        if (QPushButton *button = acquireButtonForKind(kind)) {
            button->setEnabled(m_acquireEnabled && !busy);
        }
    }
}

void CalibrationDialog::setAcquireEnabled(bool enabled)
{
    m_acquireEnabled = enabled;
    for (Calibration::ReferenceKind kind : {
             Calibration::ReferenceKind::MatchedDark,
             Calibration::ReferenceKind::Bias,
             Calibration::ReferenceKind::LongDark,
             Calibration::ReferenceKind::Flat,
             Calibration::ReferenceKind::VffEntry }) {
        if (QPushButton *button = acquireButtonForKind(kind)) {
            button->setEnabled(enabled && !m_acquireInProgress);
        }
    }
}

void CalibrationDialog::onBrowseClicked()
{
    const Calibration::ReferenceKind kind =
        static_cast<Calibration::ReferenceKind>(m_pendingKind);

    QString title;
    switch (kind) {
    case Calibration::ReferenceKind::MatchedDark:
        title = tr("Select Dark Frame");
        break;
    case Calibration::ReferenceKind::Bias:
        title = tr("Select Bias Frame");
        break;
    case Calibration::ReferenceKind::LongDark:
        title = tr("Select Long Dark Frame");
        break;
    case Calibration::ReferenceKind::Flat:
    case Calibration::ReferenceKind::VffEntry:
        title = tr("Select Flat Frame");
        break;
    }

    QString openDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QStringList filters{
        FileLoaderWorker::openFormatsDisplayName(),
        QStringLiteral("TIFF Image (*.tiff *.tif)"),
        QStringLiteral("CSV File (*.csv)"),
        QStringLiteral("All Files (*)")
    };

    const QString path = QFileDialog::getOpenFileName(
        this, title, openDir, filters.join(QStringLiteral(";;")));
    if (!path.isEmpty()) {
        emit referenceFileSelected(static_cast<int>(kind), path);
    }
}

void CalibrationDialog::onOkClicked()
{
    onApplyClicked();
    accept();
}

void CalibrationDialog::onApplyClicked()
{
    // Apply only updates MainWindow's calibration state. The window stays open
    // so the user can continue tuning thresholds/references and compare the
    // live image without reopening the dialog.
    emit applied(darkFrameEnabled(), darkFramePath(), customBias());
}
