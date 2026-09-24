#ifndef FANDIALOG_H
#define FANDIALOG_H

#include <QDialog>
#include <QString>
#include "hardware_def.h"

#include "devicecontrol.h"


namespace Ui {
class fanDialog;
}

class fanDialog : public QDialog
{
    Q_OBJECT

public:
    explicit fanDialog(deviceControl *pDevCtrl,QWidget *parent = nullptr);
    ~fanDialog();



signals:
    void backToMain();

private slots:
    void on_backbtn_clicked();

    void on_fanopen_clicked();

    void on_commitbtn_clicked();

    void on_fanSlider_valueChanged(int value);

private:
    Ui::fanDialog *ui;
    deviceControl* m_devctrl = nullptr;
};

#endif // FANDIALOG_H
