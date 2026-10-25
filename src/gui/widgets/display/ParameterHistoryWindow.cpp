#include "ParameterHistoryWindow.h"

#include "LineViewWidget.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QSpinBox>
#include <QStandardPaths>
#include <QTextStream>
#include <QVBoxLayout>

namespace {

constexpr int kMinimumWidth = 640;
constexpr int kMinimumHeight = 400;
//! 导出时记住上次的目录，和标定 Save As 用同一套做法
const char *kLastExportDirKey = "parameterHistory/lastExportDirectory";

QString parameterLabel(const QString &parameterName, const ParameterDefinition &definition)
{
    return definition.displayName.isEmpty() ? parameterName : definition.displayName;
}

} // namespace

ParameterHistoryWindow::ParameterHistoryWindow(const QString &parameterName,
                                               const ParameterDefinition &definition,
                                               ParameterHistoryStore *store,
                                               QWidget *parent)
    : QDialog(parent)
    , m_parameterName(parameterName)
    , m_definition(definition)
    , m_store(store)
{
    setWindowTitle(tr("%1 History").arg(parameterLabel(parameterName, definition)));
    setMinimumSize(kMinimumWidth, kMinimumHeight);
    setModal(false);

    setupUi();

    connect(m_store, &ParameterHistoryStore::capacityChanged,
            this, &ParameterHistoryWindow::syncCapacity);
    connect(m_store, &ParameterHistoryStore::historyCleared,
            this, [this](const QString &name) {
                if (name.isEmpty() || name == m_parameterName) {
                    refresh();
                }
            });

    refresh();
}

ParameterHistoryWindow::~ParameterHistoryWindow() = default;

void ParameterHistoryWindow::setupUi()
{
    m_plot = new LineViewWidget(this);
    m_plot->setXAxisLabel(tr("Time (s)"));
    m_plot->setYAxisLabel(parameterLabel(m_parameterName, m_definition));
    // 温度/电压这类值 0 位小数会把 -19.7 读成 -20
    m_plot->setCursorDecimals(1, 2);

    m_pointsSpin = new QSpinBox(this);
    m_pointsSpin->setObjectName("pointsSpinBox");
    m_pointsSpin->setRange(ParameterHistoryStore::kMinCapacity,
                           ParameterHistoryStore::kMaxCapacity);
    m_pointsSpin->setValue(m_store->capacity());
    m_pointsSpin->setToolTip(tr("Number of samples kept in the history. The window length\n"
                                "is shared by every telemetry parameter. Shrinking drops the\n"
                                "oldest samples immediately; growing only affects new samples."));

    m_statusLabel = new QLabel(this);
    m_statusLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    m_statusLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_clearButton = new QPushButton(tr("Clear"), this);
    m_clearButton->setObjectName("clearButton");
    m_clearButton->setToolTip(tr("Forget the samples recorded for this parameter"));

    m_exportButton = new QPushButton(tr("Export CSV..."), this);
    m_exportButton->setObjectName("exportButton");
    m_exportButton->setToolTip(tr("Write the samples currently kept in the window to a CSV file"));

    QHBoxLayout *controls = new QHBoxLayout();
    controls->addWidget(new QLabel(tr("Points:"), this));
    controls->addWidget(m_pointsSpin);
    controls->addSpacing(12);
    controls->addWidget(m_statusLabel, 1);
    controls->addWidget(m_clearButton);
    controls->addWidget(m_exportButton);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(m_plot, 1);
    layout->addLayout(controls);

    connect(m_pointsSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            m_store, &ParameterHistoryStore::setCapacity);
    connect(m_clearButton, &QPushButton::clicked,
            this, &ParameterHistoryWindow::onClearClicked);
    connect(m_exportButton, &QPushButton::clicked,
            this, &ParameterHistoryWindow::onExportClicked);
}

void ParameterHistoryWindow::refresh()
{
    const QVector<ParameterSample> samples = m_store->samples(m_parameterName);
    if (samples.isEmpty()) {
        m_plot->clearData();
        updateStatus(samples);
        return;
    }

    const qint64 origin = m_store->originTimestamp(m_parameterName);
    QVector<double> x;
    QVector<double> y;
    x.reserve(samples.size());
    y.reserve(samples.size());

    for (const ParameterSample &sample : samples) {
        x.append((sample.timestampMs - origin) / 1000.0);
        y.append(sample.value);
    }

    m_plot->setData(x, y);
    updateStatus(samples);
}

void ParameterHistoryWindow::syncCapacity(int points)
{
    if (m_pointsSpin->value() != points) {
        m_pointsSpin->blockSignals(true);
        m_pointsSpin->setValue(points);
        m_pointsSpin->blockSignals(false);
    }

    refresh();
}

void ParameterHistoryWindow::updateStatus(const QVector<ParameterSample> &samples)
{
    if (samples.isEmpty()) {
        m_statusLabel->setText(tr("No samples yet"));
        m_exportButton->setEnabled(false);
        m_clearButton->setEnabled(false);
        return;
    }

    m_statusLabel->setText(tr("%1 pts | last: %2")
                               .arg(samples.size())
                               .arg(QString::number(samples.last().value, 'g', 6)));
    m_exportButton->setEnabled(true);
    m_clearButton->setEnabled(true);
}

void ParameterHistoryWindow::onClearClicked()
{
    m_store->clear(m_parameterName);
}

void ParameterHistoryWindow::onExportClicked()
{
    if (m_store->isEmpty(m_parameterName)) {
        return;
    }

    QSettings settings;
    QString directory = settings.value(QLatin1String(kLastExportDirKey)).toString();
    if (directory.isEmpty() || !QDir(directory).exists()) {
        directory = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    }

    const QString baseName = QStringLiteral("%1_history_%2.csv")
                                 .arg(m_parameterName,
                                      QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString path = QFileDialog::getSaveFileName(
        this,
        tr("Export %1 History").arg(parameterLabel(m_parameterName, m_definition)),
        QDir(directory).filePath(baseName),
        tr("CSV File (*.csv)"));
    if (path.isEmpty()) {
        return;
    }

    QString error;
    if (!exportCsv(path, &error)) {
        QMessageBox::warning(this, tr("Export Failed"), error);
        return;
    }

    settings.setValue(QLatin1String(kLastExportDirKey), QFileInfo(path).absolutePath());
    m_statusLabel->setText(tr("Exported %1 pts to %2")
                               .arg(m_store->count(m_parameterName))
                               .arg(QFileInfo(path).fileName()));
}

bool ParameterHistoryWindow::exportCsv(const QString &filePath, QString *error) const
{
    const QVector<ParameterSample> samples = m_store->samples(m_parameterName);
    if (samples.isEmpty()) {
        if (error) {
            *error = tr("There are no samples to export.");
        }
        return false;
    }

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (error) {
            *error = tr("Cannot open %1 for writing: %2").arg(filePath, file.errorString());
        }
        return false;
    }

    QTextStream stream(&file);
    stream << "elapsed_s,timestamp,value\n";

    const qint64 origin = m_store->originTimestamp(m_parameterName);
    for (const ParameterSample &sample : samples) {
        stream << QString::number((sample.timestampMs - origin) / 1000.0, 'f', 3)
               << ','
               << QDateTime::fromMSecsSinceEpoch(sample.timestampMs).toString(Qt::ISODateWithMs)
               << ','
               << QString::number(sample.value, 'g', 12)
               << '\n';
    }

    stream.flush();
    if (file.error() != QFileDevice::NoError) {
        if (error) {
            *error = tr("Failed to write %1: %2").arg(filePath, file.errorString());
        }
        return false;
    }

    return true;
}
