#ifndef ONVIFCLIENT_H
#define ONVIFCLIENT_H

#include <QObject>
#include <QUrl>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QXmlStreamReader>
enum class OnvifAuthType{HttpDigest,Wsse,NoAuth};
enum class SOAPOperation{GetCapacities,NoOperation};
class OnvifClient : public QObject
{
    Q_OBJECT
public:
    explicit OnvifClient(QObject *parent = nullptr);
private:
    QUrl m_deviceUrl{};//设备服务地址
    QUrl m_mediaXAddr{};//媒体服务地址
    QUrl m_ptzXAddr{};//PTZ服务地址

    OnvifAuthType authType;

    QNetworkAccessManager *m_nam;
    QXmlStreamReader m_xr;
    // QRegularExpression m_rg;

    void postSoap(QUrl endPoint,const QByteArray& soapAction,const QByteArray& soapBody,OnvifAuthType type,SOAPOperation operation);
    void handleCapabilities(const QByteArray& xml);
signals:
public slots:
    void respondToMainURL(QUrl url);
};

#endif // ONVIFCLIENT_H
