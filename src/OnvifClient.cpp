#include "OnvifClient.h"
namespace xmlns{
QByteArray soap=QByteArrayLiteral("http://www.w3.org/2003/05/soap-envelope");
QByteArray wsa=QByteArrayLiteral("http://schemas.xmlsoap.org/ws/2004/08/addressing");
QByteArray wsdd=QByteArrayLiteral("http://schemas.xmlsoap.org/ws/2005/04/discovery");
QByteArray wsse=QByteArrayLiteral("http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-secext-1.0.xsd");
QByteArray wsu=QByteArrayLiteral("http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-wssecurity-utility-1.0.xsd");
QByteArray tds=QByteArrayLiteral("http://www.onvif.org/ver10/device/wsdl");
QByteArray trt=QByteArrayLiteral("http://www.onvif.org/ver10/media/wsdl");
QByteArray ttpz=QByteArrayLiteral("http://www.onvif.org/ver20/ptz/wsdl");
QByteArray tdn=QByteArrayLiteral("http://www.onvif.org/ver10/network/wsdl");
}
namespace WSDiscovery{
QHostAddress multicastAddr("239.255.255.250");
quint16 multicastPort=3702;
}
OnvifClient::OnvifClient(QObject *parent)
    : QObject{parent}
{
    m_discoverySocket=new QUdpSocket(this);
    m_udpBody=new QBuffer(this);
    m_udpBody->open(QIODeviceBase::ReadWrite);
    connect(m_discoverySocket,&QUdpSocket::readyRead,this,&OnvifClient::handleDiscovery);
    m_httpPoster=new QNetworkAccessManager(this);
    m_httpBody=new QBuffer(this);
    m_httpBody->open(QIODeviceBase::ReadWrite);
}

OnvifClient::~OnvifClient()
{
    m_discoverySocket->close();
    m_udpBody->close();
    m_httpBody->close();
}

