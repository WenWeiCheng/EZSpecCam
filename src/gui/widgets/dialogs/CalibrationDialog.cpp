#include "CalibrationDialog.h"

#include <QFileDialog>
#include <QStandardPaths>

#include "../../../gui/workers/FileLoaderWorker.h"

namespace {
// 帧数上限。placeholder 的宽度按这个上限预留，所以两处必须用同一个数。
// 注意 MainWindow::onAcquireDarkFrameStartRequested 还会把请求值夹到 1000，
// 对话框这里仍按自己的上限显示。
constexpr int kMaxFrameCount = 10000;

// 留给 QLineEdit 边框和左右内边距的宽度。QLineEdit 放不下 placeholder 时会直接
// 截成省略号，而它自己的 sizeHint 不看这段文字，所以宽度得自己算。
// test_calibration_dialog 里有一份同样的数，改这里要同步改那边。
constexpr int kPlaceholderSlack = 16;
} // namespace

CalibrationDialog::CalibrationDialog(QWidget *parent)
    : QDialog(parent)
    , m_enableCheckBox(nullptr)
    , m_pathLineEdit(nullptr)
    , m_browseButton(nullptr)
    , m_biasSpinBox(nullptr)
    , m_frameCountSpinBox(nullptr)
    , m_acquireButton(nullptr)
    , m_okButton(nullptr)
    , m_cancelButton(nullptr)
    , m_acquireInProgress(false)
    , m_acquireEnabled(true)
{
    setWindowTitle("Calibration");
    // 宽度不写死：下面会按当前字体把路径框撑到刚好放得下最长的那条提示，
    // 由 QFormLayout 顺势把窗口顶开，换字体也不会截断
    setMinimumWidth(360);
    // 采暗帧时主窗口还得能用（按 Stop、看实时画面），所以不能挡住主窗口
    setModal(false);

    QFormLayout *formLayout = new QFormLayout(this);

    m_enableCheckBox = new QCheckBox(tr("Enable Dark-Frame Calibration"), this);
    formLayout->addRow(m_enableCheckBox);

    m_pathLineEdit = new QLineEdit(this);
    m_pathLineEdit->setReadOnly(true);
    m_pathLineEdit->setPlaceholderText(tr("(no dark frame selected)"));
    // QLineEdit 的 sizeHint 不看 placeholder 文字，窗口开到多宽都可能把提示
    // 截成「(in-memory dark frame, …」。而这一行恰恰是采完暗帧之后用户第一眼
    // 要读的，所以按当前字体把最长的一条量出来当最小宽度 —— 写死像素换个字体
    // 就对不上了。留给 QFormLayout 撑开对话框，这里不再写死窗口宽度。
    const QString widestPlaceholder = tr("(in-memory dark frame, %1-frame average)")
                                          .arg(kMaxFrameCount);
    m_pathLineEdit->setMinimumWidth(m_pathLineEdit->fontMetrics().horizontalAdvance(
                                        widestPlaceholder)
                                    + kPlaceholderSlack);
    m_browseButton = new QPushButton(tr("Browse..."), this);
    m_browseButton->setObjectName("browseButton");

    QHBoxLayout *pathRow = new QHBoxLayout();
    pathRow->addWidget(m_pathLineEdit, 1);
    pathRow->addWidget(m_browseButton);
    formLayout->addRow(tr("Dark Frame:"), pathRow);

    // 采集和上面那行是同一件事的两头：这里采完，上面的框当场变成
    // 「in-memory dark frame」，接着勾上面的 Enable 就能直接用，
    // 不用来回切窗口。
    m_frameCountSpinBox = new QSpinBox(this);
    m_frameCountSpinBox->setMinimum(1);
    m_frameCountSpinBox->setMaximum(kMaxFrameCount);
    m_frameCountSpinBox->setValue(10);
    m_frameCountSpinBox->setSingleStep(1);
    m_frameCountSpinBox->setObjectName("frameCountSpinBox");
    m_acquireButton = new QPushButton(tr("Acquire"), this);
    m_acquireButton->setObjectName("acquireButton");

    // 和上面路径行同一种结构：输入框吃掉多余宽度，按钮保持自身宽度。
    // 这样帧数框和路径框一样长，Acquire 也和 Browse 对齐。
    QHBoxLayout *acquireRow = new QHBoxLayout();
    acquireRow->addWidget(m_frameCountSpinBox, 1);
    acquireRow->addWidget(m_acquireButton);
    formLayout->addRow(tr("Frames to average:"), acquireRow);

    // 这一行右边没有按钮，输入框独占整列，不需要和上面两行等长
    m_biasSpinBox = new QSpinBox(this);
    m_biasSpinBox->setRange(-32768, 32767);
    m_biasSpinBox->setValue(0);
    m_biasSpinBox->setSingleStep(1);
    formLayout->addRow(tr("Custom Bias:"), m_biasSpinBox);

    QHBoxLayout *buttonLayout = new QHBoxLayout();
    m_okButton = new QPushButton(tr("OK"), this);
    m_okButton->setObjectName("okButton");
    m_cancelButton = new QPushButton(tr("Cancel"), this);
    buttonLayout->addStretch(1);
    buttonLayout->addWidget(m_okButton);
    buttonLayout->addWidget(m_cancelButton);
    formLayout->addRow(buttonLayout);

    connect(m_browseButton, &QPushButton::clicked,
            this, &CalibrationDialog::onBrowseClicked);
    connect(m_okButton, &QPushButton::clicked,
            this, &CalibrationDialog::onOkClicked);
    connect(m_cancelButton, &QPushButton::clicked,
            this, &QDialog::reject);
    connect(m_acquireButton, &QPushButton::clicked,
            this, &CalibrationDialog::onAcquireClicked);
}

