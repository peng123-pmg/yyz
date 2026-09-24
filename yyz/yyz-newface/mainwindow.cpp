/*===============================================
*   文件名称：mainwindow.cpp
*   描    述：人脸识别门禁主窗口 + 采集线程实现（Linux / Qt）
================================================*/
#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "camera.h"

#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QMessageBox>
#include <QCloseEvent>
#include <QPixmap>
#include <QPainter>
#include <QCoreApplication>

#include <cstring>

#include "admindialog.h"
#include "facerecognizerservice.h"

/* 像素值截断到 0..255 */
static inline unsigned char clamp255(int v)
{
    if (v < 0) return 0;
    if (v > 255) return 255;
    return static_cast<unsigned char>(v);
}

/*
 * YUYV（YUY2 4:2:2）转 RGB32。
 * 每 4 字节表示 2 个像素：Y0 U Y1 V。
 * 使用 Format_RGB32 而非 Format_RGB888，兼容较老版本的 Qt 5。
 */
static QImage yuyvToRgb(const unsigned char *yuyv, int width, int height)
{
    QImage img(width, height, QImage::Format_RGB32);
    if (img.isNull())
        return img;

    for (int y = 0; y < height; ++y) {
        const unsigned char *src = yuyv + static_cast<size_t>(y) * width * 2;
        QRgb *dst = reinterpret_cast<QRgb *>(img.scanLine(y));

        int x = 0;
        for (; x + 1 < width; x += 2) {
            int y0 = src[0];
            int u  = src[1] - 128;
            int y1 = src[2];
            int v  = src[3] - 128;

            int c0 = y0 - 16;
            int c1 = y1 - 16;

            int r0 = (298 * c0 + 409 * v + 128) >> 8;
            int g0 = (298 * c0 - 100 * u - 208 * v + 128) >> 8;
            int b0 = (298 * c0 + 516 * u + 128) >> 8;

            int r1 = (298 * c1 + 409 * v + 128) >> 8;
            int g1 = (298 * c1 - 100 * u - 208 * v + 128) >> 8;
            int b1 = (298 * c1 + 516 * u + 128) >> 8;

            dst[x]     = qRgb(clamp255(r0), clamp255(g0), clamp255(b0));
            dst[x + 1] = qRgb(clamp255(r1), clamp255(g1), clamp255(b1));

            src += 4;
        }

        /* 理论上的奇数宽度保护：最后一个像素复用前一组的 U/V */
        if (x < width) {
            int y0 = src[0];
            int u = 0, v = 0;
            if (x >= 2) {
                u = src[-3] - 128;
                v = src[-1] - 128;
            }
            int c = y0 - 16;
            int r = (298 * c + 409 * v + 128) >> 8;
            int g = (298 * c - 100 * u - 208 * v + 128) >> 8;
            int b = (298 * c + 516 * u + 128) >> 8;
            dst[x] = qRgb(clamp255(r), clamp255(g), clamp255(b));
        }
    }
    return img;
}

/* ------------------------- 采集线程 ------------------------- */

CaptureThread::CaptureThread(const QString &device, unsigned int width,
                             unsigned int height,
                             FaceRecognizerService *service,
                             QObject *parent)
    : QThread(parent), m_device(device), m_width(width), m_height(height),
      m_service(service), m_stop(false)
{
}

CaptureThread::~CaptureThread()
{
    stop();
    wait();
}

void CaptureThread::stop()
{
    m_stop = true;
}

