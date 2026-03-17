#include "OnvifClient.h"
#include <QStack>
OnvifClient::OnvifClient(QObject *parent)
    : QObject{parent}
{
    m_nam=new QNetworkAccessManager(this);
}

void OnvifClient::postSoap(QUrl endPoint,const QByteArray& soapAction,const QByteArray& soapBody,OnvifAuthType type,SOAPOperation operation)
{
    QNetworkRequest httpRequest(endPoint);
    httpRequest.setHeader(QNetworkRequest::ContentTypeHeader,QStringLiteral("application/soap+xml; charset=utf-8"));
    httpRequest.setRawHeader("SOAPAction",soapAction);
    QByteArray httpBody=
        QByteArrayLiteral("<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" ")
        + QByteArrayLiteral("xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" ")
        + QByteArrayLiteral("xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\" ")
        + QByteArrayLiteral("xmlns:tr2=\"http://www.onvif.org/ver20/media/wsdl\" ")
        + QByteArrayLiteral("xmlns:tptz=\"http://www.onvif.org/ver20/ptz/wsdl\" ")
        + QByteArrayLiteral("xmlns:tt=\"http://www.onvif.org/ver10/schema\" ")
        + QByteArrayLiteral("xmlns:wsse=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd\" ")
        + QByteArrayLiteral("xmlns:wsu=\"http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd\">")
        + QByteArrayLiteral("<s:Body>")
        + soapBody
        + QByteArrayLiteral("</s:Body></s:Envelope>");

    if(type==OnvifAuthType::HttpDigest)
    {
        ;
    }

    auto reply=m_nam->post(httpRequest,httpBody);
    connect(reply,&QNetworkReply::finished,this,[this,reply,operation]{
        int httpCode=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if(httpCode==401)
        {//鉴权失败
            qDebug()<<"401";
            reply->deleteLater();
        }
        auto xml=reply->readAll();
        if(operation==SOAPOperation::GetCapacities)
        {
            handleCapabilities(xml);
        }
        reply->deleteLater();
    });
}

void OnvifClient::handleCapabilities(const QByteArray &xml)
{
    m_xr.clear();
    m_xr.addData(xml);
    int depth=0;
    int depthMedia=-1,depthPTZ=-1;
    QString name;
    enum class ServiceType{Media,PTZ,NoService} currentService=ServiceType::NoService;
    while(!m_xr.atEnd())
    {
        m_xr.readNext();
        if(m_xr.isStartElement())
        {
            ++depth;
            name=m_xr.name().toString();
            if(name.compare(QStringLiteral("Media"), Qt::CaseInsensitive) == 0)
            {
                depthMedia=depth;
                currentService=ServiceType::Media;
            }
            if(name.compare(QStringLiteral("PTZ"), Qt::CaseInsensitive) == 0)
            {
                depthPTZ=depth;
                currentService=ServiceType::PTZ;
            }
            if(currentService==ServiceType::Media&&depth==depthMedia+1&&name.compare(QStringLiteral("XAddr"), Qt::CaseInsensitive) == 0)
            {
                m_mediaXAddr=m_xr.readElementText();
            }
            if(currentService==ServiceType::PTZ&&depth==depthPTZ+1&&name.compare(QStringLiteral("XAddr"), Qt::CaseInsensitive) == 0)
            {
                m_ptzXAddr=m_xr.readElementText();
            }
        }
        if(m_xr.isEndElement())
        {
            --depth;
            if(currentService==ServiceType::Media&&depth==depthMedia-1)
            {
                currentService=ServiceType::NoService;
            }
            if(currentService==ServiceType::PTZ&&depth==depthPTZ-1)
            {
                currentService=ServiceType::NoService;
            }
        }
    }
    qDebug()<<"媒体服务地址:"<<m_mediaXAddr;
    qDebug()<<"PTZ服务地址:"<<m_ptzXAddr;
}

void OnvifClient::respondToMainURL(QUrl url)
{
    m_deviceUrl=url;
    qDebug()<<"设备服务地址:"<<m_deviceUrl;
    authType=OnvifAuthType::NoAuth;
    postSoap(m_deviceUrl
             ,QByteArrayLiteral("http://www.onvif.org/ver10/device/wsdl/GetCapabilities")
             ,QByteArrayLiteral("<tds:GetCapabilities><tds:Category>All</tds:Category></tds:GetCapabilities>")
             ,authType
             ,SOAPOperation::GetCapacities);
}