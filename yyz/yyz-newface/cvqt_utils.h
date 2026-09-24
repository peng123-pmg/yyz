#ifndef CVQT_UTILS_H
#define CVQT_UTILS_H

#include <QImage>
#include <opencv2/opencv.hpp>

/* QImage 转 cv::Mat（输出为连续的灰度 CV_8UC1 图像） */
cv::Mat qImageToCvMat(const QImage &image);

#endif
