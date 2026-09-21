#ifndef FANDIALOG_H
#define FANDIALOG_H

#include <QDialog>
#include <QTimer>
#include "Fan.h"

namespace Ui {
class fanDialog;
}

class fanDialog : public QDialog
{
    Q_OBJECT

public:
    explicit fanDialog(QWidget *parent = nullptr);
    ~fanDialog();

    // 由 MainWindow 注入同一个风扇实例，避免多实例冲突
    void setFan(Fan *fan);

signals:
    void backToMain();
    void fanSpeedChanged(int speed);

private slots:
    void on_backbtn_clicked();
    void on_commitbtn_clicked();
    void on_verticalSlider_valueChanged(int value);
    void refreshUI();

private:
    Ui::fanDialog *ui;
    Fan *m_fan = nullptr;
    QTimer *m_refreshTimer = nullptr;
};

#endif // FANDIALOG_H
