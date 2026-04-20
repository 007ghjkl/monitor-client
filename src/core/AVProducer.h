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
#include "AVBufferPool.h"
#include "../utils/MetaTreeNode.h"
extern "C"{
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/imgutils.h>
#include <libavdevice/avdevice.h>
#include <libswresample/swresample.h>
#include <libavutil/log.h>
#include <libavutil/hwcontext.h>
#include <libswscale/swscale.h>
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
    bool m_isWorking;//标示Reading状态下的播放与暂停

    QUrl m_url{};
    VideoBufferPool *m_videoBuf;
    AudioBufferPool *m_audioBuf;

    bool m_haveVideo=false,m_haveAudio=false;
    int m_width=0, m_height=0;
    qreal m_fps;
    AVPixelFormat m_pixFmt;

    bool m_withHwAccel;
    AVBufferRef *m_hwDiveceCtx=nullptr;

    bool m_withScale;
    SwsContext* m_swsCtx=nullptr;
    AVFrame *m_swFrame=nullptr;
    AVFrame *m_scaleFrame=nullptr;

    bool m_withResample;
    SwrContext* m_swrCtx=nullptr;
    AVFrame *m_swrFrame=nullptr;
    AVSampleFormat m_swrSampleFormat;

    int m_videoStreamIdx = -1, m_audioStreamIdx = -1;
    AVFormatContext *m_fmtCtx = nullptr;
    AVCodecContext *m_videoDecCtx = nullptr, *m_audioDecCtx=nullptr;
    AVPacket* m_pkt=nullptr;
    AVFrame* m_frame=nullptr;
    uint8_t *m_imageBuffer=nullptr;
    int m_imageSize;
    char m_errorStr[AV_ERROR_MAX_STRING_SIZE];
    bool openCodecContext(int *streamIdx,AVCodecContext **decCtx, AVFormatContext *fmtCtx, enum AVMediaType type);

    QSharedPointer<MetaTreeNode> m_metaTreeRoot{};
private slots:
    void init();
    void read();
    void destroy();
    void reset();
public:
    explicit AVProducer(QObject *parent = nullptr);
    ~AVProducer();
    void setBuffers(VideoBufferPool *vb,AudioBufferPool *ab){m_videoBuf=vb;m_audioBuf=ab;};
    auto currentState()const{return m_state;};
    void startStateMachine(){m_stateMachine->start();};
public slots:
    void respondToMainUrl(QUrl url);
    void respondToMainDestroy();
    void respondToMainSuspend();
    void respondToMainChangeUrl(QUrl url);
signals:
    void foundInput();//内部状态转移信号，配合respondToMainURL()
    void initFinished();
    void readyToRead();
    void turnToDestroy();//内部状态转移信号，配合respondToMainDestroy()
    void turnToReset();//内部状态转移信号，配合respondToMainDisconnect()
    void resetFinished();
    void foundVideoFormat(int w,int h);
    void foundAudioFormat(QAudioFormat f);
    void activeBuffers();
    void metaTreeDone(QSharedPointer<MetaTreeNode> metaTreeRoot);
};
#endif // AVPRODUCER_H
