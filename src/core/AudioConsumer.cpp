#include "AudioConsumer.h"
#include <QMediaDevices>
void AudioConsumer::preInit()
{
    m_format.setSampleRate(48000);
    m_format.setSampleFormat(QAudioFormat::Int16);
    m_format.setChannelCount(2);
    m_player=new QAudioSink(m_format,this);
    m_player->deleteLater();
    m_player=nullptr;
    disconnect(m_stateNoInput,&QState::entered,this,&AudioConsumer::preInit);
    m_device=QMediaDevices::defaultAudioOutput();
}

void AudioConsumer::read()
{
    m_player->start(m_buffer);
    qDebug()<<"音频消费者:开始播放,音量:"<<m_player->volume();
    // qDebug()<<player->error();
    // qDebug()<<player->state();
}

void AudioConsumer::init()
{
    m_player=new QAudioSink(m_device,m_format,this);
    m_player->setBufferSize(2048);
    qDebug()<<"音频默认缓冲:"<<m_player->bufferSize();
    connect(m_player,&QAudioSink::stateChanged,this,[this](auto s){qDebug()<<"AudioSink:"<<s<<m_player->error();});
    qDebug()<<"音频消费者:"<<"采样率"<<m_format.sampleRate()<<",声道数"<<m_format.channelCount()<<",采样格式"<<m_format.sampleFormat();
    // qDebug()<<"audioBuf:"<<m_buffer->isOpen();
    emit initFinished();
}

void AudioConsumer::destroy()
{
    qDebug()<<"音频消费者:开始销毁...";
    if(m_player)
    {
        m_player->reset();
    }
    m_stateMachine->stop();
}

void AudioConsumer::reset()
{
    m_player->stop();
    m_player->deleteLater();
    emit resetFinished();
}

AudioConsumer::AudioConsumer(QObject *parent)
    : QObject{parent}
{
    initStateMachine();
}

AudioConsumer::~AudioConsumer()
{

}

void AudioConsumer::initStateMachine()
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
    connect(m_stateNoInput,&QState::entered,this,&AudioConsumer::preInit);
    connect(m_stateInit,&QState::entered,this,&AudioConsumer::init);
    connect(m_stateReading,&QState::entered,this,&AudioConsumer::read);
    connect(m_stateDestroy,&QFinalState::entered,this,&AudioConsumer::destroy);
    connect(m_stateReset,&QState::entered,this,&AudioConsumer::reset);
    //设置状态转移
    m_stateNoInput->addTransition(this,&AudioConsumer::foundInput,m_stateInit);
    m_stateNoInput->addTransition(this,&AudioConsumer::turnToDestroy,m_stateDestroy);
    m_stateInit->addTransition(this,&AudioConsumer::initFinished,m_stateReading);
    m_stateInit->addTransition(this,&AudioConsumer::turnToDestroy,m_stateDestroy);
    m_stateReading->addTransition(this,&AudioConsumer::turnToDestroy,m_stateDestroy);
    m_stateReading->addTransition(this,&AudioConsumer::turnToReset,m_stateReset);
    m_stateReset->addTransition(this,&AudioConsumer::resetFinished,m_stateNoInput);
    m_stateReset->addTransition(this,&AudioConsumer::turnToDestroy,m_stateDestroy);
    //应用设置
    m_stateMachine->setInitialState(m_stateNoInput);
}

void AudioConsumer::respondToProducer(QAudioFormat f)
{
    m_format=f;
    emit foundInput();
}

void AudioConsumer::respondToMainDestroy()
{
    setState(AudioConsumerState::Destroy);
    emit turnToDestroy();
}

void AudioConsumer::respondToMainSuspend()
{
    if(currentState()!=AudioConsumerState::Reading){return;}
    if(QAudio::ActiveState==m_player->state())
    {
        m_player->suspend();
    }
    else
    {
        m_player->resume();
    }
}

void AudioConsumer::setVolume(int value)
{
    m_player->setVolume((qreal)value/100);
}

