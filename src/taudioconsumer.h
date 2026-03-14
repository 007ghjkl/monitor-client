#ifndef TAUDIOCONSUMER_H
#define TAUDIOCONSUMER_H
#include <QObject>
#include <QBuffer>
#include <QStateMachine>
#include <QFinalState>
#include <QAudioSink>
#include "tavbufferpool.h"
#include <QCloseEvent>
extern "C"{
#include <libavformat/avformat.h>
#include <libswresample/swresample.h>
#include <libavutil/timestamp.h>
}
enum class AudioConsumerState{NoInput,Init,Reading,Destroy,Reset};
class TAudioConsumer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(AudioConsumerState state MEMBER state READ currentState WRITE setState)
private:
    AudioConsumerState state;
    QStateMachine *stateMachine;
    QState *stateNoInput;
    QState *stateInit;
    QState *stateReading;
    QFinalState *stateDestroy;
    QState *stateReset;
    void setState(AudioConsumerState s){this->state=s;};
    void initStateMachine();

    TAVBufferPool *buffer;
    qreal delayTime;

    QAudioFormat format;
    QAudioSink *player;

private slots:
    void preInit();
    void read();
    void init();
    void destroy();
    void reset();
public:
    explicit TAudioConsumer(QObject *parent = nullptr);
    ~TAudioConsumer();
    void setBuffer(TAVBufferPool *buf){buffer=buf;};
    auto currentState()const{return state;};
    void startStateMachine(){stateMachine->start();};
public slots:
    void respondToProducer(QAudioFormat f);
    void respondToMainDestroy();
    void respondToMainDisconnect();
signals:
    void foundFormat();//内部状态转移信号，配合respondToProducer()
    void initFinished();
    void turnToDestroy();//内部状态转移信号，配合respondToMainDestroy()
    void turnToReset();//内部状态转移信号，配合respondToMainDisconnect()
    void turnToNoInput();//内部状态转移信号，配合reset()
};

#endif // TAUDIOCONSUMER_H
