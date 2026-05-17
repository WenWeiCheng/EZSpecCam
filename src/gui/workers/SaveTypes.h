#ifndef SAVETYPES_H
#define SAVETYPES_H

#include <QString>

#include "CameraTypes.h"

/**
 * @struct SaveOptions
 * @brief 保存选项 - 跨所有格式统一
 */
struct SaveOptions
{
    bool saveOriginal = false;   ///< 是否保存原始数据 (before post-processing)
    bool saveMetadata = true;   ///< 是否保存元数据 JSON
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
