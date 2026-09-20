#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include "camera.h"
#include "fandialog.h"
#include "hardware_def.h"



QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;



private slots:
    void on_cam_clicked();

    void showMainWindow();                          //展示主界面



    void on_fan_pushbutton_clicked();

private:
    Ui::MainWindow *ui;
    camera *came_page = nullptr;

    fanDialog *fanbox = nullptr;
};
#endif // MAINWINDOW_H
