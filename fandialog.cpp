#include "fandialog.h"
#include "ui_fandialog.h"



fanDialog::fanDialog(deviceControl *pDevCtrl,QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::fanDialog)
    , m_devctrl(pDevCtrl)
{
    ui->setupUi(this);

}

fanDialog::~fanDialog()
{
    delete ui;
}

void fanDialog::on_backbtn_clicked()
{
    emit fanDialog::backToMain();
    this->close();
}


void fanDialog::on_fanopen_clicked()
{

    if(ui->fanopen->text() == "打开风扇")
    {
        m_devctrl->setSensorVal(HW_FAN,150);
        m_devctrl->turnOnSensor(HW_FAN);
        ui->fanstate->setText("已打开");
        ui->fanopen->setText("关闭风扇");
    }
    else
    {
        m_devctrl->turnOffSensor(HW_FAN);
        ui->fanstate->setText("未打开");
        ui->fanopen->setText("打开风扇");
    }

}


void fanDialog::on_commitbtn_clicked()
{
    // 1.更新风扇状态
    // 2.更新风扇转速
    // 3.修改 PWM值

    int fan_speed = 0;
    if(ui->fanopen->text() == "打开风扇")
    {
        ui->fanstate->setText("未打开");
        ui->truespeed->text() = QString::number(fan_speed);
    }
    else
    {
        ui->fanstate->setText("已打开");

        // 设置风扇转速
        fan_speed = ui->fanSlider->value();
        m_devctrl->setSensorVal(HW_FAN,fan_speed);
        m_devctrl->turnOnSensor(HW_FAN);
        // 显示风扇转速
        m_devctrl->getSenserInfo(HW_FAN,fan_speed);
        ui->truespeed->text() = QString::number(fan_speed);
    }
}


void fanDialog::on_fanSlider_valueChanged(int value)
{
    ui->fanspeed->setText(QString::number(value));
}

