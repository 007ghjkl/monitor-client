#ifndef DIALOGCONNECTTOONVIF_H
#define DIALOGCONNECTTOONVIF_H

#include <QDialog>
#include <QUrl>
namespace Ui {
class DialogConnectToOnvif;
}

class DialogConnectToOnvif : public QDialog
{
    Q_OBJECT

public:
    explicit DialogConnectToOnvif(QWidget *parent = nullptr);
    ~DialogConnectToOnvif();
public slots:
    void showMatchDevice(QUrl url);
private:
    Ui::DialogConnectToOnvif *ui;
signals:
    void validDevice(QUrl url,QString userName,QString password);
};

#endif // DIALOGCONNECTTOONVIF_H
