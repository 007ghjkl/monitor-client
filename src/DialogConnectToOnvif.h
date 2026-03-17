#ifndef DIALOGCONNECTTOONVIF_H
#define DIALOGCONNECTTOONVIF_H

#include <QDialog>

namespace Ui {
class DialogConnectToOnvif;
}

class DialogConnectToOnvif : public QDialog
{
    Q_OBJECT

public:
    explicit DialogConnectToOnvif(QWidget *parent = nullptr);
    ~DialogConnectToOnvif();

private:
    Ui::DialogConnectToOnvif *ui;
signals:
    void validDeviceAddr(QUrl url);
};

#endif // DIALOGCONNECTTOONVIF_H
