#include "tvideoconsumer.h"
#include <QCoreApplication>
TVideoConsumer::TVideoConsumer(QObject *parent)
    : QObject{parent}
{
    m_baseTimer=new QTimer(this);
    m_baseTimer->setTimerType(Qt::PreciseTimer);

    initStateMachine();
}

void TVideoConsumer::init()
{
    //同步策略
    connect(m_baseTimer,&QTimer::timeout,this,&TVideoConsumer::readyToRead);

    if((m_buffer->open(QIODeviceBase::ReadOnly))==false)
    {
        qDebug()<<"视频消费者打开缓冲区失败!";
        QCoreApplication::exit(-1);
    }
    m_isWorking=true;
    m_buffer->sleep(1000);
    emit setDisplaySize(m_width,m_height);
    qDebug()<<"视频消费者:"<<m_width<<"x"<<m_height<<"fps:"<<m_fps;
    emit initFinished();
}

void TVideoConsumer::read()
{
    if(m_isWorking)
    {
        m_baseTimer->start(1/m_fps*1000);
        m_data=m_buffer->read(m_width*m_height*3/2);
        emit sendFrame(m_data);
    }
}

void TVideoConsumer::destroy()
{
    qDebug()<<"视频消费者:开始销毁...";
    if(m_buffer->isOpen())
    {
        m_buffer->close();
    }
    m_stateMachine->stop();
}

void TVideoConsumer::reset()
{
    m_baseTimer->stop();
    m_buffer->close();
    m_data.clear();
    m_width=m_height=m_fps=0;
    qDebug()<<"视频消费者:异步重置完成。";
    emit turnToNoInput();
}

void TVideoConsumer::initStateMachine()
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
    connect(m_stateInit,&QState::entered,this,&TVideoConsumer::init);
    connect(m_stateReading,&QState::entered,this,&TVideoConsumer::read);
    connect(m_stateDestroy,&QState::entered,this,&TVideoConsumer::destroy);
    connect(m_stateReset,&QState::entered,this,&TVideoConsumer::reset);
    //设置状态转移
    m_stateNoInput->addTransition(this,&TVideoConsumer::foundInput,m_stateInit);
    m_stateNoInput->addTransition(this,&TVideoConsumer::turnToDestroy,m_stateDestroy);
    m_stateInit->addTransition(this,&TVideoConsumer::initFinished,m_stateReading);
    m_stateInit->addTransition(this,&TVideoConsumer::turnToDestroy,m_stateDestroy);
    m_stateInit->addTransition(this,&TVideoConsumer::turnToReset,m_stateReset);
    m_stateReading->addTransition(this,&TVideoConsumer::readyToRead,m_stateReading);
    m_stateReading->addTransition(this,&TVideoConsumer::turnToDestroy,m_stateDestroy);
    m_stateReading->addTransition(this,&TVideoConsumer::turnToReset,m_stateReset);
    m_stateReset->addTransition(this,&TVideoConsumer::turnToNoInput,m_stateNoInput);
    //应用设置
    m_stateMachine->setInitialState(m_stateNoInput);
}

void TVideoConsumer::respondToProducer(int w, int h,qreal fps)
{
    m_width=w;
    m_height=h;
    this->m_fps=fps;
    emit foundInput();
}

void TVideoConsumer::respondToMainDestroy()
{
    setState(VideoConsumerState::Destroy);
    m_isWorking=false;
    emit turnToDestroy();
}

void TVideoConsumer::respondToMainDisconnect()
{
    m_isWorking=false;
    qDebug()<<"视频消费者:开始断开连接。";
    emit turnToReset();
}