void CaptureThread::run()
{
    unsigned int width = m_width;
    unsigned int height = m_height;
    camera_fmt_t fmt = CAMERA_FMT_UNKNOWN;

    int fd = camera_init(m_device.toLocal8Bit().constData(),
                         &width, &height, &fmt);
    if (fd < 0) {
        emit errorOccurred(QStringLiteral("无法打开摄像头设备：%1").arg(m_device));
        return;
    }

    if (camera_start(fd) != 0) {
        camera_exit(fd);
        emit errorOccurred(QStringLiteral("启动摄像头采集失败"));
        return;
    }

    emit statusChanged(QStringLiteral("采集 %1 x %2，格式 %3")
                       .arg(width)
                       .arg(height)
                       .arg(fmt == CAMERA_FMT_MJPEG ? QStringLiteral("MJPEG")
                                                    : QStringLiteral("YUYV")));

    while (!m_stop) {
        void *buf = nullptr;
        unsigned int size = 0;
        unsigned int index = 0;

        if (camera_dqbuf(fd, &buf, &size, &index) != 0) {
            if (m_stop)
                break;
            emit errorOccurred(QStringLiteral("采集数据超时或出错"));
            break;
        }

        QImage img;
        if (fmt == CAMERA_FMT_MJPEG) {
            QImage loaded;
            if (loaded.loadFromData(static_cast<const uchar *>(buf),
                                    static_cast<int>(size), "JPEG")) {
                img = loaded.convertToFormat(QImage::Format_RGB32);
            }
        } else {
            img = yuyvToRgb(static_cast<const unsigned char *>(buf),
                            static_cast<int>(width),
                            static_cast<int>(height));
        }

        camera_eqbuf(fd, index);

        if (!img.isNull()) {
            /* 识别放在采集线程里执行，避免阻塞 UI 线程导致界面卡顿 */
            const FaceResult result = m_service ? m_service->recognize(img)
                                                : FaceResult();
            emit frameReady(img, result);
        }
    }

    camera_stop(fd);
    camera_exit(fd);
    emit statusChanged(QStringLiteral("采集已停止"));
}

/* ------------------------- 主窗口 ------------------------- */

/* haarcascade 人脸检测模型相对路径（开发板/目标机部署位置） */
static QString resolveCascadePath()
{
    return QCoreApplication::applicationDirPath()
           + QStringLiteral("/haarcascade_frontalface_alt.xml");
}

static QString databasePath()
{
    return QCoreApplication::applicationDirPath()
           + QStringLiteral("/face_database.db");
}

void MainWindow::setStateDot(const QString &color)
{
    m_stateDot->setStyleSheet(QStringLiteral(
        "background-color: %1;").arg(color));
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), ui(new Ui::MainWindow), m_thread(nullptr),
      m_service(nullptr), m_frameCount(0)
{
    ui->setupUi(this);

    /* 初始化人脸识别服务：级联检测器 + SQLite 数据库 + LBPH 模型 */
    m_service = new FaceRecognizerService(this);
    QString initError;
    if (!m_service->init(resolveCascadePath(), databasePath(), &initError)) {
        QMessageBox::critical(this, QStringLiteral("初始化失败"), initError);
    }

    /* 保存 .ui 中控件的指针，方便后续代码直接使用 */
    m_videoLabel = ui->videoLabel;
    m_recogLabel = ui->recogLabel;
    m_stateDot = ui->stateDot;
    m_statusLabel = ui->statusLabel;
    m_deviceCombo = ui->deviceCombo;
    m_startButton = ui->startButton;
    m_stopButton = ui->stopButton;
    m_adminButton = ui->adminButton;

    setStateDot(QStringLiteral("#8a919c"));
    updateRecognitionLabel(FaceResult());

    connect(m_startButton, &QPushButton::clicked, this, &MainWindow::onStart);
    connect(m_stopButton, &QPushButton::clicked, this, &MainWindow::onStop);
    connect(m_adminButton, &QPushButton::clicked, this, &MainWindow::openAdmin);
}

MainWindow::~MainWindow()
{
    onStop();
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    onStop();
    event->accept();
}

void MainWindow::onStart()
{
    if (m_thread) {
        onStop();
    }

    const QString device = m_deviceCombo->currentText().trimmed();

    m_thread = new CaptureThread(device, kCaptureWidth, kCaptureHeight,
                                 m_service, this);
    connect(m_thread, &CaptureThread::frameReady,
            this, &MainWindow::onFrame);
    connect(m_thread, &CaptureThread::statusChanged,
            this, &MainWindow::onStatus);
    connect(m_thread, &CaptureThread::errorOccurred,
            this, &MainWindow::onError);

    m_frameCount = 0;
    m_fpsTimer.invalidate();

    m_startButton->setEnabled(false);
    m_stopButton->setEnabled(true);
    setStateDot(QStringLiteral("#2f9e63"));
    m_statusLabel->setText(QStringLiteral("正在打开设备…"));
    m_thread->start();
}

