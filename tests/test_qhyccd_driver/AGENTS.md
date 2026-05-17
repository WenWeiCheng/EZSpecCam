# QHYCCD Driver Detial

## Driver Location

`C:\Program Files\QHYCCD\AllInOne\sdk\x64`

## 图像数据结构

通过 GetQHYCCDSingleFrame 和 GetQHYCCDLiveFrame 函数获取图像数据时，SDK 会将二维图像的像素数据按顺序读取出来依次存放到一维数组中，读取像素数据顺序为按照Z 形读取顺序，即从左到右从上到下。 

根据图像数据格式的不同，每个像素的数据结构也会发生变化，RAW8 或 MONO8 图像数据 为一个 unsigned char 变量存储一个像素的数据；RAW16 或 MONO16 图像数据为两个 unsigned char 的变量存储一个像素的数据，此时需要考虑数据的高低位，低位在前，高位在后，像素 值计算方式为高位数据乘以 256 加上低位数据；RGB24 图像数据为三个 unsigned char 数据存 储一个像素的数据，彩色通道顺序为 BGR。当使用 unsigned char 类型一维数组存储时，具体如下：

- RAW8 或 MONO8 格式数据： 
ImgData[0]：第一行第一个像素数据，变量值即为像素值 
ImgData[1]：第一行第二个像素数据，变量值即为像素值 
后面以此类推

- RAW16 或 MONO16 格式数据：
ImgData[0], ImgData[1]：第一行第一个像素，像素值为 ImgData[1]*256+ImgData[0] 
ImgData[2], ImgData[3]：第一行第二个像素，像素值为 ImgData[3]*256+ImgData[2] 
后面以此类推

- RGB24 格式数据：
ImgData[0], ImgData[1], ImgData[2]：第一行第一个像素，变量值分别为 B、G、R 三个通道 
的像素值
ImgData[3], ImgData[4], ImgData[5]：第一行第二个像素，变量值分别为 B、G、R 三个通道 
的像素值
后面以此类推