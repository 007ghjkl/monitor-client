#ifndef ONVIFCLIENT_H
#define ONVIFCLIENT_H

#include <QObject>
#include <QTimer>
#include <QUrl>
#include <QUuid>
#include <QNetworkAccessManager>
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QDomDocument>
#include <QXmlStreamWriter>
#include <QBuffer>
#include <QUdpSocket>
#include <QNetworkInterface>
#include <QNetworkDatagram>
enum class OnvifAuthType{HttpDigest,Wsse,NoAuth};
enum class SOAPOperation
{
    GetCapabilities,
    GetDeviceInfomation,
    GetProfiles,
    GetStreamUri,
    GetStatus,
    ContinuousMove,
    Stop,
    AbsoluteMove,
    NoOperation
};
class OnvifClient : public QObject
{
    Q_OBJECT
public:
    explicit OnvifClient(QObject *parent = nullptr);
    virtual ~OnvifClient();
private:
    QUrl m_deviceUrl{};//设备服务地址
    QUrl m_mediaXAddr{};//媒体服务地址
    QUrl m_ptzXAddr{};//PTZ服务地址
    QString m_userName{"admin"};
    QString m_password{"zyn050504"};
    struct DeviceInformation
    {
        QString manufacturer{};
        QString model{};
        QString firmwareVersion{};
        QString serialNumber{};
        QString hardwareID{};
    }m_deviceInfo;
    struct DeviceProfile
    {
        QString token{};
        QString name{};
        struct VideoEncoderConfiguration
        {
            QString token{};
            QString encoding{};
            int width{};
            int heiget{};
            qreal fpsLimit{};
            qreal bitrateLimit{};
        }videoEncoder;
        struct AudioEncoderConfiguration
        {
            QString token{};
            QString encoding{};
            int sampleRate{};
        }audioEncoder;
        struct PTZConfiguration
        {
            QString token{};
            struct DefaultPTZSpeed
            {
                qreal p{};//p
                qreal t{};//t
                qreal z{};//z
            }defalutSpeed;
            struct PTZLimit
            {
                int min{};
                int max{};
            }pLimit,tLimit,zLimit;
        }ptz;
    }m_profiles[8];
    struct PTZStatus
    {
        qreal p{};
        qreal t{};
        qreal z{};
        enum class MoveState{Moving,Idle,Unknown}ptState,zState;
    }m_ptzStatus;

    QUrl m_streamUri{};

    OnvifAuthType m_authType;

    QNetworkAccessManager *m_httpPoster=nullptr;
    QDomDocument m_soapXmlDoc;
    QXmlStreamWriter m_xmlWriter;
    QBuffer *m_httpBody=nullptr;

    QUuid m_discoverUuid{};
    QUdpSocket *m_discoverySocket=nullptr;
    QBuffer *m_udpBody=nullptr;
    QDomDocument m_discoveryDoc;

    void postSoap(SOAPOperation operation,OnvifAuthType authType,qreal vp=0,qreal vt=0,qreal p=0,qreal t=0,qreal z=0);
    void makeWsseHeader();
    void makeGetCapabilitiesBody();
    void makeGetDeviceInformation();
    void makeGetProfilesBody();
    void makeGetStreamUriBody();
    void makeGetStatusBody();
    void makeContinuousMoveBody(qreal vp,qreal vt);
    void makeStopBody();
    void makeAbsoluteMove(qreal vp,qreal vt,qreal p,qreal t,qreal z=0);
    void handleCapabilities(QNetworkReply* xml);
    void handleDeviceInformation(QNetworkReply* xml);
    void handleProfiles(QNetworkReply* xml);
    void handleStreamUri(QNetworkReply* xml);
    void handleStatus(QNetworkReply* xml);

    void discoverDevices();
    void makeDiscoveryProber();
private slots:
    void handleDiscovery();
signals:
public slots:
    void respondToMainURL(QUrl url);
};

#endif // ONVIFCLIENT_H
