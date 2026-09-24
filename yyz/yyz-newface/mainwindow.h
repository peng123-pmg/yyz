/*===============================================
*   文件名称：mainwindow.h
*   描    述：人脸识别门禁主窗口 + 采集线程（Linux / Qt）
================================================*/
#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QCloseEvent>
#include <QImage>
#include <QString>
#include <QThread>
#include <QElapsedTimer>
#include <atomic>

#include "face_recognizer_sdk.h"

namespace Ui {
class MainWindow;
}

class QLabel;
class QPushButton;
class QComboBox;
class FaceRecognizerService;

/* 采集分辨率：界面不提供宽高调节，固定使用该分辨率。
 * 开发板 CPU 较弱，降到 320x240 可大幅降低 YUV 转 RGB 与界面缩放的开销。 */
const unsigned int kCaptureWidth  = 320;
const unsigned int kCaptureHeight = 240;

/* 后台采集线程：V4L2 取流 + 转 RGB888 + 信号通知界面 */
class CaptureThread : public QThread
{
    Q_OBJECT
public:
    explicit CaptureThread(const QString &device, unsigned int width,
                           unsigned int height, FaceRecognizerService *service,
                           QObject *parent = nullptr);
    ~CaptureThread() override;

    /* 请求线程停止 */
    void stop();

protected:
    void run() override;

signals:
    void frameReady(const QImage &image, const FaceResult &result);
    void statusChanged(const QString &message);
    void errorOccurred(const QString &message);

private:
    QString m_device;
    unsigned int m_width;
    unsigned int m_height;
    FaceRecognizerService *m_service;
    std::atomic<bool> m_stop;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onStart();
    void onStop();
    void onFrame(const QImage &image, const FaceResult &result);
    void onStatus(const QString &message);
    void onError(const QString &message);
    void openAdmin();

private:
    void setStateDot(const QString &color);
    void updateRecognitionLabel(const FaceResult &result);

    Ui::MainWindow *ui;

    QLabel *m_videoLabel;
    QLabel *m_recogLabel;
    QLabel *m_statusLabel;
    QLabel *m_stateDot;
    QComboBox *m_deviceCombo;
    QPushButton *m_startButton;
    QPushButton *m_stopButton;
    QPushButton *m_adminButton;

    CaptureThread *m_thread;
    FaceRecognizerService *m_service;

    /* 最近一帧画面，供管理员界面采集人脸使用 */
    QImage m_lastFrame;

    QElapsedTimer m_fpsTimer;
    int m_frameCount;
    QString m_baseStatus;
};

#endif
