#ifndef VIDEOCONSUMER_H
#define VIDEOCONSUMER_H

#include <QObject>
#include <QStateMachine>
#include <QFinalState>
#include <QTimer>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include "AVBufferPool.h"
extern "C"{
#include <libavcodec/avcodec.h>
}
enum class VideoConsumerState{NoInput,Init,Reading,Destroy,Reset};
class VideoConsumer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(VideoConsumerState state MEMBER m_state READ currentState WRITE setState)
public:
    explicit VideoConsumer(QObject *parent = nullptr);
    auto currentState()const{return m_state;};

    void startStateMachine(){m_stateMachine->start();};
    void setBuffer(AVBufferPool *buf){m_buffer=buf;};
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
    AVBufferPool *m_buffer;
    bool m_isWorking;//暂时
    QByteArray m_data;

    bool m_needPic;
    AVCodecContext *m_picEncCtx=nullptr;
    AVPacket *m_picPkt=nullptr;
    AVFrame *m_picFrame=nullptr;
    QString m_picNamePrefix{"监控截图"};
    QFile *m_picFile=nullptr;

    void init();
    void read();
    void destroy();
    void reset();
public slots:
    void respondToProducer(int w,int h,qreal fps);
    void respondToMainDestroy();
    void respondToMainDisconnect();
    void respondToMainScreenShot();
signals:
    void foundInput();//内部状态转移信号，配合respondToProducer()
    void initFinished();
    void readyToRead();
    void turnToDestroy();//内部状态转移信号，配合respondToMainDestroy()
    void turnToReset();//内部状态转移信号，配合respondToMainDisconnect()
    void turnToNoInput();//内部状态转移信号，配合reset()
    void setDisplaySize(int w,int h);
    void sendFrame(QByteArray d);
    void screenShotOK(QString dirPath);
};

#endif // VIDEOCONSUMER_H
