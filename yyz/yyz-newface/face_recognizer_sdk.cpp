#include "face_recognizer_sdk.h"

#include <fstream>  // std::ifstream，用于检测模型文件是否存在

// ---------------------------------------------------------------------
// init: 初始化 SDK，加载 Haar 级联检测器与可选的 LBPH 识别模型
// ---------------------------------------------------------------------
bool FaceRecognizerSDK::init(const std::string &cascadePath, const std::string &modelPath) {
    // 每次重新初始化前先重置模型加载状态，避免残留上一次的加载结果
    m_modelLoaded = false;

    // 级联文件路径为空时直接返回失败
    if (cascadePath.empty()) {
        return false;
    }

    // 加载人脸检测级联文件；失败则返回 false
    if (!m_cascade.load(cascadePath)) {
        return false;
    }

    // 创建 LBPH 人脸识别器（使用默认参数：radius=1, neighbors=8, grid=8x8）
    m_recognizer = cv::face::LBPHFaceRecognizer::create();

    // 若提供了模型路径，则需要加载已训练好的识别模型
    if (!modelPath.empty()) {
        // 先检查文件是否存在，避免 read() 因文件缺失而抛出难以定位的异常
        std::ifstream modelFile(modelPath.c_str());
        if (!modelFile.good()) {
            return false;
        }
        modelFile.close();

        try {
            // 读取 LBPH 模型参数（yml 文件）
            m_recognizer->read(modelPath);
        } catch (const cv::Exception &) {
            // 模型文件损坏或格式不兼容
            return false;
        }
        m_modelLoaded = true;
    }

    return true;
}

// ---------------------------------------------------------------------
// processFrame: 对单帧视频进行人脸检测与识别
// 流程: 判空 -> 灰度化 -> 直方图均衡化 -> 人脸检测 -> 归一化 -> LBPH 预测 -> 阈值判定
// ---------------------------------------------------------------------
FaceResult FaceRecognizerSDK::processFrame(const cv::Mat &frame) {
    FaceResult result;  // 结构体已带有安全默认值：hasFace=false, label=-1, isRecognized=false

    // 输入帧为空或未成功初始化检测器时，直接返回"未检测到人脸"
    if (frame.empty() || m_cascade.empty()) {
        result.promptMsg = "未检测到人脸";
        return result;
    }

    // 统一转换为 8 位单通道灰度图，供检测与识别使用
    cv::Mat gray;
    if (!toGray8UC1(frame, gray)) {
        result.promptMsg = "未检测到人脸";
        return result;
    }

    // 直方图均衡化：增强光照鲁棒性，改善弱光/强光环境下的人脸检测与识别效果
    cv::Mat equalized;
    cv::equalizeHist(gray, equalized);

    // 在均衡化后的灰度图中检测人脸，并选取面积最大的一张
    cv::Rect faceRect;
    if (!detectLargestFace(equalized, faceRect)) {
        // 未检测到人脸
        result.promptMsg = "未检测到人脸";
        return result;
    }

    result.hasFace = true;   // 标记检测到人脸
    result.faceRect = faceRect;  // 记录人脸框位置，供 UI 层绘制

    // 将检测到的人脸裁剪并归一化为 200x200 标准尺寸
    cv::Mat normalizedFace;
    normalizeFace(equalized, faceRect, normalizedFace);
    if (normalizedFace.empty()) {
        // 裁剪结果异常（例如人脸矩形越界），无法继续识别
        result.promptMsg = "识别失败";
        return result;
    }

    // 尚未加载可用模型时无法做身份匹配，按"识别失败"处理
    if (!m_modelLoaded || m_recognizer.empty()) {
        result.promptMsg = "识别失败";
        return result;
    }

    int label = -1;          // 预测出的用户 ID
    double confidence = 0.0; // LBPH 置信度得分（越低越相似）
    try {
        // 使用 LBPH 模型对归一化人脸进行预测
        m_recognizer->predict(normalizedFace, label, confidence);
    } catch (const cv::Exception &) {
        // 模型状态异常导致预测失败
        result.promptMsg = "识别失败";
        return result;
    }

    result.confidence = confidence;  // 无论匹配与否，都回传原始置信度供上层参考

    // 阈值判定：LBPH 得分越小表示越相似，低于阈值即视为已知用户
    if (confidence < m_threshold) {
        result.label = label;
        result.isRecognized = true;
        result.promptMsg = "识别成功, 用户ID: " + std::to_string(label);
    } else {
        // 置信度不足，判定为陌生人/匹配失败
        result.label = -1;
        result.isRecognized = false;
        result.promptMsg = "识别失败";
    }

    return result;
}

