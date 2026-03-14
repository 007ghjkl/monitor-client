#include "taudioconsumer.h"
#include <QElapsedTimer>

void TAudioConsumer::preInit()
{
    format.setSampleRate(48000);
    format.setSampleFormat(QAudioFormat::Int16);
    format.setChannelCount(2);
    player=new QAudioSink(format,this);
    player->deleteLater();
    disconnect(stateNoInput,&QState::entered,this,&TAudioConsumer::preInit);
}

void TAudioConsumer::read()
{
    player->start(buffer);
    qDebug()<<"音频消费者:开始播放。";
    // qDebug()<<player->error();
    // qDebug()<<player->state();
}

void TAudioConsumer::init()
{
    player=new QAudioSink(format,this);
    // qDebug()<<"音频默认缓冲:"<<player->bufferSize();
    if(buffer->open(QIODeviceBase::ReadOnly)==false)
    {
        qDebug()<<"音频消费者打开缓冲区失败!";
        QCoreApplication::exit(-1);
    }
    connect(player,&QAudioSink::stateChanged,this,[this](auto s){qDebug()<<"AudioSink:"<<s<<player->error();});
    buffer->sleep(1000);
    qDebug()<<"音频消费者:"<<"采样率"<<format.sampleRate()<<",声道格式"<<format.channelConfig()<<",采样格式"<<format.sampleFormat();
    emit initFinished();
}

void TAudioConsumer::destroy()
{
    qDebug()<<"音频消费者:开始销毁...";
    if(buffer->isOpen())
    {
        if(player)
        {
            player->stop();
        }
        buffer->close();
    }
    stateMachine->stop();
}

void TAudioConsumer::reset()
{
    if(player)
    {
        player->stop();
        player->deleteLater();
    }
    if(buffer->isOpen())
    {
        buffer->close();
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
    stateMachine=new QStateMachine(this);
    stateNoInput=new QState(stateMachine);
    stateInit=new QState(stateMachine);
    stateReading=new QState(stateMachine);
    stateDestroy=new QFinalState(stateMachine);
    stateReset=new QState(stateMachine);
    //设定属性
    QVariant stateVal;
    stateVal.setValue(AudioConsumerState::NoInput);
    stateNoInput->assignProperty(this,"state",stateVal);
    stateVal.setValue(AudioConsumerState::Init);
    stateInit->assignProperty(this,"state",stateVal);
    stateVal.setValue(AudioConsumerState::Reading);
    stateReading->assignProperty(this,"state",stateVal);
    stateVal.setValue(AudioConsumerState::Reset);
    stateReset->assignProperty(this,"state",stateVal);
    //Destroy自己设定
    //设置操作
    connect(stateNoInput,&QState::entered,this,&TAudioConsumer::preInit);
    connect(stateInit,&QState::entered,this,&TAudioConsumer::init);
    connect(stateReading,&QState::entered,this,&TAudioConsumer::read);
    connect(stateDestroy,&QFinalState::entered,this,&TAudioConsumer::destroy);
    connect(stateReset,&QState::entered,this,&TAudioConsumer::reset);
    //设置状态转移
    stateNoInput->addTransition(this,&TAudioConsumer::foundFormat,stateInit);
    stateNoInput->addTransition(this,&TAudioConsumer::turnToDestroy,stateDestroy);
    stateInit->addTransition(this,&TAudioConsumer::initFinished,stateReading);
    stateInit->addTransition(this,&TAudioConsumer::turnToDestroy,stateDestroy);
    stateInit->addTransition(this,&TAudioConsumer::turnToReset,stateReset);
    stateReading->addTransition(this,&TAudioConsumer::turnToDestroy,stateDestroy);
    stateReading->addTransition(this,&TAudioConsumer::turnToReset,stateReset);
    stateReset->addTransition(this,&TAudioConsumer::turnToNoInput,stateNoInput);
    //应用设置
    stateMachine->setInitialState(stateNoInput);
}

void TAudioConsumer::respondToProducer(QAudioFormat f)
{
    if(currentState()==AudioConsumerState::NoInput)
    {
        format=f;
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