void OnvifClient::postSoap(SOAPOperation operation,OnvifAuthType authType,qreal vp,qreal vt,qreal p,qreal t,qreal z)
{
    QByteArray soapAction{};
    QUrl url{};
    m_httpBody->buffer().clear();
    m_httpBody->seek(0);
    m_xmlWriter.setDevice(m_httpBody);
    m_xmlWriter.writeStartDocument();
    //Envelope
    m_xmlWriter.writeNamespace(xmlns::soap,"soap");
    m_xmlWriter.writeNamespace(xmlns::wsse,"wsse");
    m_xmlWriter.writeNamespace(xmlns::tds,"tds");
    m_xmlWriter.writeNamespace(xmlns::trt,"trt");
    m_xmlWriter.writeNamespace(xmlns::ttpz,"ttpz");
    m_xmlWriter.writeStartElement(xmlns::soap,"Envelope");
    //Header,鉴权
    switch(authType)
    {
        case OnvifAuthType::HttpDigest:
        {
            m_xmlWriter.writeEmptyElement(xmlns::soap,"Header");
            break;
        }
        case OnvifAuthType::Wsse:
        {
            makeWsseHeader();
            break;
        }
        case OnvifAuthType::NoAuth:
        {
            m_xmlWriter.writeEmptyElement(xmlns::soap,"Header");
            break;
        }
        default:
            return;
    }
    //Body,操作
    switch(operation)
    {
        case SOAPOperation::GetCapabilities:
        {
            soapAction=QByteArrayLiteral("http://www.onvif.org/ver10/device/wsdl/GetCapabilities");
            url=m_deviceUrl;
            makeGetCapabilitiesBody();
            break;
        }
        case SOAPOperation::GetDeviceInfomation:
        {
            soapAction=QByteArrayLiteral("http://www.onvif.org/ver10/device/wsdl/GetDeviceInformation");
            url=m_deviceUrl;
            makeGetDeviceInformation();
            break;
        }
        case SOAPOperation::GetProfiles:
        {
            soapAction=QByteArrayLiteral("http://www.onvif.org/ver10/media/wsdl/GetProfiles");
            url=m_mediaXAddr;
            makeGetProfilesBody();
            break;
        }
        case SOAPOperation::GetStreamUri:
        {
            soapAction=QByteArrayLiteral("http://www.onvif.org/ver10/media/wsdl/GetStreamUri");
            url=m_mediaXAddr;
            makeGetStreamUriBody();
            break;
        }
        case SOAPOperation::GetStatus:
        {
            soapAction=QByteArrayLiteral("http://www.onvif.org/ver20/ptz/wsdl/GetStatus");
            url=m_ptzXAddr;
            makeGetStatusBody();
            break;
        }
        case SOAPOperation::ContinuousMove:
        {
            soapAction=QByteArrayLiteral("http://www.onvif.org/ver20/ptz/wsdl/ContinuousMove");
            url=m_ptzXAddr;
            makeContinuousMoveBody(vp,vt);
            break;
        }
        case SOAPOperation::Stop:
        {
            soapAction=QByteArrayLiteral("http://www.onvif.org/ver20/ptz/wsdl/Stop");
            url=m_ptzXAddr;
            makeStopBody();
            break;
        }
        case SOAPOperation::AbsoluteMove:
        {
            soapAction=QByteArrayLiteral("http://www.onvif.org/ver20/ptz/wsdl/AbsoluteMove");
            url=m_ptzXAddr;
            makeAbsoluteMove(vp,vt,p,t,z);
            break;
        }
        case SOAPOperation::NoOperation:
        {
            return;
        }
        default:
            return;
    }
    m_xmlWriter.writeEndElement();//soap:Envelope
    m_xmlWriter.writeEndDocument();
    //添加HTTP头
    QNetworkRequest httpRequest(m_deviceUrl);
    httpRequest.setHeader(QNetworkRequest::ContentTypeHeader,QStringLiteral("application/soap+xml; charset=utf-8"));
    httpRequest.setRawHeader("SOAPAction",soapAction);
    //发送请求
    auto reply=m_httpPoster->post(httpRequest,m_httpBody->data());
    connect(reply,&QNetworkReply::finished,this,[this,reply,operation]{
        int httpCode=reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        if(httpCode==401)
        {//鉴权失败
            qDebug()<<"401";
            reply->deleteLater();
            return;
        }
        switch(operation)
        {
            case SOAPOperation::GetCapabilities:
            {
                handleCapabilities(reply);
                break;
            }
            case SOAPOperation::GetDeviceInfomation:
            {
                handleDeviceInformation(reply);
                break;
            }
            case SOAPOperation::GetProfiles:
            {
                handleProfiles(reply);
                break;
            }
            case SOAPOperation::GetStreamUri:
            {
                handleStreamUri(reply);
                break;
            }
            case SOAPOperation::GetStatus:
            {
                handleStatus(reply);
                break;
            }
            default:
                break;
        }
        reply->deleteLater();
    });
}

void OnvifClient::makeWsseHeader()
{
    const QByteArray nonceRaw = QUuid::createUuid().toByteArray(QUuid::WithoutBraces);
    const QByteArray nonceB64 = nonceRaw.toBase64();
    const QString created = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
    const QByteArray digest =
        QCryptographicHash::hash(nonceRaw + created.toUtf8() + m_password.toUtf8(), QCryptographicHash::Sha1)
            .toBase64();
    m_xmlWriter.writeStartElement(xmlns::soap,"Header");
    m_xmlWriter.writeStartElement(xmlns::wsse,"Scurity");
    m_xmlWriter.writeAttribute("mustUnderStand","1");
    m_xmlWriter.writeStartElement(xmlns::wsse,"UserNameToken");
    m_xmlWriter.writeTextElement(xmlns::wsse,"UserName",m_userName);//wsse:UserName
    m_xmlWriter.writeStartElement(xmlns::wsse,"Password");
    m_xmlWriter.writeAttribute("Type"
                               ,QByteArrayLiteral("http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-username-token-profile-1.0#PasswordDigest"));
    m_xmlWriter.writeCharacters(digest);
    m_xmlWriter.writeEndElement();//wsse:Password
    m_xmlWriter.writeStartElement(xmlns::wsse,"Nonce");
    m_xmlWriter.writeAttribute("EncodingType"
                               ,QByteArrayLiteral("http://docs.oasis-open.org/wss/2004/01/oasis-200401-wss-soap-message-security-1.0#Base64Binary"));
    m_xmlWriter.writeCharacters(nonceB64);
    m_xmlWriter.writeEndElement();//wsse:Nonce
    m_xmlWriter.writeTextElement(xmlns::wsu,"Created",created);//wsu:Created
    m_xmlWriter.writeEndElement();//wsse:UserNameToken
    m_xmlWriter.writeEndElement();//wsse:Security
    m_xmlWriter.writeEndElement();//soap:Header
}

