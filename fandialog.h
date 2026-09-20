#ifndef FANDIALOG_H
#define FANDIALOG_H

#include <QDialog>

namespace Ui {
class fanDialog;
}

class fanDialog : public QDialog
{
    Q_OBJECT

public:
    explicit fanDialog(QWidget *parent = nullptr);
    ~fanDialog();

signals:
    void backToMain();

private slots:
    void on_backbtn_clicked();

private:
    Ui::fanDialog *ui;

};

#endif // FANDIALOG_H
