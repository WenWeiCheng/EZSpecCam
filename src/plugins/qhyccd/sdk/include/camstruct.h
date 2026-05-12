#include <stdint.h>
#include <QDateTime>

#ifndef CAMSTRUCT_H
#define CAMSTRUCT_H

struct CAM
{
    double ChipW;
    double ChipH;
    double PixelW;
    double PixelH;
    uint32_t ImageW;
    uint32_t ImageH;
    uint32_t ImageB;

    uint32_t ImgW;
    uint32_t ImgH;
    uint32_t ImgB;
    uint32_t ImgC;
    uint32_t ShowImgW;
    uint32_t ShowImgH;
    uint32_t ShowImgB;
    uint32_t ShowImgC;

    bool Quit;
    bool HasQuit;

    uint32_t Frame;
    uint32_t FrameLast;
    uint32_t FrameCount;
    qint64 Time;
    qint64 TimeLast;
    qint64 TimeCount;

    unsigned char *ImgData;
    unsigned char *ShowImgData;
};

extern struct CAM CamInfo;

#endif // CAMSTRUCT_H
