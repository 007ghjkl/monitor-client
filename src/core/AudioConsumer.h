#ifndef AUDIOCONSUMER_H
#define AUDIOCONSUMER_H
#include <QObject>
#include <QBuffer>
#include <QStateMachine>
#include <QFinalState>
#include <QAudioSink>
#include <QMediaDevices>
#include "AVBufferPool.h"
#include <QCloseEvent>
extern "C"{
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/timestamp.h>
}
enum class AudioConsumerState{NoInput,Init,Reading,Destroy,Reset};
class AudioConsumer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(AudioConsumerState state MEMBER m_state READ currentState WRITE setState)
private:
    AudioConsumerState m_state;
    QStateMachine *m_stateMachine;
    QState *m_stateNoInput;
    QState *m_stateInit;
    QState *m_stateReading;
    QFinalState *m_stateDestroy;
    QState *m_stateReset;
    void setState(AudioConsumerState s){this->m_state=s;};
    void initStateMachine();

    AudioBufferPool *m_buffer;
    qreal m_delayTime;

    QAudioFormat m_format;
    QAudioDevice m_device;
    QAudioSink *m_player;

private slots:
    void preInit();
    void read();
    void init();
    void destroy();
    void reset();
public:
    explicit AudioConsumer(QObject *parent = nullptr);
    ~AudioConsumer();
    void setBuffer(AudioBufferPool *buf){m_buffer=buf;};
    auto currentState()const{return m_state;};
    void startStateMachine(){m_stateMachine->start();};
public slots:
    void respondToProducer(QAudioFormat f);
    void respondToMainDestroy();
    void respondToMainSuspend();
    void setVolume(int value);
signals:
    void foundInput();//内部状态转移信号，配合respondToProducer()
    void initFinished();
    void turnToDestroy();//内部状态转移信号，配合respondToMainDestroy()
    void turnToReset();//内部状态转移信号,与MainWindow::changeUrl()同义
    void resetFinished();//内部状态转移信号，配合reset()
};

#endif // AUDIOCONSUMER_H