void OnvifClient::makeGetCapabilitiesBody()
{
    m_xmlWriter.writeStartElement(xmlns::soap,"Body");
    m_xmlWriter.writeStartElement(xmlns::tds,"GetCapabilities");
    m_xmlWriter.writeTextElement("Category","All");//Category
    m_xmlWriter.writeEndElement();//tds:GetCapacities
    m_xmlWriter.writeEndElement();//soap:Body
}

void OnvifClient::makeGetDeviceInformation()
{
    m_xmlWriter.writeStartElement(xmlns::soap,"Body");
    m_xmlWriter.writeEmptyElement(xmlns::tds,"GetDeviceInformation");//tds:GetDeviceInformation
    m_xmlWriter.writeEndElement();//soap:Body
}

void OnvifClient::makeGetProfilesBody()
{
    m_xmlWriter.writeStartElement(xmlns::soap,"Body");
    m_xmlWriter.writeEmptyElement(xmlns::trt,"GetProfiles");//trt:GetProfiles
    m_xmlWriter.writeEndElement();//soap:Body
}

void OnvifClient::makeGetStreamUriBody()
{
    // m_profiles[0].token="profile_1";
    m_xmlWriter.writeStartElement(xmlns::soap,"Body");
    m_xmlWriter.writeStartElement(xmlns::trt,"GetStreamUri");
    m_xmlWriter.writeStartElement("StreamSetup");
    m_xmlWriter.writeTextElement("Stream","RTP-Unicast");//Stream
    m_xmlWriter.writeStartElement("Transport");
    m_xmlWriter.writeTextElement("Protocol","RTSP");//Protocol
    m_xmlWriter.writeEndElement();//Transport
    m_xmlWriter.writeEndElement();//StreamSetup
    m_xmlWriter.writeTextElement("ProfileToken",m_profiles[0].token);//ProfileToken
    m_xmlWriter.writeEndElement();//trt:GetStreamUri
    m_xmlWriter.writeEndElement();//soap:Body
}

void OnvifClient::makeGetStatusBody()
{
    // m_profiles[0].token="profile_1";
    m_xmlWriter.writeStartElement(xmlns::soap,"Body");
    m_xmlWriter.writeStartElement(xmlns::ttpz,"GetStatus");
    m_xmlWriter.writeTextElement("ProfileToken",m_profiles[0].token);//ProfileToken
    m_xmlWriter.writeEndElement();//ttpz:GetStatus
    m_xmlWriter.writeEndElement();//soap:Body
}

void OnvifClient::makeContinuousMoveBody(qreal vp,qreal vt)
{
    // m_profiles[0].token="profile_1";
    m_xmlWriter.writeStartElement(xmlns::soap,"Body");
    m_xmlWriter.writeStartElement(xmlns::ttpz,"ContinuousMove");
    m_xmlWriter.writeTextElement("ProfileToken",m_profiles[0].token);//ProfileToken
    m_xmlWriter.writeStartElement("Velocity");
    m_xmlWriter.writeEmptyElement("PanTilt");
    m_xmlWriter.writeAttribute("x",QString::number(vp));
    m_xmlWriter.writeAttribute("y",QString::number(vt));//Pantilt
    m_xmlWriter.writeEndElement();//Velocity
    m_xmlWriter.writeEndElement();//ttpz:ContinuousMove
    m_xmlWriter.writeEndElement();//soap:Body
}

