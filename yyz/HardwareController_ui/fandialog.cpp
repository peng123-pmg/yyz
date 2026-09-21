#include "fandialog.h"
#include "ui_fandialog.h"

fanDialog::fanDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::fanDialog)
{
    ui->setupUi(this);
    setWindowTitle("风扇控制");

    ui->verticalSlider->setRange(0, 255);
    ui->verticalSlider->setValue(150);

    m_refreshTimer = new QTimer(this);
    connect(m_refreshTimer, &QTimer::timeout, this, &fanDialog::refreshUI);
}

fanDialog::~fanDialog()
{
    delete ui;
}

void fanDialog::setFan(Fan *fan)
{
    m_fan = fan;
    if (m_fan) {
        ui->verticalSlider->setValue(m_fan->get_speed());
    }
    refreshUI();
    m_refreshTimer->start(500);
}

void fanDialog::refreshUI()
{
    if (!m_fan) return;
    int speed = m_fan->get_speed();
    ui->fanspeed->setText(QString::number(speed));
    ui->fanstate->setText(speed > 0 ? "运行中" : "已停止");
    // 如果用户没在拖滑块，则同步滑块位置
    if (!ui->verticalSlider->isSliderDown()) {
        ui->verticalSlider->setValue(speed);
    }
}

void fanDialog::on_verticalSlider_valueChanged(int value)
{
    ui->fanspeed->setText(QString::number(value));
}

void fanDialog::on_commitbtn_clicked()
{
    if (!m_fan) return;

    int speed = ui->verticalSlider->value();
    m_fan->set_speed(speed);
    m_fan->start();

    emit fanSpeedChanged(speed);
    emit backToMain();
    this->close();
}

void fanDialog::on_backbtn_clicked()
{
    emit backToMain();
    this->close();
}
