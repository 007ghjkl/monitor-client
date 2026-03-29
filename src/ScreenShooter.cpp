#include "ScreenShooter.h"
#include <QCoreApplication>
ScreenShooter::ScreenShooter(QObject *parent)
    : QObject{parent}
{
    m_pkt=av_packet_alloc();
    m_frame=av_frame_alloc();
    if(!m_pkt||!m_frame)
    {
        qDebug()<<"截屏器:分配AVPacket或AVFrame时出错!";
        QCoreApplication::exit(-1);
    }
    m_picFile=new QFile(this);

    m_isReady=false;
}

ScreenShooter::~ScreenShooter()
{
    destroy();
}

void ScreenShooter::destroy()
{
    m_isReady=false;
    avcodec_free_context(&m_enCtx);
    av_packet_free(&m_pkt);
    av_frame_free(&m_frame);
}

void ScreenShooter::respondToProducer(int w, int h)
{
    m_width=w;
    m_height=h;
    //一帧截屏
    auto encoder=avcodec_find_encoder(AV_CODEC_ID_JPEG2000);
    if(!encoder)
    {
        qDebug()<<"找不到JPEG2000编码器!";
        QCoreApplication::exit(-1);
    }
    m_enCtx=avcodec_alloc_context3(encoder);
    if(!m_enCtx)
    {
        qDebug()<<"给截屏编码器分配上下文空间时出错!";
        QCoreApplication::exit(-1);
    }
    m_enCtx->width=m_width;
    m_enCtx->height=m_height;
    m_enCtx->pix_fmt=AV_PIX_FMT_YUV420P;
    m_enCtx->time_base=(AVRational){1, 25};
    if(avcodec_open2(m_enCtx,encoder,nullptr)<0)
    {
        qDebug()<<"打开截屏编码器时出错!";
        QCoreApplication::exit(-1);
    }
    m_frame->width=m_width;
    m_frame->height=m_height;
    m_frame->format=AV_PIX_FMT_YUV420P;
    if(av_frame_get_buffer(m_frame,1)<0||av_frame_make_writable(m_frame)<0)
    {
        qDebug()<<"初始化截屏AVFrame时出错!";
        QCoreApplication::exit(-1);
    }

    m_isReady=true;
}

void ScreenShooter::screenShot(QByteArray pic)
{
    if(!m_isReady)
    {
        return;
    }
    memcpy(m_frame->data[0],pic.data(),m_width*m_height);
    memcpy(m_frame->data[1],pic.data()+m_width*m_height,m_width*m_height/4);
    memcpy(m_frame->data[2],pic.data()+m_width*m_height*5/4,m_width*m_height/4);
    if(avcodec_send_frame(m_enCtx,m_frame)<0)
    {
        qDebug()<<"截屏器发送一帧时出错!";
    }
    if(avcodec_receive_packet(m_enCtx,m_pkt)<0)
    {
        qDebug()<<"截屏器解码时出错!";
    }
    QString fileName=m_picNamePrefix+"_"+QDateTime::currentDateTime().toString("yyyy-MM-dd_HH:mm:ss")+".jp2";
    QString filePath=QDir::homePath()+"/Pictures/monitor-captured/ScreenShot/";
    m_picFile->setFileName(filePath+fileName);
    if(!m_picFile->open(QIODeviceBase::WriteOnly|QIODeviceBase::Truncate))
    {
        qDebug()<<"打开截屏文件失败!";
    }
    m_picFile->write((const char*)(m_pkt->data),m_width*m_height*3/2);
    m_picFile->close();
    emit screenShotOK(filePath);
}
