#ifndef PARAMETERHISTORYSTORE_H
#define PARAMETERHISTORYSTORE_H

#include <QHash>
#include <QObject>
#include <QQueue>
#include <QString>
#include <QVector>

#include "CameraTypes.h"

//! 一次遥测采样：墙上时钟时间戳 + 数值
struct ParameterSample
{
    qint64 timestampMs = 0;
    double value = 0.0;
};

//! 只读/动态/外因参数的历史值。每个参数一条队列，所有参数共用同一个
//! 固定长度的滑动窗口。
//!
//! CameraTab 每个轮询 tick 追加一次。窗口长度可以随时从外部改：缩小时立刻
//! 丢掉最老的样本，放大只影响以后的样本（已经丢掉的补不回来）。
class ParameterHistoryStore : public QObject
{
    Q_OBJECT
public:
    //! 1 Hz 轮询下约 10 分钟
    static constexpr int kDefaultCapacity = 600;
    static constexpr int kMinCapacity = 10;
    static constexpr int kMaxCapacity = 100000;

    explicit ParameterHistoryStore(QObject *parent = nullptr);

    int capacity() const { return m_capacity; }
    //! 所有参数共用的窗口长度；超出 [kMinCapacity, kMaxCapacity] 会被夹住
    void setCapacity(int points);

    void clear();
    void clear(const QString &name);

    void append(const QString &name, qint64 timestampMs, double value);

    bool isEmpty(const QString &name) const;
    int count(const QString &name) const;
    //! 按时间从老到新
    QVector<ParameterSample> samples(const QString &name) const;
    //! 本参数当前这批样本的时间原点（第一个样本的时间戳），没有数据时为 0。
    //! 窗口滚掉老样本后原点不变，曲线横轴才不会跟着一起跳
    qint64 originTimestamp(const QString &name) const;

    //! 只读 + 动态 + 外因：CameraTab 每秒轮询的那批遥测
    static bool isTelemetryParameter(const ParameterDefinition &def);
    //! 遥测里能画成曲线的数值型；字符串/布尔状态（如 sensor_cooler_status）画不了
    static bool isPlottableTelemetry(const ParameterDefinition &def);

signals:
    void capacityChanged(int points);
    //! name 为空表示全部清空
    void historyCleared(const QString &name);

private:
    void trim(QQueue<ParameterSample> &queue) const;

    QHash<QString, QQueue<ParameterSample>> m_buffers;
    QHash<QString, qint64> m_origins;
    int m_capacity = kDefaultCapacity;
};

#endif // PARAMETERHISTORYSTORE_H
