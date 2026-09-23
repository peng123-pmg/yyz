#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>

// ============================================================
// 构造 / 析构
// ============================================================
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    // 监控页只读框
    ui->lineEdit_temp->setReadOnly(true);
    ui->lineEdit_hum->setReadOnly(true);
    ui->lineEdit_light->setReadOnly(true);
    ui->lineEdit_fan->setReadOnly(true);

    // MQTT 默认参数
    ui->lineEdit->setText("192.168.7.100");
    ui->lineEdit_2->setText("1883");
    ui->lineEdit_3->setText("home/dev/cmd");
    ui->lineEdit_4->setText("home/dev/state");
    ui->pushButton_2->setText("开灯");

    ui->stackedWidget->setCurrentIndex(0);

    // MQTT 客户端
    client = new QMqttClient(this);
    connect(client, &QMqttClient::connected,       this, &MainWindow::connectSuccess);
    connect(client, &QMqttClient::messageReceived, this, &MainWindow::recv_data);

    // 风扇初始
    fan.set_speed(150);
    fan.start();
    ui->lineEdit_fan->setText(QString::number(fan.get_speed()));

    // LED 初始全灭
    hw.allLED(0);
    hw.setUserLED(1, 0);
    hw.setUserLED(2, 0);

    // ============================================================
    // 【关键】构造函数中先读一次传感器实际值，初始化 last*
    // 避免首次 update_all() 因 last* 与实际值不一致而误触发
    // ============================================================
    lastKey1   = readKey1();
    lastKey2   = readKey2();
    lastKey3   = readKey3();
    lastPeople = readPeople();   // ← 红外上次值初始化
    lastGate   = readGate();

    log(QString("【系统】传感器初始状态: KEY1=%1 KEY2=%2 KEY3=%3 人体=%4 门禁=%5")
        .arg(lastKey1).arg(lastKey2).arg(lastKey3)
        .arg(lastPeople).arg(lastGate));

    // 1 秒定时器
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::update_all);
    timer->start(1000);

    update_all();   // 首次刷新 UI，但不做边缘检测

    log("【系统】程序已启动，等待连接服务器...");
}

MainWindow::~MainWindow()
{
    fan.stop();
    hw.stopAlarm();
    hw.allLED(0);
    hw.setUserLED(1, 0);
    hw.setUserLED(2, 0);

    if (came_page) { delete came_page; came_page = nullptr; }
    if (fanbox)    { delete fanbox;    fanbox    = nullptr; }

    delete ui;
}

// ============================================================
// 日志
// ============================================================
void MainWindow::log(const QString &msg)
{
    QString line = QString("[%1] %2")
        .arg(QDateTime::currentDateTime().toString("hh:mm:ss"))
        .arg(msg);
    ui->textEdit->append(line);
    qDebug() << line;
}

