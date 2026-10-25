#include <QObject>
#include <QSignalSpy>
#include <QTest>

#include "ParameterHistoryStore.h"

namespace {

constexpr qint64 kBaseTimeMs = 1700000000000LL;

qint64 sampleTime(int index)
{
    return kBaseTimeMs + static_cast<qint64>(index) * 1000;
}

} // namespace

class TestParameterHistory : public QObject
{
    Q_OBJECT

private slots:
    void test_append_keeps_order_and_origin();
    void test_capacity_keeps_only_newest_samples();
    void test_shrinking_capacity_trims_immediately();
    void test_growing_capacity_keeps_existing_samples();
    void test_clear_resets_one_parameter();
    void test_clear_all_resets_everything();
    void test_timestamps_stay_monotonic();
    void test_capacity_is_clamped();
    void test_plottable_requires_numeric_telemetry();
};

void TestParameterHistory::test_append_keeps_order_and_origin()
{
    ParameterHistoryStore store;
    QCOMPARE(store.capacity(), static_cast<int>(ParameterHistoryStore::kDefaultCapacity));
    QVERIFY(store.isEmpty(QStringLiteral("sensor_temp")));
    QCOMPARE(store.originTimestamp(QStringLiteral("sensor_temp")), 0LL);

    for (int i = 0; i < 3; ++i) {
        store.append(QStringLiteral("sensor_temp"), sampleTime(i), i * 1.5);
    }

    QCOMPARE(store.count(QStringLiteral("sensor_temp")), 3);
    const QVector<ParameterSample> samples = store.samples(QStringLiteral("sensor_temp"));
    QCOMPARE(samples.size(), 3);
    QCOMPARE(samples.first().timestampMs, sampleTime(0));
    QCOMPARE(samples.last().timestampMs, sampleTime(2));
    QCOMPARE(samples.last().value, 3.0);
    QCOMPARE(store.originTimestamp(QStringLiteral("sensor_temp")), sampleTime(0));
}

void TestParameterHistory::test_capacity_keeps_only_newest_samples()
{
    ParameterHistoryStore store;
    store.setCapacity(10);

    for (int i = 0; i < 15; ++i) {
        store.append(QStringLiteral("sensor_temp"), sampleTime(i), i);
    }

    QCOMPARE(store.count(QStringLiteral("sensor_temp")), 10);
    const QVector<ParameterSample> samples = store.samples(QStringLiteral("sensor_temp"));
    QCOMPARE(samples.first().value, 5.0);
    QCOMPARE(samples.last().value, 14.0);

    // 原点仍是整批数据的第一个样本：窗口滚动不会让横轴跟着跳
    QCOMPARE(store.originTimestamp(QStringLiteral("sensor_temp")), sampleTime(0));
}

void TestParameterHistory::test_shrinking_capacity_trims_immediately()
{
    ParameterHistoryStore store;
    store.setCapacity(100);
    for (int i = 0; i < 20; ++i) {
        store.append(QStringLiteral("sensor_temp"), sampleTime(i), i);
    }

    QSignalSpy capacitySpy(&store, &ParameterHistoryStore::capacityChanged);
    store.setCapacity(10);

    QCOMPARE(capacitySpy.count(), 1);
    QCOMPARE(capacitySpy.at(0).at(0).toInt(), 10);
    QCOMPARE(store.count(QStringLiteral("sensor_temp")), 10);

    const QVector<ParameterSample> samples = store.samples(QStringLiteral("sensor_temp"));
    QCOMPARE(samples.first().value, 10.0);
    QCOMPARE(samples.last().value, 19.0);
}

void TestParameterHistory::test_growing_capacity_keeps_existing_samples()
{
    ParameterHistoryStore store;
    store.setCapacity(10);
    for (int i = 0; i < 10; ++i) {
        store.append(QStringLiteral("sensor_temp"), sampleTime(i), i);
    }

    store.setCapacity(50);
    QCOMPARE(store.count(QStringLiteral("sensor_temp")), 10);

    store.append(QStringLiteral("sensor_temp"), sampleTime(10), 10.0);
    QCOMPARE(store.count(QStringLiteral("sensor_temp")), 11);
}

