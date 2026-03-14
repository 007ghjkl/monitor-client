#include "tvideoconsumer.h"
#include <QCoreApplication>
TVideoConsumer::TVideoConsumer(QObject *parent)
    : QObject{parent}
{
    baseTimer=new QTimer(this);
    baseTimer->setTimerType(Qt::PreciseTimer);

    initStateMachine();
}

void TVideoConsumer::init()
{
    //同步策略
    connect(baseTimer,&QTimer::timeout,this,&TVideoConsumer::readyToRead);

    if((buffer->open(QIODeviceBase::ReadOnly))==false)
    {
        qDebug()<<"视频消费者打开缓冲区失败!";
        QCoreApplication::exit(-1);
    }
    isWorking=true;
    buffer->sleep(1000);
    emit setDisplaySize(width,height);
    qDebug()<<"视频消费者:"<<width<<"x"<<height<<"fps:"<<fps;
    emit initFinished();
}

void TVideoConsumer::read()
{
    if(isWorking)
    {
        baseTimer->start(1/fps*1000);
        data=buffer->read(width*height*3/2);
        emit sendFrame(data);
    }
}

void TVideoConsumer::destroy()
{
    qDebug()<<"视频消费者:开始销毁...";
    if(buffer->isOpen())
    {
        buffer->close();
    }
    stateMachine->stop();
}

void TVideoConsumer::reset()
{
    baseTimer->stop();
    buffer->close();
    data.clear();
    width=height=fps=0;
    qDebug()<<"视频消费者:异步重置完成。";
    emit turnToNoInput();
}

void TVideoConsumer::initStateMachine()
{
    stateMachine=new QStateMachine(this);
    stateNoInput=new QState(stateMachine);
    stateInit=new QState(stateMachine);//用于同步
    stateReading=new QState(stateMachine);
    stateDestroy=new QFinalState(stateMachine);
    stateReset=new QState(stateMachine);
    //设定属性
    QVariant stateVal;
    stateVal.setValue(VideoConsumerState::NoInput);
    stateNoInput->assignProperty(this,"state",stateVal);
    stateVal.setValue(VideoConsumerState::Init);
    stateInit->assignProperty(this,"state",stateVal);
    stateVal.setValue(VideoConsumerState::Reading);
    stateReading->assignProperty(this,"state",stateVal);
    stateVal.setValue(VideoConsumerState::Reset);
    stateReset->assignProperty(this,"state",stateVal);
    //Destroy自己设定
    //设置操作
    connect(stateInit,&QState::entered,this,&TVideoConsumer::init);
    connect(stateReading,&QState::entered,this,&TVideoConsumer::read);
    connect(stateDestroy,&QState::entered,this,&TVideoConsumer::destroy);
    connect(stateReset,&QState::entered,this,&TVideoConsumer::reset);
    //设置状态转移
    stateNoInput->addTransition(this,&TVideoConsumer::foundInput,stateInit);
    stateNoInput->addTransition(this,&TVideoConsumer::turnToDestroy,stateDestroy);
    stateInit->addTransition(this,&TVideoConsumer::initFinished,stateReading);
    stateInit->addTransition(this,&TVideoConsumer::turnToDestroy,stateDestroy);
    stateInit->addTransition(this,&TVideoConsumer::turnToReset,stateReset);
    stateReading->addTransition(this,&TVideoConsumer::readyToRead,stateReading);
    stateReading->addTransition(this,&TVideoConsumer::turnToDestroy,stateDestroy);
    stateReading->addTransition(this,&TVideoConsumer::turnToReset,stateReset);
    stateReset->addTransition(this,&TVideoConsumer::turnToNoInput,stateNoInput);
    //应用设置
    stateMachine->setInitialState(stateNoInput);
}

void TVideoConsumer::respondToProducer(int w, int h,qreal fps)
{
    width=w;
    height=h;
    this->fps=fps;
    emit foundInput();
}

void TVideoConsumer::respondToMainDestroy()
{
    setState(VideoConsumerState::Destroy);
    isWorking=false;
    emit turnToDestroy();
}

void TVideoConsumer::respondToMainDisconnect()
{
    isWorking=false;
    qDebug()<<"视频消费者:开始断开连接。";
    emit turnToReset();
}