// ============================================================
// 定时采集与场景联动
// ============================================================
void MainWindow::update_all()
{
    // ---------- 1. 读取传感器 ----------
    int    lightVal = light.get_light();
    double temp     = temphum.get_temp();
    double hum      = temphum.get_hum();
    int    fanSpeed = fan.get_speed();

    // ---------- 2. 刷新 UI ----------
    ui->lineEdit_light->setText(lightVal < 0 ? "Error" : QString::number(lightVal));
    ui->lineEdit_temp->setText(QString::number(temp, 'f', 2));
    ui->lineEdit_hum->setText(QString::number(hum,  'f', 2));
    ui->lineEdit_fan->setText(QString::number(fanSpeed));

    ui->status_temp->setText(QString("温度: %1 °C").arg(temp, 0, 'f', 1));
    ui->status_hum->setText(QString("湿度: %1 %").arg(hum, 0, 'f', 1));
    ui->status_light->setText(QString("光照: %1").arg(lightVal));
    ui->status_fan->setText(QString("风扇: %1").arg(fanSpeed));

    // ---------- 3. 读取 GPIO ----------
    int key1   = readKey1();
    int key2   = readKey2();
    int key3   = readKey3();
    int people = readPeople();   // ← 红外：1=无人，0=有人
    int gate   = readGate();

    // ---------- 4. 边缘检测 ----------
    if (!firstRun) {

        // ============ KEY1：回家模式 ============
        if (lastKey1 == 1 && key1 == 0) {
            log("【KEY1】按下 → 回家模式");
            enterHomeMode();
            sendEvent("home/scene/mode", "home_mode");
        }

        // ============ KEY2：观影模式 ============
        if (lastKey2 == 1 && key2 == 0) {
            log("【KEY2】按下 → 观影模式");
            enterMovieMode();
            sendEvent("home/scene/mode", "movie_mode");
        }

        // ============ KEY3：离家模式 ============
        if (lastKey3 == 1 && key3 == 0) {
            log("【KEY3】按下 → 离家模式");
            enterAwayMode();
            sendEvent("home/scene/mode", "away_mode");
        }

        // ============================================================
        // 【核心】红外检测：输出 1=无人，输出 0=检测到人
        // 下降沿 1 → 0 触发报警
        // ============================================================
        if (lastPeople == 1 && people == 0) {
            log("【人体红外】⚠️ 检测到人 → 触发报警！");
            handlePeopleAlarm();
            sendEvent("home/sensor/people", "people_alarm");
        }

        // 上升沿 0 → 1：人离开
        if (lastPeople == 0 && people == 1) {
            log("【人体红外】人已离开");
            // 如果红外报警还在激活，主动停止
            if (peopleAlarmActive) {
                peopleAlarmActive = false;
                tryStopAlarm();
            }
            sendEvent("home/sensor/people", "people_left");
        }

        // ============ 门禁报警 ============
        if (!gateAlarmActive && lastGate == 1 && gate == 0) {
            log("【门禁/火焰】⚠️ 触发报警！");
            handleGateAlarm();
            sendEvent("home/sensor/gate", "gate_alarm");
        }

        if (gateAlarmActive && lastGate == 0 && gate == 1) {
            gateAlarmActive = false;
            tryStopAlarm();
            log("【门禁/火焰】已恢复，报警可再次触发");
        }
    }

    // ---------- 5. 更新 last*（只有有效值才更新）----------
    if (key1   >= 0) lastKey1   = key1;
    if (key2   >= 0) lastKey2   = key2;
    if (key3   >= 0) lastKey3   = key3;
    if (people >= 0) lastPeople = people;
    if (gate   >= 0) lastGate   = gate;

    // 首次运行结束
    if (firstRun) {
        firstRun = false;
        log("【系统】首次初始化完成，开始边缘检测");
    }

    // ---------- 6. MQTT 上报 ----------
    if (client->state() == QMqttClient::Connected) {
        publishSensorData();
    }
}

// ============================================================
// 报警处理
// ============================================================

// 门禁报警
void MainWindow::handleGateAlarm()
{
    if (gateAlarmActive) return;

    hw.triggerAlarm();          // 蜂鸣器 + 震动马达
    hw.allLED(255);             // 所有 LED 全亮
    gateAlarmActive = true;

    QTimer::singleShot(5000, this, [this]() {
        if (gateAlarmActive) {
            gateAlarmActive = false;
            tryStopAlarm();
            log("【系统】门禁报警自动停止（5 秒超时）");
        }
    });
}

// 【核心】红外报警：gpioget 5 12 输出 0 时触发
void MainWindow::handlePeopleAlarm()
{
    if (peopleAlarmActive) return;   // 已在报警，避免重复触发

    hw.triggerAlarm();          // 蜂鸣器 1kHz + 震动马达
    hw.allLED(255);             // 3 路系统 LED 全亮
    peopleAlarmActive = true;

    // 5 秒后自动停止，防止长时间鸣叫损坏硬件
    QTimer::singleShot(5000, this, [this]() {
        if (peopleAlarmActive) {
            peopleAlarmActive = false;
            tryStopAlarm();
            log("【系统】红外报警自动停止（5 秒超时）");
        }
    });
}

// 尝试停止报警：只有当所有报警源都停止时才真正关闭硬件
void MainWindow::tryStopAlarm()
{
    if (!gateAlarmActive && !peopleAlarmActive) {
        hw.stopAlarm();       // 停止蜂鸣器 + 震动
        hw.allLED(0);         // LED 全灭
        log("【系统】报警已停止");
    }
}

// ============================================================
// 场景模式
// ============================================================
void MainWindow::enterHomeMode()
{
    hw.allLED(255);
    hw.setUserLED(1, 255);
    fan.set_speed(150);
    fan.start();
    update_all();
}

