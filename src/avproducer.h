#ifndef AVPRODUCER_H
#define AVPRODUCER_H
#include <QObject>
#include <QVariant>
#include <QUrl>
#include <QCoreApplication>
#include <QMessageBox>
#include <QStateMachine>
#include <QFinalState>
#include <QAudioSink>
#include "avbufferpool.h"
extern "C"{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavdevice/avdevice.h>
#include <libswresample/swresample.h>
#include <libavutil/log.h>
}
enum class AVProducerState{NoInput,Init,Reading,Destroy,Reset};
// Q_DECLARE_METATYPE(AVProducerState)
class AVProducer : public QObject
{
    Q_OBJECT
    Q_PROPERTY(AVProducerState state MEMBER m_state READ currentState WRITE setState)
private:
    AVProducerState m_state;
    QStateMachine *m_stateMachine;
    QState *m_stateNoInput;
    QState *m_stateInit;
    QState *m_stateReading;
    QFinalState *m_stateDestroy;
    QState *m_stateReset;
    void setState(AVProducerState s){this->m_state=s;};
    void initStateMachine();

    QUrl m_url;
    QString m_userName;
    QString m_password;
    AVBufferPool *m_videoBuf;
    AVBufferPool *m_audioBuf;

    bool m_haveVideo=false,m_haveAudio=false;
    int m_width=0, m_height=0;
    qreal m_fps;
    AVPixelFormat m_pixFmt;
    uint8_t *m_aVideoFrameData[4]={nullptr};
    int m_aVideoFrameLinesize[4];
    int m_aVideoFrameSize;
    bool m_withResample;
    bool m_isWorking;

    int m_videoStreamIdx = -1, m_audioStreamIdx = -1;
    AVFormatContext *m_fmtCtx = nullptr;
    AVCodecContext *m_videoDecCtx = nullptr, *m_audioDecCtx=nullptr;
    AVPacket* m_pkt=nullptr;
    AVFrame* m_frame=nullptr;
    SwrContext* m_swrCtx=nullptr;
    char m_errorStr[AV_ERROR_MAX_STRING_SIZE];
    bool openCodecContext(int *streamIdx,AVCodecContext **decCtx, AVFormatContext *fmtCtx, enum AVMediaType type);
private slots:
    void init();
    void read();
    void destroy();
    void reset();
public:
    explicit AVProducer(QObject *parent = nullptr);
    ~AVProducer();
    void setBuffers(AVBufferPool *vb,AVBufferPool *ab){m_videoBuf=vb;m_audioBuf=ab;};
    auto currentState()const{return m_state;};
    void startStateMachine(){m_stateMachine->start();};
public slots:
    void respondToMainURL(QUrl url);
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
#endif // AVPRODUCER_H
