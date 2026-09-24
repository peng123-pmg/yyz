#include "cvqt_utils.h"

cv::Mat qImageToCvMat(const QImage &image)
{
    if (image.isNull())
        return cv::Mat();

    // 统一转为 RGB888（3 字节/像素，顺序 R,G,B），再交给 OpenCV
    const QImage rgb = image.convertToFormat(QImage::Format_RGB888);
    if (rgb.isNull())
        return cv::Mat();

    cv::Mat mat(rgb.height(), rgb.width(), CV_8UC3,
                const_cast<uchar *>(rgb.constBits()),
                static_cast<size_t>(rgb.bytesPerLine()));

    // 直接转为灰度图，避免多一次 BGR 中间拷贝；人脸识别只需要灰度
    cv::Mat gray;
    cv::cvtColor(mat, gray, cv::COLOR_RGB2GRAY);
    return gray;
}