void MainWindow::onStop()
{
    if (!m_thread)
        return;

    m_thread->stop();
    m_thread->wait();
    m_thread->deleteLater();
    m_thread = nullptr;

    m_startButton->setEnabled(true);
    m_stopButton->setEnabled(false);
    setStateDot(QStringLiteral("#8a919c"));
}

void MainWindow::onFrame(const QImage &image, const FaceResult &result)
{
    /* 缓存最近一帧，供管理员界面采集人脸使用 */
    m_lastFrame = image;

    QPixmap pix = QPixmap::fromImage(image);
    QPixmap scaled = pix.scaled(m_videoLabel->size(),
                                Qt::KeepAspectRatio,
                                Qt::SmoothTransformation);

    /* 在画面上绘制人脸框（绿色=识别成功，黄色=不匹配） */
    if (result.hasFace && result.faceRect.width > 0 &&
        result.faceRect.height > 0 && !scaled.isNull()) {
        QPainter painter(&scaled);
        const QColor color = result.isRecognized ? QColor("#2f9e63")
                                                 : QColor("#f0a020");
        painter.setPen(QPen(color, 2));

        const double sx = static_cast<double>(scaled.width()) / image.width();
        const double sy = static_cast<double>(scaled.height()) / image.height();
        const int ox = (m_videoLabel->width() - scaled.width()) / 2;
        const int oy = (m_videoLabel->height() - scaled.height()) / 2;

        QRect rect(ox + static_cast<int>(result.faceRect.x * sx),
                   oy + static_cast<int>(result.faceRect.y * sy),
                   static_cast<int>(result.faceRect.width * sx),
                   static_cast<int>(result.faceRect.height * sy));
        painter.drawRect(rect);
    }
    m_videoLabel->setPixmap(scaled);

    updateRecognitionLabel(result);

    /* 每秒刷新一次帧率显示 */
    ++m_frameCount;
    if (!m_fpsTimer.isValid()) {
        m_fpsTimer.start();
    } else {
        qint64 elapsed = m_fpsTimer.elapsed();
        if (elapsed >= 1000) {
            const double fps = m_frameCount * 1000.0 / elapsed;
            m_statusLabel->setText(QStringLiteral("%1 · %2 FPS")
                                   .arg(m_baseStatus)
                                   .arg(QString::number(fps, 'f', 1)));
            m_frameCount = 0;
            m_fpsTimer.restart();
        }
    }
}

void MainWindow::updateRecognitionLabel(const FaceResult &result)
{
    if (!m_recogLabel)
        return;

    if (!result.hasFace) {
        m_recogLabel->setText(QStringLiteral("未检测到人脸"));
        m_recogLabel->setStyleSheet(QStringLiteral("color:#8a919c;"));
    } else if (result.isRecognized) {
        const QString name = m_service ? m_service->memberName(result.label)
                                       : QString();
        const QString text = name.isEmpty()
            ? QStringLiteral("识别成功，已开门")
            : QStringLiteral("识别成功，已开门 · %1").arg(name);
        m_recogLabel->setText(text);
        m_recogLabel->setStyleSheet(QStringLiteral("color:#2f9e63;"));
    } else {
        m_recogLabel->setText(QStringLiteral("人脸不匹配"));
        m_recogLabel->setStyleSheet(QStringLiteral("color:#d95050;"));
    }
}

void MainWindow::openAdmin()
{
    if (!m_service)
        return;

    AdminDialog dialog(m_service, [this]() { return m_lastFrame; }, this);
    dialog.exec();
}

void MainWindow::onStatus(const QString &message)
{
    m_baseStatus = message;
    m_statusLabel->setText(message);
}

void MainWindow::onError(const QString &message)
{
    m_statusLabel->setText(QStringLiteral("错误：") + message);
    QMessageBox::warning(this, QStringLiteral("摄像头"), message);
    onStop();
    setStateDot(QStringLiteral("#d95050"));
}