void MainWindow::enterMovieMode()
{
    hw.setLED(1, 30);
    hw.setLED(2, 0);
    hw.setLED(3, 0);
    hw.setUserLED(1, 0);
}

void MainWindow::enterAwayMode()
{
    hw.allLED(0);
    hw.setUserLED(1, 0);
    hw.setUserLED(2, 0);
    fan.stop();
    update_all();
}

// ============================================================
// MQTT 上报
// ============================================================
void MainWindow::publishSensorData()
{
    QJsonObject data;
    data["deviceId"]    = "fsmp1a_board";
    data["temperature"] = temphum.get_temp();
    data["humidity"]    = temphum.get_hum();
    data["light"]       = light.get_light();
    data["fan"]         = fan.get_speed();
    data["pot1"]        = hw.readPot(1);
    data["pot2"]        = hw.readPot(2);
    data["key1"]   = (readKey1()   == 0) ? 1 : 0;
    data["key2"]   = (readKey2()   == 0) ? 1 : 0;
    data["key3"]   = (readKey3()   == 0) ? 1 : 0;
    data["people"] = (readPeople() == 0) ? 1 : 0;   // 1=有人，0=无人
    data["gate"]   = (readGate()   == 0) ? 1 : 0;
    data["alarm"]  = (gateAlarmActive || peopleAlarmActive) ? 1 : 0;

    client->publish(QMqttTopicName("home/sensor/state"),
                    QJsonDocument(data).toJson(QJsonDocument::Compact));
}

void MainWindow::sendEvent(const QString &topic, const QString &event)
{
    if (client->state() != QMqttClient::Connected) return;
    QJsonObject obj;
    obj["event"] = event;
    obj["ts"]    = QDateTime::currentMSecsSinceEpoch();
    client->publish(QMqttTopicName(topic),
                    QJsonDocument(obj).toJson(QJsonDocument::Compact));
}

// ============================================================
// 磁贴主页
// ============================================================
void MainWindow::on_cam_clicked()
{
    if (came_page == nullptr) {
        came_page = new camera();
        connect(came_page, &camera::backToMain, this, &MainWindow::showMainWindow);
    }
    this->hide();
    came_page->resize(1024, 600);
    came_page->show();
}

void MainWindow::on_light_clicked()
{
    ledState = !ledState;
    hw.allLED(ledState ? 255 : 0);
    log(QString("【磁贴】灯光 %1").arg(ledState ? "开" : "关"));
}

void MainWindow::on_fan_pushbutton_clicked()
{
    if (fanbox == nullptr) {
        fanbox = new fanDialog();
        connect(fanbox, &fanDialog::backToMain,      this, &MainWindow::showMainWindow);
        connect(fanbox, &fanDialog::fanSpeedChanged, this, &MainWindow::onFanSpeedChanged);
    }
    fanbox->setFan(&fan);
    this->hide();
    fanbox->show();
}

void MainWindow::on_alarm_button_clicked()
{
    hw.triggerAlarm();
    hw.allLED(255);
    gateAlarmActive = true;
    log("【磁贴】⚠️ 报警已触发");

    QTimer::singleShot(5000, this, [this]() {
        if (gateAlarmActive) {
            gateAlarmActive = false;
            tryStopAlarm();
            log("【系统】报警自动停止（5 秒超时）");
        }
    });
}

void MainWindow::on_home_mode_btn_clicked()
{
    enterHomeMode();
    log("【场景】回家模式");
}

void MainWindow::on_movie_mode_btn_clicked()
{
    enterMovieMode();
    log("【场景】观影模式");
}

void MainWindow::on_away_mode_btn_clicked()
{
    enterAwayMode();
    log("【场景】离家模式");
}

void MainWindow::on_all_off_btn_clicked()
{
    hw.allLED(0);
    hw.setUserLED(1, 0);
    hw.setUserLED(2, 0);
    hw.stopAlarm();
    fan.stop();
    gateAlarmActive   = false;
    peopleAlarmActive = false;
    update_all();
    log("【场景】全部关闭");
}

void MainWindow::on_btn_switch_to_monitor_clicked()
{
    ui->stackedWidget->setCurrentIndex(1);
}

// ============================================================
// 监控页
// ============================================================
void MainWindow::on_btn_switch_to_home_clicked()
{
    ui->stackedWidget->setCurrentIndex(0);
}

