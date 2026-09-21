#include "mainwindow.h"
#include "ui_mainwindow.h"
#include <QMessageBox>
#include <QDebug>

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

    // 默认显示磁贴主页
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
    HardwareController::allLED(0);
    HardwareController::setUserLED(1, 0);
    HardwareController::setUserLED(2, 0);

    // 1 秒定时器
    timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &MainWindow::update_all);
    timer->start(1000);

    update_all();
    ui->textEdit->append("【系统】程序已启动，等待连接服务器...");
}

MainWindow::~MainWindow()
{
    fan.stop();
    HardwareController::stopAlarm();
    HardwareController::allLED(0);
    HardwareController::setUserLED(1, 0);
    HardwareController::setUserLED(2, 0);

    if (came_page) { delete came_page; came_page = nullptr; }
    if (fanbox)    { delete fanbox;    fanbox    = nullptr; }

    delete ui;
}

// ==================== 定时采集与场景联动 ====================
void MainWindow::update_all()
{
    int    lightVal = light.get_light();
    double temp     = temphum.get_temp();
    double hum      = temphum.get_hum();
    int    fanSpeed = fan.get_speed();

    // 监控页显示
    ui->lineEdit_light->setText(lightVal < 0 ? "Error" : QString::number(lightVal));
    ui->lineEdit_temp->setText(QString::number(temp, 'f', 2));
    ui->lineEdit_hum->setText(QString::number(hum,  'f', 2));
    ui->lineEdit_fan->setText(QString::number(fanSpeed));

    // 主页顶部状态栏
    ui->status_temp->setText(QString("温度: %1 °C").arg(temp, 0, 'f', 1));
    ui->status_hum->setText(QString("湿度: %1 %").arg(hum, 0, 'f', 1));
    ui->status_light->setText(QString("光照: %1").arg(lightVal));
    ui->status_fan->setText(QString("风扇: %1").arg(fanSpeed));

    // GPIO
    int key1   = readKey1();
    int key2   = readKey2();
    int key3   = readKey3();
    int people = readPeople();
    int gate   = readGate();

    if (lastKey1 == 1 && key1 == 0) {
        ui->textEdit->append("【KEY1】按下 → 回家模式");
        HardwareController::allLED(255);
        HardwareController::setUserLED(1, 255);
        fan.set_speed(150);
        fan.start();
        sendEvent("home/scene/mode", "home_mode");
    }
    if (lastKey2 == 1 && key2 == 0) {
        ui->textEdit->append("【KEY2】按下 → 观影模式");
        HardwareController::setLED(1, 30);
        HardwareController::setLED(2, 0);
        HardwareController::setLED(3, 0);
        HardwareController::setUserLED(1, 0);
        sendEvent("home/scene/mode", "movie_mode");
    }
    if (lastKey3 == 1 && key3 == 0) {
        ui->textEdit->append("【KEY3】按下 → 离家模式");
        HardwareController::allLED(0);
        HardwareController::setUserLED(1, 0);
        HardwareController::setUserLED(2, 0);
        fan.stop();
        sendEvent("home/scene/mode", "away_mode");
    }
    if (lastPeople == 1 && people == 0) {
        ui->textEdit->append("【人体红外】检测到人 → 迎宾模式");
        HardwareController::setUserLED(2, 255);
        sendEvent("home/sensor/people", "people_detected");
    }
    if (lastPeople == 0 && people == 1) {
        ui->textEdit->append("【人体红外】人已离开");
        HardwareController::setUserLED(2, 0);
        sendEvent("home/sensor/people", "people_left");
    }
    if (lastGate == 1 && gate == 0) {
        ui->textEdit->append("【门禁/火焰】⚠️ 触发报警！");
        HardwareController::triggerAlarm();
        HardwareController::allLED(255);
        sendEvent("home/sensor/gate", "gate_alarm");
    }

    lastKey1 = key1; lastKey2 = key2; lastKey3 = key3;
    lastPeople = people; lastGate = gate;

    if (client->state() == QMqttClient::Connected) {
        publishSensorData();
    }
}

