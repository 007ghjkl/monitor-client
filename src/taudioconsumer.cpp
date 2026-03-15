#include "taudioconsumer.h"
#include <QElapsedTimer>

void TAudioConsumer::preInit()
{
    m_format.setSampleRate(48000);
    m_format.setSampleFormat(QAudioFormat::Int16);
    m_format.setChannelCount(2);
    m_player=new QAudioSink(m_format,this);
    m_player->deleteLater();
    disconnect(m_stateNoInput,&QState::entered,this,&TAudioConsumer::preInit);
}

void TAudioConsumer::read()
{
    m_player->start(m_buffer);
    qDebug()<<"音频消费者:开始播放。";
    // qDebug()<<player->error();
    // qDebug()<<player->state();
}

void TAudioConsumer::init()
{
    m_player=new QAudioSink(m_format,this);
    // qDebug()<<"音频默认缓冲:"<<player->bufferSize();
    if(m_buffer->open(QIODeviceBase::ReadOnly)==false)
    {
        qDebug()<<"音频消费者打开缓冲区失败!";
        QCoreApplication::exit(-1);
    }
    connect(m_player,&QAudioSink::stateChanged,this,[this](auto s){qDebug()<<"AudioSink:"<<s<<m_player->error();});
    m_buffer->sleep(1000);
    qDebug()<<"音频消费者:"<<"采样率"<<m_format.sampleRate()<<",声道格式"<<m_format.channelConfig()<<",采样格式"<<m_format.sampleFormat();
    emit initFinished();
}

void TAudioConsumer::destroy()
{
    qDebug()<<"音频消费者:开始销毁...";
    if(m_buffer->isOpen())
    {
        if(m_player)
        {
            m_player->stop();
        }
        m_buffer->close();
    }
    m_stateMachine->stop();
}

void TAudioConsumer::reset()
{
    if(m_player)
    {
        m_player->stop();
        m_player->deleteLater();
    }
    if(m_buffer->isOpen())
    {
        m_buffer->close();
    }
    qDebug()<<"音频消费者:异步重置完成。";
    emit turnToNoInput();
}

TAudioConsumer::TAudioConsumer(QObject *parent)
    : QObject{parent}
{
    initStateMachine();
}

TAudioConsumer::~TAudioConsumer()
{

}

void TAudioConsumer::initStateMachine()
{
    m_stateMachine=new QStateMachine(this);
    m_stateNoInput=new QState(m_stateMachine);
    m_stateInit=new QState(m_stateMachine);
    m_stateReading=new QState(m_stateMachine);
    m_stateDestroy=new QFinalState(m_stateMachine);
    m_stateReset=new QState(m_stateMachine);
    //设定属性
    QVariant stateVal;
    stateVal.setValue(AudioConsumerState::NoInput);
    m_stateNoInput->assignProperty(this,"state",stateVal);
    stateVal.setValue(AudioConsumerState::Init);
    m_stateInit->assignProperty(this,"state",stateVal);
    stateVal.setValue(AudioConsumerState::Reading);
    m_stateReading->assignProperty(this,"state",stateVal);
    stateVal.setValue(AudioConsumerState::Reset);
    m_stateReset->assignProperty(this,"state",stateVal);
    //Destroy自己设定
    //设置操作
    connect(m_stateNoInput,&QState::entered,this,&TAudioConsumer::preInit);
    connect(m_stateInit,&QState::entered,this,&TAudioConsumer::init);
    connect(m_stateReading,&QState::entered,this,&TAudioConsumer::read);
    connect(m_stateDestroy,&QFinalState::entered,this,&TAudioConsumer::destroy);
    connect(m_stateReset,&QState::entered,this,&TAudioConsumer::reset);
    //设置状态转移
    m_stateNoInput->addTransition(this,&TAudioConsumer::foundFormat,m_stateInit);
    m_stateNoInput->addTransition(this,&TAudioConsumer::turnToDestroy,m_stateDestroy);
    m_stateInit->addTransition(this,&TAudioConsumer::initFinished,m_stateReading);
    m_stateInit->addTransition(this,&TAudioConsumer::turnToDestroy,m_stateDestroy);
    m_stateInit->addTransition(this,&TAudioConsumer::turnToReset,m_stateReset);
    m_stateReading->addTransition(this,&TAudioConsumer::turnToDestroy,m_stateDestroy);
    m_stateReading->addTransition(this,&TAudioConsumer::turnToReset,m_stateReset);
    m_stateReset->addTransition(this,&TAudioConsumer::turnToNoInput,m_stateNoInput);
    //应用设置
    m_stateMachine->setInitialState(m_stateNoInput);
}

void TAudioConsumer::respondToProducer(QAudioFormat f)
{
    if(currentState()==AudioConsumerState::NoInput)
    {
        m_format=f;
        emit foundFormat();
    }
}

void TAudioConsumer::respondToMainDestroy()
{
    setState(AudioConsumerState::Destroy);
    emit turnToDestroy();
}

void TAudioConsumer::respondToMainDisconnect()
{
    qDebug()<<"音频消费者:开始断开连接。";
    emit turnToReset();
}