void TestParameterHistory::test_clear_resets_one_parameter()
{
    ParameterHistoryStore store;
    store.append(QStringLiteral("a"), sampleTime(0), 1.0);
    store.append(QStringLiteral("b"), sampleTime(0), 2.0);

    QSignalSpy clearSpy(&store, &ParameterHistoryStore::historyCleared);
    store.clear(QStringLiteral("a"));

    QCOMPARE(clearSpy.count(), 1);
    QCOMPARE(clearSpy.at(0).at(0).toString(), QStringLiteral("a"));
    QVERIFY(store.isEmpty(QStringLiteral("a")));
    QCOMPARE(store.originTimestamp(QStringLiteral("a")), 0LL);
    QCOMPARE(store.count(QStringLiteral("b")), 1);
}

void TestParameterHistory::test_clear_all_resets_everything()
{
    ParameterHistoryStore store;
    store.append(QStringLiteral("a"), sampleTime(0), 1.0);
    store.append(QStringLiteral("b"), sampleTime(0), 2.0);

    store.clear();

    QVERIFY(store.isEmpty(QStringLiteral("a")));
    QVERIFY(store.isEmpty(QStringLiteral("b")));
    QCOMPARE(store.originTimestamp(QStringLiteral("a")), 0LL);
    QCOMPARE(store.originTimestamp(QStringLiteral("b")), 0LL);
}

void TestParameterHistory::test_timestamps_stay_monotonic()
{
    ParameterHistoryStore store;

    // 系统时钟回拨：新样本的时间戳不能比上一个还早，否则横轴不再升序
    store.append(QStringLiteral("sensor_temp"), 5000, 1.0);
    store.append(QStringLiteral("sensor_temp"), 1000, 2.0);

    const QVector<ParameterSample> samples = store.samples(QStringLiteral("sensor_temp"));
    QCOMPARE(samples.size(), 2);
    QCOMPARE(samples.last().timestampMs, samples.first().timestampMs + 1);
}

void TestParameterHistory::test_capacity_is_clamped()
{
    ParameterHistoryStore store;

    store.setCapacity(1);
    QCOMPARE(store.capacity(), static_cast<int>(ParameterHistoryStore::kMinCapacity));

    store.setCapacity(1000000);
    QCOMPARE(store.capacity(), static_cast<int>(ParameterHistoryStore::kMaxCapacity));
}

void TestParameterHistory::test_plottable_requires_numeric_telemetry()
{
    ParameterDefinition temperature;
    temperature.name = QStringLiteral("sensor_temp");
    temperature.displayName = QStringLiteral("Sensor Temperature");
    temperature.description = QStringLiteral("Sensor temperature");
    temperature.category = ParameterCategory::Cooling;
    temperature.type = ParameterType::FloatRange;
    temperature.isReadOnly = true;
    temperature.isDynamic = true;
    temperature.isExtrinsic = true;

    QVERIFY(ParameterHistoryStore::isTelemetryParameter(temperature));
    QVERIFY(ParameterHistoryStore::isPlottableTelemetry(temperature));

    ParameterDefinition status = temperature;
    status.name = QStringLiteral("sensor_cooler_status");
    status.type = ParameterType::StringCollection;
    QVERIFY(ParameterHistoryStore::isTelemetryParameter(status));
    QVERIFY(!ParameterHistoryStore::isPlottableTelemetry(status));

    ParameterDefinition writable = temperature;
    writable.isReadOnly = false;
    QVERIFY(!ParameterHistoryStore::isTelemetryParameter(writable));
    QVERIFY(!ParameterHistoryStore::isPlottableTelemetry(writable));
}

QTEST_MAIN(TestParameterHistory)
#include "test_parameter_history.moc"
