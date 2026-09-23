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
        // 阻塞信号，防止 setValue 触发 valueChanged 造成误写
        ui->verticalSlider->blockSignals(true);
        ui->verticalSlider->setValue(m_fan->get_speed());
        ui->verticalSlider->blockSignals(false);
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

    // 只在滑块没有被拖动时同步位置
    if (!ui->verticalSlider->isSliderDown()) {
        ui->verticalSlider->blockSignals(true);
        ui->verticalSlider->setValue(speed);
        ui->verticalSlider->blockSignals(false);
    }
}

void fanDialog::on_verticalSlider_valueChanged(int value)
{
    ui->fanspeed->setText(QString::number(value));

    if (m_fan) {
        m_fan->set_speed(value);
        m_fan->start();
        emit fanSpeedChanged(value);
    }
}

void fanDialog::on_commitbtn_clicked()
{
    // 拖动时已经实时改了风扇，这里只需返回
    emit backToMain();
    this->close();
}

void fanDialog::on_backbtn_clicked()
{
    emit backToMain();
    this->close();
}
