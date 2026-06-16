#pragma once

#include "core/CameraTypes.h"
#include "picam.h"

#include <QHash>
#include <QString>

struct PicamParameterRecord {
    PicamParameter    param;
    QString           name;
    QString           displayName;
    QString           description;
    ParameterCategory category;
    QVariant          fallbackDefault;
    float             order;
};

const QHash<PicamParameter, PicamParameterRecord> &picamParameterRegistry();
const QHash<QString, PicamParameter> &picamReverseNameMap();
const PicamParameterRecord *findByPicamParam(PicamParameter param);
const PicamParameterRecord *findByName(const QString &name);
QString picamParamName(PicamParameter param);
PicamParameter picamParamFromName(const QString &name);
ParameterCategory picamParamCategory(PicamParameter param);