// ---------------------------------------------------------------------
// extractFaceROI: 从单帧中截取并归一化一张 200x200 灰度人脸，供注册采样
// ---------------------------------------------------------------------
bool FaceRecognizerSDK::extractFaceROI(const cv::Mat &inputFrame, cv::Mat &outFaceRoi) {
    // 先清空输出，保证调用方不会误用上一次的残留数据
    outFaceRoi = cv::Mat();

    // 输入为空或检测器未初始化时直接失败
    if (inputFrame.empty() || m_cascade.empty()) {
        return false;
    }

    // 转灰度图
    cv::Mat gray;
    if (!toGray8UC1(inputFrame, gray)) {
        return false;
    }

    // 直方图均衡化：与识别流程保持一致，保证训练样本与识别输入分布一致
    cv::Mat equalized;
    cv::equalizeHist(gray, equalized);

    // 检测面积最大的人脸
    cv::Rect faceRect;
    if (!detectLargestFace(equalized, faceRect)) {
        return false;
    }

    // 裁剪并归一化为 200x200
    normalizeFace(equalized, faceRect, outFaceRoi);
    return !outFaceRoi.empty();
}

// ---------------------------------------------------------------------
// trainAndSaveModel: 使用样本重训 LBPH 模型并保存到磁盘
// 流程: 参数校验 -> 样本灰度化/均衡化/归一化 -> 训练 -> 写盘 -> 更新内存模型
// ---------------------------------------------------------------------
bool FaceRecognizerSDK::trainAndSaveModel(const std::vector<cv::Mat> &faceImages,
                                          const std::vector<int> &labels,
                                          const std::string &saveModelPath) {
    // 保存路径为空时直接返回失败
    if (saveModelPath.empty()) {
        return false;
    }

    // 先完成内存训练（trainModel 会校验样本与标签并更新内存模型）
    if (!trainModel(faceImages, labels)) {
        return false;
    }

    try {
        // 将训练好的模型写入 yml 文件
        m_recognizer->write(saveModelPath);
    } catch (const cv::Exception &) {
        // 写盘失败
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------
// trainModel: 仅在内存中重训 LBPH 模型，不写盘
// 应用场景：程序启动时从数据库加载所有人脸样本后重建模型，
//           或在注册/删除成员后快速更新内存模型。
// ---------------------------------------------------------------------
bool FaceRecognizerSDK::trainModel(const std::vector<cv::Mat> &faceImages,
                                   const std::vector<int> &labels) {
    // 样本为空、样本与标签数量不一致时视为参数非法
    if (faceImages.empty() || faceImages.size() != labels.size()) {
        return false;
    }

    std::vector<cv::Mat> normalizedImages;
    normalizedImages.reserve(faceImages.size());

    // 对每个样本做统一预处理：灰度化 -> 直方图均衡化 -> 200x200 归一化
    // 该流程与 processFrame 的识别输入保持一致，保证训练与推理分布相同
    for (size_t i = 0; i < faceImages.size(); ++i) {
        cv::Mat gray;
        if (!toGray8UC1(faceImages[i], gray)) {
            return false;
        }

        cv::Mat equalized;
        cv::equalizeHist(gray, equalized);

        cv::Mat resized;
        cv::resize(equalized, resized, cv::Size(kFaceSize, kFaceSize));
        normalizedImages.push_back(resized);
    }

    // 创建新的 LBPH 识别器用于本次训练
    cv::Ptr<cv::face::LBPHFaceRecognizer> recognizer = cv::face::LBPHFaceRecognizer::create();

    try {
        recognizer->train(normalizedImages, labels);
    } catch (const cv::Exception &) {
        return false;
    }

    // 训练成功后替换内存中的旧模型，后续 processFrame 立即使用新模型
    m_recognizer = recognizer;
    m_modelLoaded = true;
    return true;
}

// ---------------------------------------------------------------------
// setThreshold: 动态调整识别置信度阈值，负值统一修正为 0
// ---------------------------------------------------------------------
void FaceRecognizerSDK::setThreshold(double threshold) {
    m_threshold = threshold < 0.0 ? 0.0 : threshold;
}

// ---------------------------------------------------------------------
// toGray8UC1: 将输入图像统一转换为 8 位单通道灰度图
// ---------------------------------------------------------------------
bool FaceRecognizerSDK::toGray8UC1(const cv::Mat &src, cv::Mat &gray) {
    // 空图直接失败
    if (src.empty()) {
        return false;
    }

    // 单通道图像：若已是 8 位则直接引用，否则做类型转换
    if (src.channels() == 1) {
        if (src.depth() == CV_8U) {
            gray = src;
        } else {
            src.convertTo(gray, CV_8U);
        }
        return true;
    }

    // 三通道彩色图：按 BGR 转灰度
    if (src.channels() == 3) {
        cv::cvtColor(src, gray, cv::COLOR_BGR2GRAY);
        return true;
    }

    // 四通道彩色图：按 BGRA 转灰度
    if (src.channels() == 4) {
        cv::cvtColor(src, gray, cv::COLOR_BGRA2GRAY);
        return true;
    }

    // 其他通道数（如 2 通道等）暂不支持
    return false;
}

// ---------------------------------------------------------------------
// detectLargestFace: 检测灰度图中的所有人脸，返回面积最大的那张
// ---------------------------------------------------------------------
bool FaceRecognizerSDK::detectLargestFace(const cv::Mat &grayFrame, cv::Rect &faceRect) {
    // 检测器未加载或输入为空时直接失败
    if (m_cascade.empty() || grayFrame.empty()) {
        return false;
    }

    std::vector<cv::Rect> faces;
    // 使用 Haar 级联进行多尺度人脸检测
    //   scaleFactor = 1.1  : 每次缩放比例，取值越接近 1 检测越精细但更耗时
    //   minNeighbors = 5   : 邻域窗口数量，越大误检越少
    //   minSize = 60x60    : 过滤掉过小的候选区域，减少噪声干扰
    m_cascade.detectMultiScale(grayFrame, faces, 1.1, 5, 0, cv::Size(60, 60));

    if (faces.empty()) {
        return false;
    }

    // 遍历所有候选框，找出面积最大的一张人脸
    int bestIndex = 0;
    int bestArea = faces[0].area();
    for (size_t i = 1; i < faces.size(); ++i) {
        int area = faces[i].area();
        if (area > bestArea) {
            bestArea = area;
            bestIndex = static_cast<int>(i);
        }
    }

    faceRect = faces[bestIndex];
    return true;
}

// ---------------------------------------------------------------------
// normalizeFace: 按人脸矩形裁剪灰度图并缩放为 200x200 标准尺寸
// ---------------------------------------------------------------------
void FaceRecognizerSDK::normalizeFace(const cv::Mat &grayFrame,
                                      const cv::Rect &faceRect,
                                      cv::Mat &normalizedFace) {
    // 先清空输出，保证异常情况下返回的是空矩阵
    normalizedFace = cv::Mat();

    if (grayFrame.empty()) {
        return;
    }

    // 将人脸矩形与图像有效范围求交集，避免越界访问导致崩溃
    cv::Rect roi = faceRect & cv::Rect(0, 0, grayFrame.cols, grayFrame.rows);
    if (roi.width <= 0 || roi.height <= 0) {
        return;
    }

    // 裁剪人脸区域并缩放到 200x200，作为 LBPH 的标准输入尺寸
    cv::resize(grayFrame(roi), normalizedFace, cv::Size(kFaceSize, kFaceSize));
}
