#include "SequenceRunner.h"

#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

bool SequenceRunner::loadFromFile(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
    {
        m_error = QString("Cannot open sequence file: %1").arg(filePath);
        return false;
    }

    QJsonParseError parseError;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError)
    {
        m_error = QString("JSON parse error: %1").arg(parseError.errorString());
        return false;
    }

    QJsonObject root = doc.object();

    if (root.contains("settings"))
    {
        QJsonObject s = root["settings"].toObject();
        if (s.contains("output")) defaultOutputDir = s["output"].toString();
        if (s.contains("format")) defaultFormat = s["format"].toString();
        if (s.contains("prefix")) defaultPrefix = s["prefix"].toString();
        if (s.contains("suffix")) defaultSuffix = s["suffix"].toString();
        // save_metadata 字段已废弃，被忽略（metadata 总是随帧一起保存）
    }

    QJsonArray stepsArray = root["steps"].toArray();
    for (const QJsonValue &stepVal : stepsArray)
    {
        QJsonObject stepObj = stepVal.toObject();
        SequenceStep step;
        step.outputDir = defaultOutputDir;
        step.format = defaultFormat;
        step.prefix = defaultPrefix;
        step.suffix = defaultSuffix;

        if (stepObj.contains("configure"))
        {
            step.type = SequenceStep::Configure;
            QJsonObject cfg = stepObj["configure"].toObject();
            for (auto it = cfg.begin(); it != cfg.end(); ++it)
                step.parameters[it.key()] = it.value().toVariant();
        }
        else if (stepObj.contains("capture"))
        {
            step.type = SequenceStep::Capture;
            QJsonObject cap = stepObj["capture"].toObject();
            if (cap.contains("frames")) step.frames = cap["frames"].toInt(1);
            if (cap.contains("output")) step.outputDir = cap["output"].toString();
            if (cap.contains("format")) step.format = cap["format"].toString();
            if (cap.contains("prefix")) step.prefix = cap["prefix"].toString();
            if (cap.contains("suffix")) step.suffix = cap["suffix"].toString();
            // save_metadata 字段已废弃，被忽略
        }
        else if (stepObj.contains("wait_stable"))
        {
            step.type = SequenceStep::WaitStable;
            QJsonObject ws = stepObj["wait_stable"].toObject();
            step.stableParam = ws["parameter"].toString();
            step.stableTarget = ws["target"].toDouble();
            step.stableTolerance = ws["tolerance"].toDouble(0.1);
            step.stableWindowSec = ws["window_sec"].toDouble(10.0);
            step.stableTimeoutSec = ws["timeout_sec"].toDouble(300.0);
        }
        else
        {
            m_error = QString("Step %1: missing type key (configure/capture/wait_stable)")
                .arg(m_steps.size());
            return false;
        }

        m_steps.append(step);
    }

    if (m_steps.isEmpty())
    {
        m_error = "No steps defined in sequence file";
        return false;
    }

    return true;
}
