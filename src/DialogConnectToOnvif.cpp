#include "DialogConnectToOnvif.h"
#include "../ui/ui_DialogConnectToOnvif.h"
#include <QMessageBox>
DialogConnectToOnvif::DialogConnectToOnvif(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::DialogConnectToOnvif)
{
    ui->setupUi(this);
    connect(ui->btnCancel,&QPushButton::clicked,this,&QDialog::finished);
    connect(ui->btnAddDevice,&QPushButton::clicked,this,[this]{
        QString ip=ui->lineEditIPAddr->text();
        QUrl url;
        url.setHost(ip);
        url.setScheme(QStringLiteral("http"));
        url.setPath(QStringLiteral("/onvif/device_service"));
        if(!url.isValid())
        {
            QMessageBox::critical(this,"错误","设备URL格式不正确!");
            return;
        }
        emit validDeviceAddr(url);
        done(true);
    });
}

DialogConnectToOnvif::~DialogConnectToOnvif()
{
    delete ui;
}
