#pragma once

// =====================================================================
// 文件名称: face_recognizer_sdk.h
// 功能说明: 人脸识别模块 SDK 头文件
//           - 基于 OpenCV4 的 Haar 级联人脸检测 + LBPH 人脸识别
//           - 对外提供初始化、实时识别、人脸采样、模型重训等接口
//           - 纯 C++ 实现，不依赖任何 UI / Qt / 数据库代码
// =====================================================================

#include <opencv2/opencv.hpp>  // OpenCV 核心、图像处理、级联分类器等模块
#include <opencv2/face.hpp>    // OpenCV contrib 中的人脸识别器（LBPH 等）

#include <string>  // std::string
#include <vector>  // std::vector

// ---------------------------------------------------------------------
// FaceResult: 单帧人脸识别结果结构体
// 调用方（通常是门禁业务层）根据该结构体中的字段决定是否开锁并更新 UI。
// ---------------------------------------------------------------------
struct FaceResult {
    bool hasFace = false;        // 当前帧是否检测到人脸
    int label = -1;              // 匹配到的用户 ID；-1 表示未知/陌生人
    double confidence = 0.0;     // LBPH 置信度得分，数值越小表示越相似
    bool isRecognized = false;   // 是否为通过认证的已知用户
    std::string promptMsg;       // 状态提示文本，例如"识别成功"、"识别失败"、"未检测到人脸"
    cv::Rect faceRect;           // 检测到的人脸外接矩形，供 UI 层绘制检测框
};

// ---------------------------------------------------------------------
// FaceRecognizerSDK: 人脸识别 SDK 主体类
// 典型使用流程：
//   1. 调用 init() 加载 haarcascade XML 与已训练好的 LBPH 模型（yml）
//   2. 对每帧视频调用 processFrame() 获取识别结果
//   3. 注册新用户时调用 extractFaceROI() 采集人脸，再调用 trainAndSaveModel() 重训并保存模型
//   4. 可随时调用 setThreshold() 动态调整置信度判定阈值
// ---------------------------------------------------------------------
class FaceRecognizerSDK {
public:
    FaceRecognizerSDK() = default;
    ~FaceRecognizerSDK() = default;

    // 禁止拷贝：内部持有 OpenCV 分类器与识别器指针，避免隐式复制导致状态不一致
    FaceRecognizerSDK(const FaceRecognizerSDK &) = delete;
    FaceRecognizerSDK &operator=(const FaceRecognizerSDK &) = delete;

    // 初始化 SDK：加载人脸检测级联文件，并可选地加载已训练好的 LBPH 模型
    // 参数:
    //   cascadePath - haarcascade_frontalface_alt.xml 的绝对或相对路径
    //   modelPath    - 已训练 LBPH 模型路径（如 face_model.yml）；为空表示本次不加载模型
    // 返回:
    //   true  - 级联文件加载成功（且若提供了模型路径，模型也加载成功）
    //   false - 参数非法、级联文件加载失败或模型文件不存在/损坏
    bool init(const std::string &cascadePath, const std::string &modelPath = "");

    // 核心实时识别接口：对输入视频帧进行人脸检测与识别
    // 参数:
    //   frame - 输入视频帧（通常为 BGR 彩色图，也支持灰度图）
    // 返回:
    //   包含检测与识别结果的 FaceResult 结构体
    FaceResult processFrame(const cv::Mat &frame);

    // 截取并归一化单帧中的人脸 ROI，供注册采样使用
    // 参数:
    //   inputFrame - 输入视频帧
    //   outFaceRoi - 输出 200x200 的灰度人脸图（CV_8UC1）
    // 返回:
    //   true - 成功检测到人脸并完成归一化；false - 未检测到人脸或参数非法
    bool extractFaceROI(const cv::Mat &inputFrame, cv::Mat &outFaceRoi);

    // 使用采集到的人脸样本与标签重训 LBPH 模型，并保存到指定路径
    // 参数:
    //   faceImages    - 人脸样本图像列表（内部会统一转为灰度并归一化为 200x200）
    //   labels        - 与 faceImages 一一对应的用户 ID 列表
    //   saveModelPath - 模型保存路径（如 face_model.yml）
    // 返回:
    //   true - 训练并保存成功（同时更新内存中的模型）；false - 参数非法或训练/写盘失败
    bool trainAndSaveModel(const std::vector<cv::Mat> &faceImages,
                           const std::vector<int> &labels,
                           const std::string &saveModelPath);

    // 仅在内存中重训 LBPH 模型（不写盘），供应用启动或新增成员后快速更新模型
    // 参数:
    //   faceImages - 人脸样本图像列表（内部会统一转为灰度并归一化为 200x200）
    //   labels     - 与 faceImages 一一对应的用户 ID 列表
    // 返回:
    //   true - 训练成功并更新内存模型；false - 参数非法或训练失败
    bool trainModel(const std::vector<cv::Mat> &faceImages, const std::vector<int> &labels);

    // 动态调整识别置信度阈值（LBPH 得分越低越相似，低于阈值即判定为已知用户）
    // 参数:
    //   threshold - 新的置信度阈值；负值会被自动修正为 0
    void setThreshold(double threshold);

private:
    static const int kFaceSize = 200;  // 人脸归一化后的标准尺寸（宽 = 高 = 200）

    cv::CascadeClassifier m_cascade;                         // Haar 级联人脸检测器
    cv::Ptr<cv::face::LBPHFaceRecognizer> m_recognizer;      // LBPH 人脸识别器
    double m_threshold = 70.0;                               // 识别置信度阈值，默认 70.0
    bool m_modelLoaded = false;                              // 是否已加载可用的识别模型

    // 将任意输入图像统一转换为 8 位单通道灰度图（CV_8UC1）
    // 参数:
    //   src  - 输入图像（支持 CV_8U 灰度、BGR/BGRA 彩色等常见类型）
    //   gray - 输出灰度图
    // 返回:
    //   true - 转换成功；false - 图像为空或通道数不受支持
    bool toGray8UC1(const cv::Mat &src, cv::Mat &gray);

    // 在灰度图中检测人脸，并返回面积最大的人脸矩形
    // 参数:
    //   grayFrame - 已做直方图均衡化的灰度图
    //   faceRect  - 输出面积最大的人脸矩形
    // 返回:
    //   true - 检测到至少一张人脸；false - 未检测到人脸或输入非法
    bool detectLargestFace(const cv::Mat &grayFrame, cv::Rect &faceRect);

    // 根据人脸矩形从灰度图中裁剪并缩放到 200x200 的标准尺寸
    // 参数:
    //   grayFrame      - 已做直方图均衡化的灰度图
    //   faceRect       - 待裁剪的人脸矩形
    //   normalizedFace - 输出 200x200 的灰度人脸图
    void normalizeFace(const cv::Mat &grayFrame, const cv::Rect &faceRect, cv::Mat &normalizedFace);
};
