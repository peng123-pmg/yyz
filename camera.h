#ifndef CAMERA_H
#define CAMERA_H

#include <QWidget>

namespace Ui {
class camera;
}

class camera : public QWidget
{
    Q_OBJECT

public:
    explicit camera(QWidget *parent = nullptr);
    ~camera();

signals:
    void backToMain();      // 返回主界面信号

private slots:
    void on_back_button_clicked();

private:
    Ui::camera *ui;
};

#endif // CAMERA_H
