#include "facerecognizerservice.h"

#include <vector>

#include "cvqt_utils.h"

namespace {
/* 两次识别之间的最小间隔（毫秒），超过该间隔才重新跑一次人脸检测 */
const int kRecognitionIntervalMs = 200;
}

FaceRecognizerService::FaceRecognizerService(QObject *parent)
    : QObject(parent)
{
}

bool FaceRecognizerService::init(const QString &cascadePath, const QString &dbPath,
                                 QString *error)
{
    if (!m_db.open(dbPath, error))
        return false;

    if (!m_sdk.init(cascadePath.toStdString())) {
        if (error)
            *error = QStringLiteral("加载人脸检测模型失败：%1").arg(cascadePath);
        return false;
    }

    return rebuildModel(error);
}

FaceResult FaceRecognizerService::recognize(const QImage &image)
{
    // 限频：间隔不足时不重复检测，直接返回上一次结果
    if (m_hasLastResult && m_recogTimer.isValid() &&
        m_recogTimer.elapsed() < kRecognitionIntervalMs) {
        return m_lastResult;
    }
    m_recogTimer.restart();

    cv::Mat gray = qImageToCvMat(image);
    if (gray.empty()) {
        m_lastResult = FaceResult();
        m_hasLastResult = true;
        return m_lastResult;
    }

    // 加锁执行识别，避免与注册/删除时的模型重训产生数据竞争
    {
        QMutexLocker locker(&m_modelMutex);
        m_lastResult = m_sdk.processFrame(gray);
    }

    m_hasLastResult = true;
    return m_lastResult;
}

cv::Mat FaceRecognizerService::extractFaceRoi(const QImage &image)
{
    const cv::Mat frame = qImageToCvMat(image);
    if (frame.empty())
        return cv::Mat();

    cv::Mat face;
    if (!m_sdk.extractFaceROI(frame, face))
        return cv::Mat();
    return face;
}

bool FaceRecognizerService::registerMember(const QString &name,
                                           const QList<cv::Mat> &faces,
                                           QString *error)
{
    const QString trimmed = name.trimmed();
    if (trimmed.isEmpty()) {
        if (error) *error = QStringLiteral("姓名不能为空");
        return false;
    }
    if (faces.isEmpty()) {
        if (error) *error = QStringLiteral("请至少采集一张人脸");
        return false;
    }
    if (m_db.memberExists(trimmed)) {
        if (error) *error = QStringLiteral("该姓名已存在，请使用其它姓名");
        return false;
    }

    int memberId = -1;
    if (!m_db.transaction()) {
        if (error) *error = QStringLiteral("开启数据库事务失败");
        return false;
    }

    if (!m_db.addMember(trimmed, &memberId, error)) {
        m_db.rollback();
        return false;
    }

    for (const cv::Mat &face : faces) {
        if (face.empty())
            continue;

        std::vector<uchar> buf;
        if (!cv::imencode(".png", face, buf)) {
            m_db.rollback();
            if (error) *error = QStringLiteral("人脸样本编码失败");
            return false;
        }

        const QByteArray bytes(reinterpret_cast<const char *>(buf.data()),
                               static_cast<int>(buf.size()));
        if (!m_db.addSample(memberId, bytes, error)) {
            m_db.rollback();
            return false;
        }
    }

    if (!m_db.commit()) {
        m_db.rollback();
        if (error) *error = QStringLiteral("保存成员数据失败");
        return false;
    }

    if (!rebuildModel(error))
        return false;

    return true;
}

bool FaceRecognizerService::deleteMember(int memberId, QString *error)
{
    if (!m_db.removeMember(memberId, error))
        return false;

    if (!rebuildModel(error))
        return false;

    return true;
}

QList<MemberInfo> FaceRecognizerService::memberList() const
{
    return m_db.members();
}

QString FaceRecognizerService::memberName(int label) const
{
    if (label < 0)
        return QString();
    return m_db.memberName(label);
}

void FaceRecognizerService::setThreshold(double threshold)
{
    m_sdk.setThreshold(threshold);
}

bool FaceRecognizerService::rebuildModel(QString *error)
{
    const QList<FaceDatabase::Sample> samples = m_db.samples();

    std::vector<cv::Mat> images;
    std::vector<int> labels;
    images.reserve(static_cast<size_t>(samples.size()));
    labels.reserve(static_cast<size_t>(samples.size()));

    for (const FaceDatabase::Sample &sample : samples) {
        if (sample.pngBytes.isEmpty())
            continue;

        const uchar *begin =
            reinterpret_cast<const uchar *>(sample.pngBytes.constData());
        const uchar *end = begin + sample.pngBytes.size();
        std::vector<uchar> buf(begin, end);
        cv::Mat img = cv::imdecode(buf, cv::IMREAD_GRAYSCALE);
        if (img.empty())
            continue;

        images.push_back(img);
        labels.push_back(sample.memberId);
    }

    // 尚无任何注册样本时，识别器保持未训练状态；
    // 此时仍能正常检测人脸，只是所有结果都会判为“人脸不匹配”。
    if (images.empty())
        return true;

    {
        QMutexLocker locker(&m_modelMutex);
        if (!m_sdk.trainModel(images, labels)) {
            if (error) *error = QStringLiteral("重建识别模型失败");
            return false;
        }
    }
    return true;
}