void OnvifClient::makeStopBody()
{
    // m_profiles[0].token="profile_1";
    m_xmlWriter.writeStartElement(xmlns::soap,"Body");
    m_xmlWriter.writeStartElement(xmlns::ttpz,"Stop");
    m_xmlWriter.writeTextElement("ProfileToken",m_profiles[0].token);//ProfileToken
    m_xmlWriter.writeTextElement("PanTilt","true");//PanTilt
    m_xmlWriter.writeTextElement("Zoom","false");//Zoom
    m_xmlWriter.writeEndElement();//ttpz:Stop
    m_xmlWriter.writeEndElement();//soap:Body
}

void OnvifClient::makeAbsoluteMove(qreal vp, qreal vt, qreal p, qreal t, qreal z)
{
    // m_profiles[0].token="profile_1";
    m_xmlWriter.writeStartElement(xmlns::soap,"Body");
    m_xmlWriter.writeStartElement(xmlns::ttpz,"AbsoluteMove");
    m_xmlWriter.writeTextElement("ProfileToken",m_profiles[0].token);//ProfileToken
    m_xmlWriter.writeStartElement("Position");
    m_xmlWriter.writeEmptyElement("PanTilt");
    m_xmlWriter.writeAttribute("x",QString::number(p));
    m_xmlWriter.writeAttribute("y",QString::number(t));//Pantilt
    m_xmlWriter.writeEmptyElement("Zoom");
    m_xmlWriter.writeAttribute("x",QString::number(z));//Zoom
    m_xmlWriter.writeEndElement();//Position
    m_xmlWriter.writeStartElement("Speed");
    m_xmlWriter.writeEmptyElement("PanTilt");
    m_xmlWriter.writeAttribute("x",QString::number(vp));
    m_xmlWriter.writeAttribute("y",QString::number(vt));//Pantilt
    m_xmlWriter.writeEndElement();//Speed
    m_xmlWriter.writeEndElement();//ttpz:AbsoluteMove
    m_xmlWriter.writeEndElement();//soap:Body
}

void OnvifClient::handleCapabilities(QNetworkReply* xml)
{
    m_soapXmlDoc.setContent(xml,QDomDocument::ParseOption::UseNamespaceProcessing);
    auto capabilities=m_soapXmlDoc.elementsByTagName("Capabilities").at(0);
    // qDebug()<<capabilities.toElement().tagName();
    auto device=capabilities.firstChildElement("Device");
    m_deviceUrl=device.firstChildElement("XAddr").text();
    qDebug()<<"设备服务地址:"<<m_deviceUrl;
    auto media=capabilities.firstChildElement("Media");
    m_mediaXAddr=media.firstChildElement("XAddr").text();
    qDebug()<<"媒体服务地址:"<<m_mediaXAddr;
    auto ptz=capabilities.firstChildElement("PTZ");
    m_ptzXAddr=ptz.firstChildElement("XAddr").text();
    qDebug()<<"PTZ服务地址:"<<m_ptzXAddr;
}

void OnvifClient::handleDeviceInformation(QNetworkReply *xml)
{
    m_soapXmlDoc.setContent(xml,QDomDocument::ParseOption::UseNamespaceProcessing);
    auto info=m_soapXmlDoc.elementsByTagName("GetDeviceInformationResponse").at(0);
    m_deviceInfo.manufacturer=info.firstChildElement("Manufacturer").text();
    m_deviceInfo.model=info.firstChildElement("Model").text();
    m_deviceInfo.firmwareVersion=info.firstChildElement("FirmwareVersion").text();
    m_deviceInfo.serialNumber=info.firstChildElement("SerialNumber").text();
    m_deviceInfo.hardwareID=info.firstChildElement("HardwareId").text();
}

