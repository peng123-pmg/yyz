#include "mainwindow.h"
#include "ui_mainwindow.h"


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);

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
    came_page->show();
}

void MainWindow::showMainWindow()
{
    this->show();
}





void MainWindow::on_fan_pushbutton_clicked()
{
    if(fanbox == nullptr)
    {
        fanbox = new fanDialog();
        connect(fanbox,&fanDialog::backToMain,this,&MainWindow::showMainWindow);
    }

    fanbox->show();
}

