#include "VideoConsumer.h"
#include <QCoreApplication>
VideoConsumer::VideoConsumer(QObject *parent)
    : QObject{parent}
{
    m_baseTimer=new QTimer(this);
    m_baseTimer->setTimerType(Qt::PreciseTimer);

    initStateMachine();
}

void VideoConsumer::init()
{
    //同步策略
    connect(m_baseTimer,&QTimer::timeout,this,&VideoConsumer::readyToRead);

    if((m_buffer->open(QIODeviceBase::ReadOnly))==false)
    {
        qDebug()<<"视频消费者打开缓冲区失败!";
        QCoreApplication::exit(-1);
    }
    //一帧截屏
    auto encoder=avcodec_find_encoder(AV_CODEC_ID_JPEG2000);
    if(!encoder)
    {
        qDebug()<<"找不到JPEG2000编码器!";
        QCoreApplication::exit(-1);
    }
    m_picEncCtx=avcodec_alloc_context3(encoder);
    if(!m_picEncCtx)
    {
        qDebug()<<"给截屏编码器分配上下文空间时出错!";
        QCoreApplication::exit(-1);
    }
    m_picEncCtx->width=m_width;
    m_picEncCtx->height=m_height;
    m_picEncCtx->pix_fmt=AV_PIX_FMT_YUV420P;
    m_picEncCtx->time_base=(AVRational){1, 25};
    if(avcodec_open2(m_picEncCtx,encoder,nullptr)<0)
    {
        qDebug()<<"打开截屏编码器时出错!";
        QCoreApplication::exit(-1);
    }
    m_picPkt=av_packet_alloc();
    m_picFrame=av_frame_alloc();
    if(!m_picPkt||!m_picFrame)
    {
        qDebug()<<"分配截屏AVFrame或AVPacket空间失败!";
        QCoreApplication::exit(-1);
    }
    m_picFrame->width=m_width;
    m_picFrame->height=m_height;
    m_picFrame->format=AV_PIX_FMT_YUV420P;
    if(av_frame_get_buffer(m_picFrame,1)<0||av_frame_make_writable(m_picFrame)<0)
    {
        qDebug()<<"初始化截屏AVFrame时出错!";
        QCoreApplication::exit(-1);
    }
    m_picFile=new QFile(this);
    m_needPic=false;
    //其他
    m_isWorking=true;
    m_buffer->sleep(1000);
    emit setDisplaySize(m_width,m_height);
    qDebug()<<"视频消费者:"<<m_width<<"x"<<m_height<<"fps:"<<m_fps;
    emit initFinished();
}

void VideoConsumer::read()
{
    if(m_isWorking)
    {
        m_baseTimer->start(1/m_fps*1000);
        m_data=m_buffer->read(m_width*m_height*3/2);
        if(m_needPic)
        {
            memcpy(m_picFrame->data[0],m_data.data(),m_width*m_height);
            memcpy(m_picFrame->data[1],m_data.data()+m_width*m_height,m_width*m_height/4);
            memcpy(m_picFrame->data[2],m_data.data()+m_width*m_height*5/4,m_width*m_height/4);
            if(avcodec_send_frame(m_picEncCtx,m_picFrame)<0)
            {
                qDebug()<<"截屏器发送一帧时出错!";
            }
            if(avcodec_receive_packet(m_picEncCtx,m_picPkt)<0)
            {
                qDebug()<<"截屏器解码时出错!";
            }
            QString fileName=m_picNamePrefix+"_"+QDateTime::currentDateTime().toString("yyyy-MM-dd_HH:mm:ss");
            QDir::setCurrent("/home/rainbow/Pictures/monitor-captured/ScreenShot");
            m_picFile->setFileName(fileName);
            if(!m_picFile->open(QIODeviceBase::WriteOnly|QIODeviceBase::Truncate))
            {
                qDebug()<<"打开截屏文件失败!";
            }
            m_picFile->write((const char*)(m_picPkt->data),m_width*m_height*3/2);
            m_needPic=false;
            m_picFile->close();
            emit screenShotOK(QDir::currentPath());
            QDir::setCurrent(QCoreApplication::applicationDirPath());
        }
        emit sendFrame(m_data);
    }
}

