#ifndef VIDEOCONSUMER_H
#define VIDEOCONSUMER_H

#include <QObject>
#include <QStateMachine>
#include <QFinalState>
#include <QTimer>
#include "AVBufferPool.h"
enum class VideoConsumerState{NoInput,Init,Reading,Destroy,Reset};
class VideoConsumer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(VideoConsumerState state MEMBER m_state READ currentState WRITE setState)
public:
    explicit VideoConsumer(QObject *parent = nullptr);
    auto currentState()const{return m_state;};

    void startStateMachine(){m_stateMachine->start();};
    void setBuffer(VideoBufferPool *buf){m_buffer=buf;};
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

    int m_width;
    int m_height;
    qsizetype m_frameSize;
    VideoBufferPool *m_buffer;
    QByteArray m_data;

    QMetaObject::Connection m_readConnection;
    bool m_needPic;

    void init();
    void read();
    void destroy();
    void reset();
public slots:
    void respondToProducer(int w,int h);
    void respondToMainDestroy();
    void respondToMainScreenShot();
signals:
    void foundInput();//内部状态转移信号，配合respondToProducer()
    void initFinished();
    void turnToDestroy();//内部状态转移信号，配合respondToMainDestroy()
    void turnToReset();//内部状态转移信号，与MainWindow::changeUrl()同义
    void resetFinished();
    void sendFrame(QByteArray d);
    void notifyScreenShot(QByteArray d);
};

#endif // VIDEOCONSUMER_H
