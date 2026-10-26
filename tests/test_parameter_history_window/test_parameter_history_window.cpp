#include <QDateTime>
#include <QFile>
#include <QSpinBox>
#include <QTemporaryDir>
#include <QTest>

#include "LineViewWidget.h"
#include "ParameterHistoryStore.h"
#include "ParameterHistoryWindow.h"

namespace {

constexpr qint64 kBaseTimeMs = 1700000000000LL;

qint64 sampleTime(int index)
{
    return kBaseTimeMs + static_cast<qint64>(index) * 1000;
}

ParameterDefinition makeTelemetryDefinition(const QString &name = QStringLiteral("sensor_temp"))
{
    ParameterDefinition def;
    def.name = name;
    def.displayName = QStringLiteral("Sensor Temperature");
    def.description = QStringLiteral("Sensor temperature");
    def.category = ParameterCategory::Cooling;
    def.type = ParameterType::FloatRange;
    def.isReadOnly = true;
    def.isDynamic = true;
    def.isExtrinsic = true;
    return def;
}

void appendSamples(ParameterHistoryStore &store, int count, double first = 0.0)
{
    for (int i = 0; i < count; ++i) {
        store.append(QStringLiteral("sensor_temp"), sampleTime(i), first + i);
    }
}

} // namespace

class TestParameterHistoryWindow : public QObject
{
    Q_OBJECT

private slots:
    void test_plot_follows_stored_samples();
    void test_capacity_spin_trims_history();
    void test_export_csv_contains_window_samples();
    void test_export_without_samples_fails();
};

void TestParameterHistoryWindow::test_plot_follows_stored_samples()
{
    ParameterHistoryStore store;
    store.setCapacity(100);
    appendSamples(store, 5, -20.0);

    ParameterHistoryWindow window(QStringLiteral("sensor_temp"),
                                  makeTelemetryDefinition(), &store);

    LineViewWidget *plot = window.plotWidget();
    QVERIFY(plot);
    QCOMPARE(plot->xData(), (QVector<double>{ 0.0, 1.0, 2.0, 3.0, 4.0 }));
    QCOMPARE(plot->yData(), (QVector<double>{ -20.0, -19.0, -18.0, -17.0, -16.0 }));

    // 新样本到达后 refresh() 把它画进去，横轴仍是相对本次会话的秒数
    store.append(QStringLiteral("sensor_temp"), sampleTime(5), -15.0);
    window.refresh();

    QCOMPARE(plot->yData().size(), 6);
    QCOMPARE(plot->yData().last(), -15.0);
    QCOMPARE(plot->xData().last(), 5.0);
}

void TestParameterHistoryWindow::test_capacity_spin_trims_history()
{
    ParameterHistoryStore store;
    store.setCapacity(100);
    appendSamples(store, 20);

    ParameterHistoryWindow window(QStringLiteral("sensor_temp"),
                                  makeTelemetryDefinition(), &store);

    auto *points = window.findChild<QSpinBox *>(QStringLiteral("pointsSpinBox"));
    QVERIFY(points);
    QCOMPARE(points->value(), 100);

    // 容量是全局共享的：从窗口里改，store 立刻丢掉最老的样本
    points->setValue(10);

    QCOMPARE(store.capacity(), 10);
    QCOMPARE(store.count(QStringLiteral("sensor_temp")), 10);
    QCOMPARE(window.plotWidget()->xData().size(), 10);
    QCOMPARE(window.plotWidget()->xData().first(), 10.0);
    QCOMPARE(window.plotWidget()->yData().last(), 19.0);
}

void TestParameterHistoryWindow::test_export_csv_contains_window_samples()
{
    ParameterHistoryStore store;
    store.setCapacity(100);
    store.append(QStringLiteral("sensor_temp"), sampleTime(0), -19.75);
    store.append(QStringLiteral("sensor_temp"), sampleTime(1), -19.5);
    // 1 Hz 轮询定时器会落在 999 / 1001 ms 上，导出时 elapsed_s 必须收敛到 0.1 s，
    // 不能把毫秒级的定时器抖动写成 656.001 / 656.999
    store.append(QStringLiteral("sensor_temp"), sampleTime(1) + 999, -19.25);

    ParameterHistoryWindow window(QStringLiteral("sensor_temp"),
                                  makeTelemetryDefinition(), &store);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString path = dir.filePath(QStringLiteral("history.csv"));

    QString error;
    QVERIFY2(window.exportCsv(path, &error), qPrintable(error));

    QFile file(path);
    QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
    const QString content = QString::fromUtf8(file.readAll());
    const QStringList lines = content.split(QLatin1Char('\n'), Qt::SkipEmptyParts);

    QCOMPARE(lines.size(), 4);   // 表头 + 3 行
    QCOMPARE(lines.at(0), QStringLiteral("elapsed_s,timestamp,value"));

    const QStringList first = lines.at(1).split(QLatin1Char(','));
    QCOMPARE(first.size(), 3);
    QCOMPARE(first.at(0), QStringLiteral("0.0"));
    QCOMPARE(first.at(2).toDouble(), -19.75);

    const QStringList second = lines.at(2).split(QLatin1Char(','));
    QCOMPARE(second.at(0), QStringLiteral("1.0"));
    QCOMPARE(second.at(2).toDouble(), -19.5);

    const QStringList jittered = lines.at(3).split(QLatin1Char(','));
    QCOMPARE(jittered.at(0), QStringLiteral("2.0"));   // 1.999 s → 2.0
    QCOMPARE(jittered.at(2).toDouble(), -19.25);

    // 时间戳列是带时区的 ISO 8601，能被 Excel/脚本直接解析
    QVERIFY(QDateTime::fromString(first.at(1), Qt::ISODateWithMs).isValid());
}

void TestParameterHistoryWindow::test_export_without_samples_fails()
{
    ParameterHistoryStore store;
    ParameterHistoryWindow window(QStringLiteral("sensor_temp"),
                                  makeTelemetryDefinition(), &store);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    QString error;
    QVERIFY(!window.exportCsv(dir.filePath(QStringLiteral("empty.csv")), &error));
    QVERIFY(!error.isEmpty());
}

QTEST_MAIN(TestParameterHistoryWindow)
#include "test_parameter_history_window.moc"
