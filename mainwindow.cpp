#include "mainwindow.h"
#include "ui_mainwindow.h"


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(&m_timerblew,&QTimer::timeout,this,&MainWindow::top_bar_show);

    m_timerblew.start(1000);    // 1s触发一次刷新顶部进度条

}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_cam_clicked()
{
    if(came_page == nullptr)
    {
        came_page = new camera();
        connect(came_page,&camera::backToMain,this,&MainWindow::showMainWindow);
    }
    this->hide();
    came_page->resize(1366,768);
    came_page->showFullScreen();
}

void MainWindow::showMainWindow()
{
    this->showFullScreen();
}





void MainWindow::on_fan_pushbutton_clicked()
{
    if(fanbox == nullptr)
    {
        fanbox = new fanDialog(&dev_ctrl);
        connect(fanbox,&fanDialog::backToMain,this,&MainWindow::showMainWindow);
    }

    fanbox->showFullScreen();
}

void MainWindow::top_bar_show()
{
    double temp,hum = 0.0;
    int light = 0;
    int fanspeed = 0;
    // 获取风扇数据
    dev_ctrl.getSenserInfo(HW_FAN,fanspeed);
    if(fanspeed == 0)
    {
        ui->status_fan->setText("未打开");
    }
    else
    {
        ui->status_fan->setText(QString::number(fanspeed));
    }

    // 读取温湿度传感器
    dev_ctrl.getSenserInfo(HW_TEMP_HUB,temp,hum);
    ui->status_temp->setText(QString::number(temp, 'f',1));
    ui->status_hum->setText(QString::number(hum, 'f',1));
    // 读取光敏传感器
    dev_ctrl.getSenserInfo(HW_PhOTOSENS,light);
    ui->status_light->setText(QString::number(light));
}

/*
 * @breif:回家模式
    1.打开led灯
    2.读取光敏传感器，控制LED开关
    3.读取温湿度传感器控制风扇
    4.打开摄像头，开始人脸识别
    5.震动马达震动一下
    6.蜂鸣器短滴一声
*/
void MainWindow::on_home_mode_btn_clicked()
{
    static bool state = false;
    if(state)
    {
        // 打开风扇
        dev_ctrl.setSensorVal(HW_FAN,150);
        dev_ctrl.turnOnSensor(HW_FAN);

        // 打开LED
        dev_ctrl.turnOnSensor(HW_LEDS,1);
        dev_ctrl.turnOnSensor(HW_LEDS,2);
        dev_ctrl.turnOnSensor(HW_LEDS,3);
    }
    else
    {
        dev_ctrl.turnOffSensor(HW_FAN);

        // 打开LED
        dev_ctrl.turnOffSensor(HW_LEDS,1);
        dev_ctrl.turnOffSensor(HW_LEDS,2);
        dev_ctrl.turnOffSensor(HW_LEDS,3);
    }
    state = !state;

    // 按键反馈系统   一次
    dev_ctrl.turnOnSensor(HW_BEEP);
    dev_ctrl.turnOnSensor(HW_VIBRATE);
}

// 关闭所有传感器
void MainWindow::on_all_off_btn_clicked()
{
    // 风扇
    dev_ctrl.turnOffSensor(HW_FAN);

    // LED
    dev_ctrl.turnOffSensor(HW_LEDS,1);
    dev_ctrl.turnOffSensor(HW_LEDS,2);
    dev_ctrl.turnOffSensor(HW_LEDS,3);


    // 反馈
    dev_ctrl.turnOnSensor(HW_BEEP);
    dev_ctrl.turnOnSensor(HW_VIBRATE);
}

