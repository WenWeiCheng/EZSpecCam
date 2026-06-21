#ifndef SAVETYPES_H
#define SAVETYPES_H

#include <QString>

#include "CameraTypes.h"

/**
 * @struct SaveOptions
 * @brief 保存选项 - 跨所有格式统一
 *
 * 新格式：始终保存原始 2D 图 + _metadata.json，不再提供开关。
 */
struct SaveOptions
{
    QString prefix;             ///< 文件名前缀
    QString suffix;             ///< 文件名后缀
};

/**
 * @struct SaveRequest
 * @brief 保存请求 - 统一的数据结构
 */
struct SaveRequest
{
    ImageData frame;           ///< 包含 image, originalImage, spectrum
    QString filePath;          ///< 目标文件路径
    SaveOptions options;       ///< 保存选项
};

#endif // SAVETYPES_H
