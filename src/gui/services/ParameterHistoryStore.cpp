#include "ParameterHistoryStore.h"

ParameterHistoryStore::ParameterHistoryStore(QObject *parent)
    : QObject(parent)
{
}

void ParameterHistoryStore::setCapacity(int points)
{
    points = qBound(kMinCapacity, points, kMaxCapacity);
    if (points == m_capacity) {
        return;
    }

    m_capacity = points;

    for (auto it = m_buffers.begin(); it != m_buffers.end(); ++it) {
        trim(it.value());
    }

    emit capacityChanged(m_capacity);
}

void ParameterHistoryStore::clear()
{
    m_buffers.clear();
    m_origins.clear();
    emit historyCleared(QString());
}

void ParameterHistoryStore::clear(const QString &name)
{
    const bool hadBuffer = m_buffers.remove(name);
    const bool hadOrigin = m_origins.remove(name);
    if (!hadBuffer && !hadOrigin) {
        return;
    }

    emit historyCleared(name);
}

void ParameterHistoryStore::append(const QString &name, qint64 timestampMs, double value)
{
    QQueue<ParameterSample> &queue = m_buffers[name];

    // 系统时钟回拨时不能让时间轴倒退：横轴要按升序排，光标查找依赖这个约定
    if (!queue.isEmpty() && timestampMs <= queue.last().timestampMs) {
        timestampMs = queue.last().timestampMs + 1;
    }

    if (!m_origins.contains(name)) {
        m_origins.insert(name, timestampMs);
    }

    ParameterSample sample;
    sample.timestampMs = timestampMs;
    sample.value = value;
    queue.enqueue(sample);

    trim(queue);
}

bool ParameterHistoryStore::isEmpty(const QString &name) const
{
    return m_buffers.value(name).isEmpty();
}

int ParameterHistoryStore::count(const QString &name) const
{
    return m_buffers.value(name).size();
}

QVector<ParameterSample> ParameterHistoryStore::samples(const QString &name) const
{
    return m_buffers.value(name);
}

qint64 ParameterHistoryStore::originTimestamp(const QString &name) const
{
    return m_origins.value(name, 0);
}

bool ParameterHistoryStore::isTelemetryParameter(const ParameterDefinition &def)
{
    return def.isReadOnly && def.isDynamic && def.isExtrinsic;
}

bool ParameterHistoryStore::isPlottableTelemetry(const ParameterDefinition &def)
{
    if (!isTelemetryParameter(def)) {
        return false;
    }

    switch (def.type) {
        case ParameterType::FloatRange:
        case ParameterType::IntRange:
        case ParameterType::FloatCollection:
        case ParameterType::IntCollection:
            return true;
        default:
            return false;
    }
}

void ParameterHistoryStore::trim(QQueue<ParameterSample> &queue) const
{
    while (queue.size() > m_capacity) {
        queue.dequeue();
    }
}