void OnvifClient::handleProfiles(QNetworkReply* xml)
{
    m_soapXmlDoc.setContent(xml,QDomDocument::ParseOption::UseNamespaceProcessing);
    auto profilesList=m_soapXmlDoc.elementsByTagName("Profiles");
    for(int i=0;i<profilesList.length();++i)
    {
        auto profile=profilesList.at(i).toElement();
        m_profiles[i].token=profile.attribute("token");
        m_profiles[i].name=profile.firstChildElement("Name").text();
        auto videoEncoder=profile.firstChildElement("VideoEncoderConfiguration");
        m_profiles[i].videoEncoder.token=videoEncoder.attribute("token");
        m_profiles[i].videoEncoder.encoding=videoEncoder.firstChildElement("Encoding").text();
        auto resolution=videoEncoder.firstChildElement("Resolution");
        m_profiles[i].videoEncoder.width=resolution.firstChildElement("Width").text().toInt();
        m_profiles[i].videoEncoder.heiget=resolution.firstChildElement("Height").text().toInt();
        auto rateControl=videoEncoder.firstChildElement("RateControl");
        m_profiles[i].videoEncoder.fpsLimit=rateControl.firstChildElement("FrameRateLimit").text().toDouble();
        m_profiles[i].videoEncoder.bitrateLimit=rateControl.firstChildElement("BitRateLimit").text().toDouble();
        auto audioEncoder=profile.firstChildElement("AudioEncoderConfiguration");
        m_profiles[i].audioEncoder.token=audioEncoder.attribute("token");
        m_profiles[i].audioEncoder.encoding=audioEncoder.firstChildElement("Encoding").text();
        m_profiles[i].audioEncoder.sampleRate=audioEncoder.firstChildElement("SampleRate").text().toInt();
        auto ptz=profile.firstChildElement("PTZConfiguration");
        m_profiles[i].ptz.token=ptz.attribute("token");
        auto defaultSpeed=ptz.firstChildElement("DefaultPTZSpeed");
        auto pt=defaultSpeed.firstChildElement("PanTilt");
        m_profiles[i].ptz.defalutSpeed.p=pt.attribute("x").toDouble();
        m_profiles[i].ptz.defalutSpeed.t=pt.attribute("y").toDouble();
        m_profiles[i].ptz.defalutSpeed.z=defaultSpeed.firstChildElement("Zoom").attribute("x").toDouble();
        pt=ptz.firstChildElement("PanTiltLimits").firstChildElement("Range");
        m_profiles[i].ptz.pLimit.min=pt.firstChildElement("XRange").firstChildElement("Min").text().toInt();
        m_profiles[i].ptz.pLimit.max=pt.firstChildElement("XRange").firstChildElement("Max").text().toInt();
        m_profiles[i].ptz.tLimit.min=pt.firstChildElement("YRange").firstChildElement("Min").text().toInt();
        m_profiles[i].ptz.tLimit.max=pt.firstChildElement("YRange").firstChildElement("Max").text().toInt();
        auto z=ptz.firstChildElement("ZoomLimits").firstChildElement("Range").firstChildElement("XRange");
        m_profiles[i].ptz.zLimit.min=z.firstChildElement("Min").text().toInt();
        m_profiles[i].ptz.zLimit.max=z.firstChildElement("Max").text().toInt();
    }
    qDebug()<<"填充完成!";
}

void OnvifClient::handleStreamUri(QNetworkReply *xml)
{
    m_soapXmlDoc.setContent(xml,QDomDocument::ParseOption::UseNamespaceProcessing);
    m_streamUri=m_soapXmlDoc.elementsByTagName("Uri").at(0).toElement().text();
    qDebug()<<"StreamUri:"<<m_streamUri;
}

