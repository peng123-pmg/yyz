#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include "camera.h"
#include "fandialog.h"
#include "devicecontrol.h"
#include "hardware_def.h"
#include <QWidget>


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

    void top_bar_show();                       // 回家模式刷新

    void on_fan_pushbutton_clicked();

    void on_home_mode_btn_clicked();

    void on_all_off_btn_clicked();

private:
    Ui::MainWindow *ui;

    deviceControl dev_ctrl;          // 传感器操作对象
    QTimer m_timerblew;             // 定时器刷新

    camera *came_page = nullptr;

    fanDialog *fanbox = nullptr;
};
#endif // MAINWINDOW_H