void MainWindow::on_pushButton_clicked()
{
    if (client->state() == QMqttClient::Connected) {
        client->disconnectFromHost();
        ui->pushButton->setText("连接");
        return;
    }
    client->setHostname(ui->lineEdit->text().trimmed());
    client->setPort(ui->lineEdit_2->text().toInt());
    client->connectToHost();
    ui->pushButton->setText("连接中...");
}

void MainWindow::connectSuccess()
{
    QMessageBox::about(this, "提示", "连接服务器成功");
    ui->pushButton->setText("断开");
    log("【系统】已连接 MQTT 服务器");
}

void MainWindow::on_pushButton_2_clicked()
{
    if (client->state() != QMqttClient::Connected) {
        QMessageBox::warning(this, "警告", "请先连接服务器！");
        return;
    }
    bool turnOn = (ui->pushButton_2->text() == "开灯");
    QJsonObject cmd;
    cmd["lamp"] = turnOn;
    cmd["id"]   = 0;
    client->publish(QMqttTopicName(ui->lineEdit_3->text()),
                    QJsonDocument(cmd).toJson(QJsonDocument::Compact));
    hw.allLED(turnOn ? 255 : 0);
    ui->pushButton_2->setText(turnOn ? "关灯" : "开灯");
    log(turnOn ? "【发送】开灯" : "【发送】关灯");
}

void MainWindow::on_pushButton_3_clicked()
{
    if (client->state() != QMqttClient::Connected) {
        QMessageBox::warning(this, "警告", "请先连接服务器！");
        return;
    }
    if (isSubscribed) {
        QMessageBox::about(this, "提示", "已经订阅过了！");
        return;
    }
    client->subscribe(ui->lineEdit_4->text());
    isSubscribed = true;
    log("【系统】已订阅主题: " + ui->lineEdit_4->text());
}

void MainWindow::recv_data(const QByteArray &mess, const QMqttTopicName &topic)
{
    log(QString("【%1】 %2")
        .arg(topic.name()).arg(QString::fromUtf8(mess)));
    QJsonDocument doc = QJsonDocument::fromJson(mess);
    if (doc.isObject()) handleCommand(doc.object());
}

void MainWindow::handleCommand(const QJsonObject &cmd)
{
    QString action = cmd.value("action").toString();

    if (action == "set_fan") {
        int value = cmd["value"].toInt(-1);
        if (value >= 0 && value <= 255) {
            fan.set_speed(value);
            fan.start();
            ui->lineEdit_fan->setText(QString::number(value));
        }
    }
    else if (action == "set_led") {
        int id    = cmd["id"].toInt(-1);
        int value = cmd["value"].toInt(255);
        if (id >= 1 && id <= 3) hw.setLED(id, value);
        else if (id == 10)      hw.setUserLED(1, value);
        else if (id == 11)      hw.setUserLED(2, value);
    }
    else if (action == "all_led") {
        int value = cmd["value"].toInt(255);
        hw.allLED(value);
    }
    else if (action == "alarm") {
        hw.triggerAlarm();
        hw.allLED(255);
        gateAlarmActive = true;
        log("【MQTT】收到报警指令");
    }
    else if (action == "stop_alarm") {
        gateAlarmActive   = false;
        peopleAlarmActive = false;
        hw.stopAlarm();
        hw.allLED(0);
        log("【MQTT】收到停止报警指令");
    }
    else if (cmd.contains("lamp")) {
        bool lampState = cmd["lamp"].toBool();
        hw.allLED(lampState ? 255 : 0);
    }
}

void MainWindow::on_pushButton_minus_clicked()
{
    int val = ui->lineEdit_fan->text().toInt();
    val -= 10; if (val < 0) val = 0;
    fan.set_speed(val);
    fan.start();
    ui->lineEdit_fan->setText(QString::number(val));
}

void MainWindow::on_pushButton_plus_clicked()
{
    int val = ui->lineEdit_fan->text().toInt();
    val += 10; if (val > 255) val = 255;
    fan.set_speed(val);
    fan.start();
    ui->lineEdit_fan->setText(QString::number(val));
}

// ============================================================
// 子界面返回
// ============================================================
void MainWindow::showMainWindow()
{
    this->show();
}

void MainWindow::onFanSpeedChanged(int speed)
{
    ui->lineEdit_fan->setText(QString::number(speed));
    log(QString("【风扇界面】速度调整为 %1").arg(speed));
}