CalibrationDialog::~CalibrationDialog()
{
}

void CalibrationDialog::setDarkFrameEnabled(bool enabled)
{
    if (m_enableCheckBox) {
        m_enableCheckBox->setChecked(enabled);
    }
}

bool CalibrationDialog::darkFrameEnabled() const
{
    return m_enableCheckBox ? m_enableCheckBox->isChecked() : false;
}

void CalibrationDialog::setDarkFramePath(const QString &path)
{
    if (m_pathLineEdit) {
        m_pathLineEdit->setText(path);
    }
}

void CalibrationDialog::setInMemoryDarkFrameUsed(bool used, int frameCount)
{
    if (!m_pathLineEdit) {
        return;
    }
    m_pathLineEdit->setPlaceholderText(used
        ? tr("(in-memory dark frame, %1-frame average)").arg(frameCount)
        : tr("(no dark frame selected)"));
}

QString CalibrationDialog::darkFramePath() const
{
    return m_pathLineEdit ? m_pathLineEdit->text() : QString();
}

void CalibrationDialog::setCustomBias(int bias)
{
    if (m_biasSpinBox) {
        m_biasSpinBox->setValue(bias);
    }
}

int CalibrationDialog::customBias() const
{
    return m_biasSpinBox ? m_biasSpinBox->value() : 0;
}

int CalibrationDialog::frameCount() const
{
    return m_frameCountSpinBox ? m_frameCountSpinBox->value() : 10;
}

void CalibrationDialog::setFrameCount(int n)
{
    if (m_frameCountSpinBox) {
        m_frameCountSpinBox->setValue(n);
    }
}

void CalibrationDialog::setAcquireInProgress(bool busy)
{
    m_acquireInProgress = busy;
    if (m_frameCountSpinBox) {
        m_frameCountSpinBox->setEnabled(!busy);
    }
    if (m_acquireButton) {
        m_acquireButton->setEnabled(m_acquireEnabled && !busy);
    }
}

void CalibrationDialog::setAcquireEnabled(bool enabled)
{
    m_acquireEnabled = enabled;
    if (m_acquireButton) {
        m_acquireButton->setEnabled(enabled && !m_acquireInProgress);
    }
}

void CalibrationDialog::onBrowseClicked()
{
    QString openDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);

    QStringList filters{
        FileLoaderWorker::openFormatsDisplayName(),
        QStringLiteral("TIFF Image (*.tiff *.tif)"),
        QStringLiteral("CSV File (*.csv)"),
        QStringLiteral("All Files (*)")
    };

    QString path = QFileDialog::getOpenFileName(this,
        tr("Select Dark Frame"), openDir, filters.join(QStringLiteral(";;;")));

    if (!path.isEmpty()) {
        m_pathLineEdit->setText(path);
    }
}

void CalibrationDialog::onOkClicked()
{
    emit applied(darkFrameEnabled(), darkFramePath(), customBias());
    accept();
}

void CalibrationDialog::onAcquireClicked()
{
    if (m_acquireInProgress || !m_acquireEnabled) {
        return;
    }
    emit acquireRequested(frameCount());
    setAcquireInProgress(true);
}