void OnvifClient::handleStatus(QNetworkReply *xml)
{
    m_soapXmlDoc.setContent(xml,QDomDocument::ParseOption::UseNamespaceProcessing);
    auto ptz=m_soapXmlDoc.elementsByTagName("PTZStatus").at(0).toElement();
    auto pt=ptz.firstChildElement("Position").firstChildElement("PanTilt");
    m_ptzStatus.p=pt.attribute("x").toDouble();
    m_ptzStatus.t=pt.attribute("y").toDouble();
    m_ptzStatus.z=ptz.firstChildElement("Position").firstChildElement("Zoom").attribute("x").toDouble();
    auto ptState=ptz.firstChildElement("MoveStatus").firstChildElement("PanTilt").text();
    if(ptState.compare("Moving",Qt::CaseInsensitive)==0)
        m_ptzStatus.ptState=PTZStatus::MoveState::Moving;
    else if(ptState.compare("Idle",Qt::CaseInsensitive)==0)
        m_ptzStatus.ptState=PTZStatus::MoveState::Idle;
    else
        m_ptzStatus.ptState=PTZStatus::MoveState::Unknown;
    auto zState=ptz.firstChildElement("MoveStatus").firstChildElement("Zoom").text();
    if(zState.compare("Moving",Qt::CaseInsensitive)==0)
        m_ptzStatus.zState=PTZStatus::MoveState::Moving;
    else if(zState.compare("Idle",Qt::CaseInsensitive)==0)
        m_ptzStatus.zState=PTZStatus::MoveState::Idle;
    else
        m_ptzStatus.zState=PTZStatus::MoveState::Unknown;
}

