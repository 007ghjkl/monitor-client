#ifndef TVIDEOCONSUMER_H
#define TVIDEOCONSUMER_H

#include <QObject>
#include <QStateMachine>
#include <QFinalState>
#include <QTimer>
#include "tavbufferpool.h"
enum class VideoConsumerState{NoInput,Init,Reading,Destroy,Reset};
class TVideoConsumer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(VideoConsumerState state MEMBER state READ currentState WRITE setState)
public:
    explicit TVideoConsumer(QObject *parent = nullptr);
    auto currentState()const{return state;};

    void startStateMachine(){stateMachine->start();};
    void setBuffer(TAVBufferPool *buf){buffer=buf;};
private:
    VideoConsumerState state;
    QStateMachine *stateMachine;
    QState *stateNoInput;
    QState *stateInit;
    QState *stateReading;
    QFinalState *stateDestroy;
    QState *stateReset;
    void setState(VideoConsumerState s){this->state=s;};
    void initStateMachine();

    QTimer *baseTimer;

    int width;
    int height;
    qreal fps;
    TAVBufferPool *buffer;
    bool isWorking;//暂时
    QByteArray data;

    void init();
    void read();
    void destroy();
    void reset();
public slots:
    void respondToProducer(int w,int h,qreal fps);
    void respondToMainDestroy();
    void respondToMainDisconnect();
signals:
    void foundInput();//内部状态转移信号，配合respondToProducer()
    void initFinished();
    void readyToRead();
    void turnToDestroy();//内部状态转移信号，配合respondToMainDestroy()
    void turnToReset();//内部状态转移信号，配合respondToMainDisconnect()
    void turnToNoInput();//内部状态转移信号，配合reset()
    void setDisplaySize(int w,int h);
    void sendFrame(QByteArray d);
};

#endif // TVIDEOCONSUMER_H
