#include "camera.h"
#include "ui_camera.h"

camera::camera(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::camera)
{
    ui->setupUi(this);
    setWindowTitle("摄像头监控");
}

camera::~camera()
{
    delete ui;
}

void camera::on_back_button_clicked()
{
    emit backToMain();
    this->hide();
}
