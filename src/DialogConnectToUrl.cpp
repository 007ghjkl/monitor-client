#include "DialogConnectToUrl.h"
#include "../ui/ui_DialogConnectToUrl.h"
#include <QMessageBox>
DialogConnectToUrl::DialogConnectToUrl(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::DialogConnectToUrl)
{
    ui->setupUi(this);
    connect(ui->btnCancel,&QPushButton::clicked,this,&QDialog::finished);
    connect(ui->btnConnect,&QPushButton::clicked,this,&DialogConnectToUrl::postForm);
}

DialogConnectToUrl::~DialogConnectToUrl()
{
    delete ui;
}

void DialogConnectToUrl::postForm()
{
    m_url.setScheme(ui->lineEditScheme->text());
    m_url.setHost(ui->lineEditHost->text());
    m_url.setPort(ui->spinBoxPort->value());
    m_url.setPath(ui->lineEditPath->text());
    m_url.setUserName(ui->lineEditUserName->text());
    m_url.setPassword(ui->lineEditPassword->text());

    if(m_url.isValid())
    {
        emit validUrl(m_url);
        done(true);
    }
    else
    {
        QMessageBox::critical(this,"错误","URL格式不正确!");
    }
}