void OnvifClient::discoverDevices()
{
    m_discoverySocket->close();
    //绑定0.0.0.0:3702
    m_discoverySocket->bind(QHostAddress::AnyIPv4,WSDiscovery::multicastPort,QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
    //选择正确接口
    auto ifaces = QNetworkInterface::allInterfaces();
    QList<QNetworkInterface> validInterfaces{};
    for(auto &iface:ifaces)
    {
        const auto flags = iface.flags();
        if (!(flags & QNetworkInterface::IsUp) || !(flags & QNetworkInterface::IsRunning) ||
            (flags & QNetworkInterface::IsLoopBack) || !(flags & QNetworkInterface::CanMulticast)) {
            continue;
        }
        validInterfaces.push_back(iface);
    }
    qDebug()<<"参与组播的接口数:"<<validInterfaces.length();
    //探测报文
    makeDiscoveryProber();
    for(auto &iface:validInterfaces)
    {
        m_discoverySocket->setMulticastInterface(iface);
        m_discoverySocket->writeDatagram(m_udpBody->buffer(),WSDiscovery::multicastAddr,WSDiscovery::multicastPort);
    }
}

void OnvifClient::makeDiscoveryProber()
{
    m_discoverUuid=QUuid::createUuid();
    m_udpBody->buffer().clear();
    m_udpBody->seek(0);
    m_xmlWriter.setDevice(m_udpBody);
    m_xmlWriter.writeStartDocument();
    //Envelope
    m_xmlWriter.writeNamespace(xmlns::soap,"soap");
    m_xmlWriter.writeNamespace(xmlns::wsa,"wsa");
    m_xmlWriter.writeNamespace(xmlns::wsdd,"wsdd");
    m_xmlWriter.writeNamespace(xmlns::tdn,"tdn");
    m_xmlWriter.writeStartElement(xmlns::soap,"Envelope");
    //Header
    m_xmlWriter.writeStartElement(xmlns::soap,"Header");
    m_xmlWriter.writeStartElement(xmlns::wsa,"Action");
    m_xmlWriter.writeAttribute(xmlns::soap,"mustUnderstand","1");
    m_xmlWriter.writeCharacters(QByteArrayLiteral("http://schemas.xmlsoap.org/ws/2005/04/discovery/Probe"));
    m_xmlWriter.writeEndElement();//wsa:Action
    m_xmlWriter.writeTextElement(xmlns::wsa,"MessageID","uuid:"+m_discoverUuid.toByteArray(QUuid::WithoutBraces));//wsa::MessageID
    m_xmlWriter.writeStartElement(xmlns::wsa,"ReplyTo");
    m_xmlWriter.writeTextElement(xmlns::wsa,"Address",QByteArrayLiteral("http://schemas.xmlsoap.org/ws/2004/08/addressing/role/anonymous"));//wsa:Address
    m_xmlWriter.writeEndElement();//wsa:ReplyTo
    m_xmlWriter.writeStartElement(xmlns::wsa,"To");
    m_xmlWriter.writeAttribute(xmlns::soap,"mustUnderstand","1");
    m_xmlWriter.writeCharacters(QByteArrayLiteral("urn:schemas-xmlsoap-org:ws:2005:04:discovery"));
    m_xmlWriter.writeEndElement();//wsa:To
    m_xmlWriter.writeEndElement();//soap:Header
    //Body
    m_xmlWriter.writeStartElement(xmlns::soap,"Body");
    m_xmlWriter.writeStartElement(xmlns::wsdd,"Probe");
    m_xmlWriter.writeTextElement(xmlns::wsdd,"Type",QByteArrayLiteral("tdn:NetworkVideoTransmitter"));//wssd:Type
    m_xmlWriter.writeEndElement();//wsdd:Probe
    m_xmlWriter.writeEndElement();//soap:Body
    m_xmlWriter.writeEndElement();//soap:Envelope
    m_xmlWriter.writeEndDocument();
}

void OnvifClient::handleDiscovery()
{
    while(m_discoverySocket->hasPendingDatagrams())
    {
        auto dataGram=m_discoverySocket->receiveDatagram();
        if(!dataGram.isValid()||dataGram.isNull())
        {
            continue;
        }
        m_discoveryDoc.setContent(dataGram.data(),QDomDocument::ParseOption::UseNamespaceProcessing);
        auto uuidElement=m_discoveryDoc.elementsByTagName("MessageID").at(0).toElement();
        QString uuid=uuidElement.text().last(36);
        if(uuid.compare(m_discoverUuid.toString(QUuid::WithoutBraces),Qt::CaseInsensitive)!=0)
        {
            continue;
        }
        QUrl xaddr=m_discoveryDoc.elementsByTagNameNS(xmlns::wsdd,"XAddrs").at(0).toElement().text();
        qDebug()<<"找到"<<xaddr.toString();
        // qDebug()<<dataGram.data()<<"\n";
    }
}

void OnvifClient::respondToMainURL(QUrl url)
{
    m_deviceUrl=url;
    qDebug()<<"设备服务地址:"<<m_deviceUrl;
    m_authType=OnvifAuthType::Wsse;
    // auto timer1=new QTimer(this);
    // auto timer2=new QTimer(this);
    // auto absMove=[this,timer2]{
    //     postSoap(SOAPOperation::AbsoluteMove,m_authType,1,1,0,0,0);
    //     timer2->deleteLater();
    // };
    // auto stop=[this,timer1,timer2,absMove]{
    //     postSoap(SOAPOperation::Stop,m_authType);
    //     connect(timer2,&QTimer::timeout,this,absMove);
    //     timer2->start(2000);
    //     timer1->deleteLater();
    // };
    // connect(timer1,&QTimer::timeout,this,stop);
    // postSoap(SOAPOperation::ContinuousMove,m_authType,0.5,0);
    // timer1->start(2000);

    // postSoap(SOAPOperation::GetCapabilities,m_authType);
    // postSoap(SOAPOperation::GetDeviceInfomation,m_authType);
    // postSoap(SOAPOperation::GetProfiles,m_authType);
    // postSoap(SOAPOperation::GetStreamUri,m_authType);
    // postSoap(SOAPOperation::GetStatus,m_authType);

    discoverDevices();
}