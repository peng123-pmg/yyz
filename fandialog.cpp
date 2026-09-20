#include "fandialog.h"
#include "ui_fandialog.h"

fanDialog::fanDialog(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::fanDialog)
{
    ui->setupUi(this);
}

fanDialog::~fanDialog()
{
    delete ui;
}

void fanDialog::on_backbtn_clicked()
{
    emit fanDialog::backToMain();
    this->close();
}