// ==================== 数据上报 ====================
void MainWindow::publishSensorData()
{
    QJsonObject data;
    data["deviceId"]    = "fsmp1a_board";
    data["temperature"] = temphum.get_temp();
    data["humidity"]    = temphum.get_hum();
    data["light"]       = light.get_light();
    data["fan"]         = fan.get_speed();
    data["pot1"] = HardwareController::readFileValue("/sys/bus/iio/devices/iio:device3/in_voltage0_raw");
    data["pot2"] = HardwareController::readFileValue("/sys/bus/iio/devices/iio:device3/in_voltage1_raw");
    data["key1"]   = (readKey1()   == 0) ? 1 : 0;
    data["key2"]   = (readKey2()   == 0) ? 1 : 0;
    data["key3"]   = (readKey3()   == 0) ? 1 : 0;
    data["people"] = (readPeople() == 0) ? 1 : 0;
    data["gate"]   = (readGate()   == 0) ? 1 : 0;

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

// ==================== 磁贴主页 ====================
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
    HardwareController::allLED(ledState ? 255 : 0);
    ui->textEdit->append(QString("【磁贴】灯光 %1").arg(ledState ? "开" : "关"));
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
    HardwareController::triggerAlarm();
    HardwareController::allLED(255);
    ui->textEdit->append("【磁贴】⚠️ 报警已触发");
}

void MainWindow::on_home_mode_btn_clicked()
{
    HardwareController::allLED(255);
    HardwareController::setUserLED(1, 255);
    fan.set_speed(150);
    fan.start();
    update_all();
    ui->textEdit->append("【场景】回家模式");
}

void MainWindow::on_movie_mode_btn_clicked()
{
    HardwareController::setLED(1, 30);
    HardwareController::setLED(2, 0);
    HardwareController::setLED(3, 0);
    HardwareController::setUserLED(1, 0);
    ui->textEdit->append("【场景】观影模式");
}

void MainWindow::on_away_mode_btn_clicked()
{
    HardwareController::allLED(0);
    HardwareController::setUserLED(1, 0);
    HardwareController::setUserLED(2, 0);
    fan.stop();
    update_all();
    ui->textEdit->append("【场景】离家模式");
}

void MainWindow::on_all_off_btn_clicked()
{
    HardwareController::allLED(0);
    HardwareController::setUserLED(1, 0);
    HardwareController::setUserLED(2, 0);
    HardwareController::stopAlarm();
    fan.stop();
    update_all();
    ui->textEdit->append("【场景】全部关闭");
}

void MainWindow::on_btn_switch_to_monitor_clicked()
{
    ui->stackedWidget->setCurrentIndex(1);
}

// ==================== 监控页 ====================
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
    ui->textEdit->append("【系统】已连接 MQTT 服务器");
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
    HardwareController::allLED(turnOn ? 255 : 0);
    ui->pushButton_2->setText(turnOn ? "关灯" : "开灯");
    ui->textEdit->append(turnOn ? "【发送】开灯" : "【发送】关灯");
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
    ui->textEdit->append("【系统】已订阅主题: " + ui->lineEdit_4->text());
}

void MainWindow::recv_data(const QByteArray &mess, const QMqttTopicName &topic)
{
    ui->textEdit->append(QString("【%1】 %2")
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
        if (id >= 1 && id <= 3) HardwareController::setLED(id, value);
        else if (id == 10) HardwareController::setUserLED(1, value);
        else if (id == 11) HardwareController::setUserLED(2, value);
    }
    else if (action == "all_led") {
        int value = cmd["value"].toInt(255);
        HardwareController::allLED(value);
    }
    else if (action == "alarm") {
        HardwareController::triggerAlarm();
        HardwareController::allLED(255);
    }
    else if (action == "stop_alarm") {
        HardwareController::stopAlarm();
        HardwareController::allLED(0);
    }
    else if (cmd.contains("lamp")) {
        bool lampState = cmd["lamp"].toBool();
        HardwareController::allLED(lampState ? 255 : 0);
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

// ==================== 子界面返回 ====================
void MainWindow::showMainWindow()
{
    this->show();
}

void MainWindow::onFanSpeedChanged(int speed)
{
    ui->lineEdit_fan->setText(QString::number(speed));
    ui->textEdit->append(QString("【风扇界面】速度调整为 %1").arg(speed));
}