void VideoConsumer::destroy()
{
    qDebug()<<"视频消费者:开始销毁...";
    if(m_buffer->isOpen())
    {
        m_buffer->close();
    }
    avcodec_free_context(&m_picEncCtx);
    av_packet_free(&m_picPkt);
    av_frame_free(&m_picFrame);
    m_stateMachine->stop();
}

void VideoConsumer::reset()
{
    m_baseTimer->stop();
    m_buffer->close();
    m_data.clear();
    m_width=m_height=m_fps=0;
    qDebug()<<"视频消费者:异步重置完成。";
    emit turnToNoInput();
}

void VideoConsumer::initStateMachine()
{
    m_stateMachine=new QStateMachine(this);
    m_stateNoInput=new QState(m_stateMachine);
    m_stateInit=new QState(m_stateMachine);//用于同步
    m_stateReading=new QState(m_stateMachine);
    m_stateDestroy=new QFinalState(m_stateMachine);
    m_stateReset=new QState(m_stateMachine);
    //设定属性
    QVariant stateVal;
    stateVal.setValue(VideoConsumerState::NoInput);
    m_stateNoInput->assignProperty(this,"state",stateVal);
    stateVal.setValue(VideoConsumerState::Init);
    m_stateInit->assignProperty(this,"state",stateVal);
    stateVal.setValue(VideoConsumerState::Reading);
    m_stateReading->assignProperty(this,"state",stateVal);
    stateVal.setValue(VideoConsumerState::Reset);
    m_stateReset->assignProperty(this,"state",stateVal);
    //Destroy自己设定
    //设置操作
    connect(m_stateInit,&QState::entered,this,&VideoConsumer::init);
    connect(m_stateReading,&QState::entered,this,&VideoConsumer::read);
    connect(m_stateDestroy,&QState::entered,this,&VideoConsumer::destroy);
    connect(m_stateReset,&QState::entered,this,&VideoConsumer::reset);
    //设置状态转移
    m_stateNoInput->addTransition(this,&VideoConsumer::foundInput,m_stateInit);
    m_stateNoInput->addTransition(this,&VideoConsumer::turnToDestroy,m_stateDestroy);
    m_stateInit->addTransition(this,&VideoConsumer::initFinished,m_stateReading);
    m_stateInit->addTransition(this,&VideoConsumer::turnToDestroy,m_stateDestroy);
    m_stateInit->addTransition(this,&VideoConsumer::turnToReset,m_stateReset);
    m_stateReading->addTransition(this,&VideoConsumer::readyToRead,m_stateReading);
    m_stateReading->addTransition(this,&VideoConsumer::turnToDestroy,m_stateDestroy);
    m_stateReading->addTransition(this,&VideoConsumer::turnToReset,m_stateReset);
    m_stateReset->addTransition(this,&VideoConsumer::turnToNoInput,m_stateNoInput);
    //应用设置
    m_stateMachine->setInitialState(m_stateNoInput);
}

void VideoConsumer::respondToProducer(int w, int h,qreal fps)
{
    m_width=w;
    m_height=h;
    this->m_fps=fps;
    emit foundInput();
}

void VideoConsumer::respondToMainDestroy()
{
    setState(VideoConsumerState::Destroy);
    m_isWorking=false;
    emit turnToDestroy();
}

void VideoConsumer::respondToMainDisconnect()
{
    m_isWorking=false;
    qDebug()<<"视频消费者:开始断开连接。";
    emit turnToReset();
}

void VideoConsumer::respondToMainScreenShot()
{
    m_needPic=true;
}
