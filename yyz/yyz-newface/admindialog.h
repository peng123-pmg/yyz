#ifndef ADMINDIALOG_H
#define ADMINDIALOG_H

#include <QDialog>
#include <QImage>
#include <QList>
#include <functional>

#include <opencv2/opencv.hpp>

class QComboBox;
class QLabel;
class QTableWidget;
class QTimer;
class QPushButton;
class FaceRecognizerService;

/* 管理员界面：查看/删除成员，采集人脸并注册新成员 */
class AdminDialog : public QDialog {
    Q_OBJECT
public:
    /* frameProvider 用于从主窗口获取当前最新画面帧 */
    AdminDialog(FaceRecognizerService *service,
                std::function<QImage()> frameProvider,
                QWidget *parent = nullptr);

private slots:
    void onPreviewTimeout();
    void onStartCapture();
    void onCaptureTimeout();
    void onDeleteSelected();

private:
    void refreshMemberTable();
    void finishRegistration();

    FaceRecognizerService *m_service;
    std::function<QImage()> m_frameProvider;

    QComboBox *m_nameCombo;
    QLabel *m_previewLabel;
    QLabel *m_captureCountLabel;
    QTableWidget *m_memberTable;
    QTimer *m_previewTimer;
    QTimer *m_captureTimer;
    QPushButton *m_captureButton;
    QList<cv::Mat> m_pendingFaces;
};

#endif
