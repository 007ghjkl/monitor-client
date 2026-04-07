#ifndef DIALOGCONNECTTOURL_H
#define DIALOGCONNECTTOURL_H

#include <QDialog>
#include <QUrl>
namespace Ui {
class DialogConnectToUrl;
}

class DialogConnectToUrl : public QDialog
{
    Q_OBJECT

public:
    explicit DialogConnectToUrl(QWidget *parent = nullptr);
    ~DialogConnectToUrl();

private:
    Ui::DialogConnectToUrl *ui;
    QUrl m_url;
private slots:
    void postForm();
signals:
    void validUrl(QUrl url);
};

#endif // DIALOGCONNECTTOURL_H
