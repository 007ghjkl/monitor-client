#include "DialogConnectToOnvif.h"
#include "../../ui/ui_DialogConnectToOnvif.h"
#include <QMessageBox>
DialogConnectToOnvif::DialogConnectToOnvif(QWidget *parent)
    : QDialog(parent)
    , ui(new Ui::DialogConnectToOnvif)
{
    ui->setupUi(this);
    ui->listWidgetDiscovery->setSelectionMode(QAbstractItemView::SingleSelection);
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
        emit validDevice(url,ui->lineEditUserName->text(),ui->lineEditPassword->text());
        done(true);
    });
    connect(ui->btnConnect,&QPushButton::clicked,this,[this]{
        auto selected=ui->listWidgetDiscovery->selectedItems();
        if(selected.isEmpty())
        {
            QMessageBox::warning(this,"警告","没有选中设备!");
            return;
        }
        QUrl url=selected.at(0)->text();
        if(!url.isValid())
        {
            QMessageBox::critical(this,"错误","设备URL格式不正确!");
            return;
        }
        emit validDevice(url,ui->lineEditUserName->text(),ui->lineEditPassword->text());
        done(true);
    });
}

DialogConnectToOnvif::~DialogConnectToOnvif()
{
    delete ui;
}

void DialogConnectToOnvif::showMatchDevice(QUrl url)
{
    ui->listWidgetDiscovery->addItem(url.toString());
    ui->listWidgetDiscovery->setCurrentItem(ui->listWidgetDiscovery->item(0));
}
