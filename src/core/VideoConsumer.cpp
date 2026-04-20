#include "VideoConsumer.h"
#include <QCoreApplication>
VideoConsumer::VideoConsumer(QObject *parent)
    : QObject{parent}
{
    initStateMachine();
}

void VideoConsumer::init()
{
    m_frameSize=m_width*m_height*3/2;
    m_needPic=false;
    //其他
    qDebug()<<"视频消费者:"<<m_width<<"x"<<m_height;
    emit initFinished();
}

void VideoConsumer::read()
{
    m_readConnection=connect(m_buffer,&AVBufferPool::readyRead,this,[this]{
        m_data=m_buffer->read(m_frameSize);
        emit sendFrame(m_data);
        if(m_needPic)
        {
            emit notifyScreenShot(m_data);
            m_needPic=false;
        }
    });
}

void VideoConsumer::destroy()
{
    disconnect(m_readConnection);
    qDebug()<<"视频消费者:开始销毁...";
    m_stateMachine->stop();
}

void VideoConsumer::reset()
{
    disconnect(m_readConnection);
    m_data.clear();
    m_width=m_height=0;
    emit resetFinished();
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
    m_stateReading->addTransition(this,&VideoConsumer::turnToDestroy,m_stateDestroy);
    m_stateReading->addTransition(this,&VideoConsumer::turnToReset,m_stateReset);
    m_stateReset->addTransition(this,&VideoConsumer::resetFinished,m_stateNoInput);
    m_stateReset->addTransition(this,&VideoConsumer::turnToDestroy,m_stateDestroy);
    //应用设置
    m_stateMachine->setInitialState(m_stateNoInput);
}

void VideoConsumer::respondToProducer(int w, int h)
{
    m_width=w;
    m_height=h;
    emit foundInput();
}

void VideoConsumer::respondToMainDestroy()
{
    setState(VideoConsumerState::Destroy);
    emit turnToDestroy();
}

void VideoConsumer::respondToMainScreenShot()
{
    m_needPic=true;
}
