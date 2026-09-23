#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QMqttClient>
#include <QTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <QDateTime>

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
    void on_btn_switch_to_monitor_clicked();

    // === 监控页 ===
    void on_btn_switch_to_home_clicked();
    void on_pushButton_clicked();
    void connectSuccess();
    void on_pushButton_2_clicked();
    void on_pushButton_3_clicked();
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

    // ============ 全局硬件实例 ============
    HardwareController hw;
    Light    light;
    Fan      fan;
    TempHum  temphum;

    QTimer  *timer = nullptr;

    // ============ 边缘检测状态 ============
    int  lastKey1   = -1;
    int  lastKey2   = -1;
    int  lastKey3   = -1;
    int  lastPeople = -1;    // ← 人体红外上次值
    int  lastGate   = -1;

    bool firstRun = true;

    // ============ 报警状态（分来源）============
    bool gateAlarmActive   = false;   // 门禁报警
    bool peopleAlarmActive = false;   // 红外报警

    // 子界面
    camera    *came_page = nullptr;
    fanDialog *fanbox    = nullptr;

    bool ledState = false;

    // ============ 辅助函数 ============
    void publishSensorData();
    void handleCommand(const QJsonObject &cmd);
    void sendEvent(const QString &topic, const QString &event);

    // ============ GPIO 读取 ============
    int readKey1()   { return hw.readGPIO(5, 9);  }
    int readKey2()   { return hw.readGPIO(5, 7);  }
    int readKey3()   { return hw.readGPIO(5, 8);  }
    int readPeople() { return hw.readGPIO(5, 12); }   // ← 红外
    int readGate()   { return hw.readGPIO(4, 15); }

    // ============ 场景模式 ============
    void enterHomeMode();
    void enterMovieMode();
    void enterAwayMode();

    // ============ 报警处理 ============
    void handleGateAlarm();        // 门禁报警
    void handlePeopleAlarm();      // ← 红外报警（新增）
    void tryStopAlarm();           // 尝试停止报警（检查其他报警是否还在）

    // ============ 日志 ============
    void log(const QString &msg);
};

#endif // MAINWINDOW_H
