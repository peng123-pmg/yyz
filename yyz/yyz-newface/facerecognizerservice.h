#ifndef FACERECOGNIZERSERVICE_H
#define FACERECOGNIZERSERVICE_H

#include <QObject>
#include <QString>
#include <QList>
#include <QImage>
#include <QElapsedTimer>
#include <QMutex>
#include <QMetaType>

#include <opencv2/opencv.hpp>

#include "face_recognizer_sdk.h"
#include "facedb.h"

/* 允许 FaceResult 作为 Qt 跨线程信号参数（配合 qRegisterMetaType 使用） */
Q_DECLARE_METATYPE(FaceResult)

/* 人脸识别业务服务：组合 SDK 与 SQLite 数据库，供界面层调用 */
class FaceRecognizerService : public QObject {
    Q_OBJECT
public:
    explicit FaceRecognizerService(QObject *parent = nullptr);

    /* 初始化级联检测器、数据库，并从数据库重建 LBPH 模型 */
    bool init(const QString &cascadePath, const QString &dbPath,
              QString *error = nullptr);

    /* 对一帧 QImage 做人脸检测与识别 */
    FaceResult recognize(const QImage &image);

    /* 从一帧 QImage 截取 200x200 灰度人脸，供注册采样 */
    cv::Mat extractFaceRoi(const QImage &image);

    /* 注册新成员：写入数据库并重训模型 */
    bool registerMember(const QString &name, const QList<cv::Mat> &faces,
                        QString *error = nullptr);

    /* 删除成员及其全部样本，并重训模型 */
    bool deleteMember(int memberId, QString *error = nullptr);

    QList<MemberInfo> memberList() const;
    QString memberName(int label) const;
    void setThreshold(double threshold);

private:
    /* 从数据库加载所有样本并重建内存模型 */
    bool rebuildModel(QString *error = nullptr);

    FaceRecognizerSDK m_sdk;
    FaceDatabase m_db;

    /* 保护 LBPH 模型：识别（采集线程）与重训（界面线程）互斥访问 */
    QMutex m_modelMutex;

    /* 识别限频：避免每帧都做耗时检测导致界面卡顿 */
    FaceResult m_lastResult;
    bool m_hasLastResult = false;
    QElapsedTimer m_recogTimer;
};

#endif
