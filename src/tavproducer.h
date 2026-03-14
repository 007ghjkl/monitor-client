#ifndef TAVPRODUCER_H
#define TAVPRODUCER_H
#include <QObject>
#include <QVariant>
#include <QCoreApplication>
#include <QMessageBox>
#include <QStateMachine>
#include <QFinalState>
#include <QAudioSink>
#include "tavbufferpool.h"
extern "C"{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavdevice/avdevice.h>
#include <libswresample/swresample.h>
}
enum class AVProducerState{NoInput,Init,Reading,Destroy,Reset};
// Q_DECLARE_METATYPE(AVProducerState)
class TAVProducer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(AVProducerState state MEMBER state READ currentState WRITE setState)
private:
    AVProducerState state;
    QStateMachine *stateMachine;
    QState *stateNoInput;
    QState *stateInit;
    QState *stateReading;
    QFinalState *stateDestroy;
    QState *stateReset;
    void setState(AVProducerState s){this->state=s;};
    void initStateMachine();

    QString url;
    TAVBufferPool *videoBuf;
    TAVBufferPool *audioBuf;
    qreal delayTime;

    bool haveVideo=false,haveAudio=false;
    int width=0, height=0;
    qreal fps;
    AVPixelFormat pixFmt;
    uint8_t *aVideoFrameData[4]={nullptr};
    int aVideoFrameLinesize[4];
    int aVideoFrameSize;
    bool withResample;
    bool isWorking;

    int videoStreamIdx = -1, audioStreamIdx = -1;
    AVFormatContext *fmtCtx = nullptr;
    AVCodecContext *videoDecCtx = nullptr, *audioDecCtx=nullptr;
    AVPacket* pkt=nullptr;
    AVFrame* frame=nullptr;
    SwrContext* swrCtx=nullptr;
    char errorStr[AV_ERROR_MAX_STRING_SIZE];
    bool openCodecContext(int *streamIdx,AVCodecContext **decCtx, AVFormatContext *fmtCtx, enum AVMediaType type);
private slots:
    void init();
    void read();
    void destroy();
    void reset();
public:
    explicit TAVProducer(QObject *parent = nullptr);
    ~TAVProducer();
    void setBuffers(TAVBufferPool *vb,TAVBufferPool *ab){videoBuf=vb;audioBuf=ab;};
    auto currentState()const{return state;};
    void startStateMachine(){stateMachine->start();};
public slots:
    void respondToMainURL(QString url);
    void respondToMainDestroy();
    void respondToMainDisconnect();
signals:
    void foundInput();//内部状态转移信号，配合respondToMainURL()
    void initFinished();
    void readyToRead();
    void turnToDestroy();//内部状态转移信号，配合respondToMainDestroy()
    void turnToReset();//内部状态转移信号，配合respondToMainDisconnect()
    void turnToNoInput();//内部状态转移信号，配合reset()
    void foundVideoFormat(int w,int h,qreal fps);
    void foundAudioFormat(QAudioFormat f);
};
void avLogCallBack(void* ptr, int level, const char* fmt, va_list vl);
#endif // TAVPRODUCER_H
