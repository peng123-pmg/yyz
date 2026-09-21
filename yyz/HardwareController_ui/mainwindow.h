#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMqttClient>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>

#include "camera.h"
#include "fandialog.h"
#include "light.h"
#include "Fan.h"
#include "temp_hum.h"
#include "HardwareController.h"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

private slots:
    // === 磁贴主页 ===
    void on_cam_clicked();
    void on_light_clicked();
    void on_fan_pushbutton_clicked();
    void on_alarm_button_clicked();
    void on_home_mode_btn_clicked();
    void on_movie_mode_btn_clicked();
    void on_away_mode_btn_clicked();
    void on_all_off_btn_clicked();
    void on_btn_switch_to_monitor_clicked();   // 切到监控页

    // === 监控页 ===
    void on_btn_switch_to_home_clicked();      // 切回主页
    void on_pushButton_clicked();              // MQTT 连接/断开
    void connectSuccess();
    void on_pushButton_2_clicked();            // 开/关灯
    void on_pushButton_3_clicked();            // 订阅
    void recv_data(const QByteArray &mess, const QMqttTopicName &topic);
    void on_pushButton_minus_clicked();
    void on_pushButton_plus_clicked();

    // === 定时器 ===
    void update_all();

    // === 子界面返回 ===
    void showMainWindow();
    void onFanSpeedChanged(int speed);

private:
    Ui::MainWindow *ui;

    QMqttClient *client = nullptr;
    bool isSubscribed = false;

    // 全局共享硬件实例（两个页面共用）
    Light    light;
    Fan      fan;
    TempHum  temphum;

    QTimer  *timer = nullptr;

    // 边缘检测状态
    int lastKey1 = 1, lastKey2 = 1, lastKey3 = 1, lastPeople = 1, lastGate = 1;

    // 子界面
    camera    *came_page = nullptr;
    fanDialog *fanbox    = nullptr;

    bool ledState = false;   // 磁贴灯状态

    void publishSensorData();
    void handleCommand(const QJsonObject &cmd);
    void sendEvent(const QString &topic, const QString &event);

    int readKey1()   { return HardwareController::readGPIO(5, 9);  }
    int readKey2()   { return HardwareController::readGPIO(5, 7);  }
    int readKey3()   { return HardwareController::readGPIO(5, 8);  }
    int readPeople() { return HardwareController::readGPIO(5, 12); }
    int readGate()   { return HardwareController::readGPIO(4, 15); }
};

#endif // MAINWINDOW_H
