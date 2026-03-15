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
    Q_PROPERTY(VideoConsumerState state MEMBER m_state READ currentState WRITE setState)
public:
    explicit TVideoConsumer(QObject *parent = nullptr);
    auto currentState()const{return m_state;};

    void startStateMachine(){m_stateMachine->start();};
    void setBuffer(TAVBufferPool *buf){m_buffer=buf;};
private:
    VideoConsumerState m_state;
    QStateMachine *m_stateMachine;
    QState *m_stateNoInput;
    QState *m_stateInit;
    QState *m_stateReading;
    QFinalState *m_stateDestroy;
    QState *m_stateReset;
    void setState(VideoConsumerState s){this->m_state=s;};
    void initStateMachine();

    QTimer *m_baseTimer;

    int m_width;
    int m_height;
    qreal m_fps;
    TAVBufferPool *m_buffer;
    bool m_isWorking;//暂时
    QByteArray m_data;

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
