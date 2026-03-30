#include "AVProducer.h"
#include <QElapsedTimer>
namespace HWAccel{
#if defined(Q_OS_LINUX)
constexpr AVHWDeviceType deviceType=AV_HWDEVICE_TYPE_VAAPI;
#elif defined(Q_OS_WIN)
constexpr AVHWDeviceType deviceType=AV_HWDEVICE_TYPE_D3D11VA;
#endif
AVPixelFormat pixFmt;
AVPixelFormat hwGetFormat(AVCodecContext *ctx,const enum AVPixelFormat *pix_fmts)
{
    while(*pix_fmts!=pixFmt)
    {
        ++pix_fmts;
        if(AV_PIX_FMT_NONE==*pix_fmts){break;}
    }
    // qDebug()<<*pix_fmts;
    return *pix_fmts;
}
}
bool AVProducer::openCodecContext(int *streamIdx, AVCodecContext **decCtx, AVFormatContext *fmtCtx, AVMediaType type)
{
    //按媒体类型寻找流号
    int streamIndex=av_find_best_stream(fmtCtx,type,-1,-1,nullptr,0);
    if(streamIndex<0)
    {
        qDebug()<<"寻找媒体类型为["<<av_get_media_type_string(type)<<"]的流号时出错！";
        return false;
    }
    *streamIdx = streamIndex;
    AVStream *st;
    st = fmtCtx->streams[streamIndex];
    //按输入流信息中指定的解码器ID找到解码器
    const AVCodec* dec=avcodec_find_decoder(st->codecpar->codec_id);
    if(!dec)
    {
        qDebug()<<"寻找ID为["<<st->codecpar->codec_id<<"]的解码器时出错！";
        return false;
    }
    //给解码器分配上下文空间
    *decCtx = avcodec_alloc_context3(dec);
    if(!*decCtx)
    {
        qDebug()<<"给解码器["<<dec->name<<"]分配上下文时出错！";
        return false;
    }
    //将输入流信息中的配置安装到上下文中
    if(avcodec_parameters_to_context(*decCtx, st->codecpar)<0)
    {
        qDebug()<<"不能正常设置解码器["<<dec->name<<"]的上下文，将使用默认配置。";
    }
    if(AVMEDIA_TYPE_VIDEO==type)
    {
        //配置硬件加速
        m_withHwAccel=true;
        int hwIndex=0;
        const AVCodecHWConfig *hwConfig=nullptr;
        while(true)
        {
            hwConfig=avcodec_get_hw_config(m_videoDecCtx->codec, hwIndex);
            if(hwConfig)
            {
                if(hwConfig->device_type==HWAccel::deviceType&&hwConfig->methods & AV_CODEC_HW_CONFIG_METHOD_HW_DEVICE_CTX)
                {
                    HWAccel::pixFmt=hwConfig->pix_fmt;
                    qDebug()<<"硬件加速类型:"<<HWAccel::deviceType<<",像素格式:"<<HWAccel::pixFmt;
                    break;
                }
            }
            else
            {
                m_withHwAccel=false;
                qDebug()<<"硬件加速配置失败!";
                break;
            }
            ++hwIndex;
        }
        //初始化硬件加速
        if(m_withHwAccel)
        {
            if(av_hwdevice_ctx_create(&m_hwDiveceCtx, HWAccel::deviceType, nullptr, nullptr, 0)<0)
            {
                qDebug()<<"给AVHWDeviceContext初始化时出错!";
                QCoreApplication::exit(-1);
            }
            m_videoDecCtx->get_format=HWAccel::hwGetFormat;
            m_videoDecCtx->hw_device_ctx=av_buffer_ref(m_hwDiveceCtx);
        }
    }
    //启动解码器
    if(avcodec_open2(*decCtx, dec, nullptr)<0)
    {
        qDebug()<<"解码器["<<dec->name<<"]启动时出错！";
        return false;
    }
    return true;
}

void AVProducer::init()
{
    av_log_set_level(AV_LOG_ERROR);
    AVDictionary* options=nullptr;
    // 设置缓存大小
    // av_dict_set(&options, "buffer_size", "2073600", 0);
    //以tcp方式打开
    av_dict_set(&options, "rtsp_transport", "tcp", 0);
    //设置超时断开连接时间,单位微秒
    av_dict_set(&options, "stimeout", "3000000", 0);
    //设置最大时延,单位微秒
    av_dict_set(&options, "max_delay", "3000000", 0);
    //自动开启线程数
    av_dict_set(&options, "threads", "auto", 0);
    //初始化网络流
    avformat_network_init();
    //打开输入
    if(avformat_open_input(&m_fmtCtx,m_url.toString().toUtf8().constData(),nullptr,&options)<0)
    {
        qDebug()<<"打开"<<m_url<<"时出错!";
        QCoreApplication::exit(-1);
    }
    av_dict_free(&options);
    //检索视频流信息
    if(avformat_find_stream_info(m_fmtCtx,nullptr)<0)
    {
        qDebug()<<"检索视频流信息时出错!";
        QCoreApplication::exit(-1);
    }
    m_haveVideo=(m_videoBuf!=nullptr);
    m_haveAudio=(m_audioBuf!=nullptr);
    //初始化视频解码器
    if(openCodecContext(&m_videoStreamIdx,&m_videoDecCtx,m_fmtCtx,AVMEDIA_TYPE_VIDEO))
    {
        m_width=m_videoDecCtx->width;
        m_height=m_videoDecCtx->height;
        m_fps=(qreal)(m_videoDecCtx->framerate.num)/(m_videoDecCtx->framerate.den);
        m_pixFmt=m_videoDecCtx->pix_fmt;
        //分配一帧空间
        m_imageSize=av_image_get_buffer_size(AV_PIX_FMT_YUV420P,m_width,m_height,1);
        m_imageBuffer=(uint8_t *)av_malloc(m_imageSize);
        if(!m_imageBuffer)
        {
            qDebug()<<"分配一帧对齐空间时出错!";
            QCoreApplication::exit(-1);
        }
        //像素格式转换
        m_withScale=m_withHwAccel||m_pixFmt!=AV_PIX_FMT_YUV420P;
        if(m_withScale)
        {
            m_swsCtx=sws_alloc_context();
            m_tmpFrame=av_frame_alloc();
            m_swsFrame=av_frame_alloc();
            if(!m_swsCtx||!m_swsFrame||!m_tmpFrame)
            {
                qDebug()<<"初始化ScaleContext和转换帧时出错!";
                QCoreApplication::exit(-1);
            }
        }
        if(m_haveVideo)
        {
            emit foundVideoFormat(m_width,m_height,m_fps);
        }
    }
    //初始化音频解码器
    if(openCodecContext(&m_audioStreamIdx,&m_audioDecCtx,m_fmtCtx,AVMEDIA_TYPE_AUDIO))
    {
        QAudioFormat format;
        m_withResample=true;
        //采样率
        format.setSampleRate(m_audioDecCtx->sample_rate);
        //声道数
        int channelCount=m_audioDecCtx->ch_layout.nb_channels;
        switch(channelCount)
        {
        case 1:
            format.setChannelConfig(QAudioFormat::ChannelConfigMono);
            break;
        case 2:
            format.setChannelConfig(QAudioFormat::ChannelConfigStereo);
            break;
        default:
            format.setChannelConfig(QAudioFormat::ChannelConfigUnknown);
            break;
        }
        //采样格式
        auto raw=m_audioDecCtx->sample_fmt;
        if(raw==AV_SAMPLE_FMT_U8||raw==AV_SAMPLE_FMT_U8P)
        {
            format.setSampleFormat(QAudioFormat::UInt8);
            m_swrSampleFormat=AV_SAMPLE_FMT_U8;
        }
        else if(raw==AV_SAMPLE_FMT_S16||raw==AV_SAMPLE_FMT_S16P)
        {
            format.setSampleFormat(QAudioFormat::Int16);
            m_swrSampleFormat=AV_SAMPLE_FMT_S16;
        }
        else if(raw==AV_SAMPLE_FMT_S32||raw==AV_SAMPLE_FMT_S32P||raw==AV_SAMPLE_FMT_S64||raw==AV_SAMPLE_FMT_S64P)
        {
            format.setSampleFormat(QAudioFormat::Int32);
            m_swrSampleFormat=AV_SAMPLE_FMT_S32;
        }
        else if(raw==AV_SAMPLE_FMT_FLT||raw==AV_SAMPLE_FMT_FLTP||raw==AV_SAMPLE_FMT_DBL||raw==AV_SAMPLE_FMT_DBLP)
        {
            format.setSampleFormat(QAudioFormat::Float);
            m_swrSampleFormat=AV_SAMPLE_FMT_FLT;
        }
        //重采样
        if(raw==AV_SAMPLE_FMT_U8||raw==AV_SAMPLE_FMT_S16||raw==AV_SAMPLE_FMT_S32||raw==AV_SAMPLE_FMT_FLT)
        {
            m_withResample=false;
        }
        if(m_withResample)
        {
            m_swrCtx=swr_alloc();
            m_swrFrame=av_frame_alloc();
            if(!m_swrCtx||!m_swrFrame)
            {
                qDebug()<<"初始化ResampleContext和转换帧时出错!";
                QCoreApplication::exit(-1);
            }
        }
        if(m_haveAudio)
        {
            emit foundAudioFormat(format);
        }
    }
    av_log_set_level(AV_LOG_DEBUG);
    //打印输入流信息
    av_dump_format(m_fmtCtx,0,m_url.toString().toUtf8().constData(),0);
    av_log_set_level(AV_LOG_ERROR);
    //分配AVPacket和AVFrame空间
    m_pkt=av_packet_alloc();
    if(!m_pkt)
    {
        qDebug()<<"分配AVPacket空间时出错！";
        QCoreApplication::exit(-1);
    }
    m_frame=av_frame_alloc();
    if(!m_frame)
    {
        qDebug()<<"分配AVFrame空间时出错！";
        QCoreApplication::exit(-1);
    }
    //调试部分
    qDebug()<<"生产者:初始化完成。";
    m_isWorking=true;
    emit initFinished();
}

void AVProducer::read()
{
    int ret=0;
    int index;
    if(av_read_frame(m_fmtCtx,m_pkt)>=0)
    {
        index=m_pkt->stream_index;
        //AVPacket->解码器
        if(index==m_videoStreamIdx)
        {
            ret=avcodec_send_packet(m_videoDecCtx,m_pkt);
        }
        else if(index==m_audioStreamIdx)
        {
            ret=avcodec_send_packet(m_audioDecCtx,m_pkt);
        }
        if(ret<0)
        {
            av_make_error_string(m_errorStr,AV_ERROR_MAX_STRING_SIZE,ret);
            qDebug()<<"将AVPacket包传入解码器时出现错误:"<<m_errorStr;
        }
        av_packet_unref(m_pkt);
        //解码器->AVFrame
        while(ret>=0)
        {
            if(index==m_videoStreamIdx)
            {
                ret=avcodec_receive_frame(m_videoDecCtx,m_frame);
            }
            else if(index==m_audioStreamIdx)
            {
                ret=avcodec_receive_frame(m_audioDecCtx,m_frame);
            }
            if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
            {//本包已结束
                break;
            }
            if(ret<0)
            {
                av_make_error_string(m_errorStr,AV_ERROR_MAX_STRING_SIZE,ret);
                qDebug()<<"将解码器输出传入AVFrame帧时出现错误:"<<m_errorStr;
            }
            //一帧视频
            if(index==m_videoStreamIdx&&m_haveVideo)
            {
                if(m_withHwAccel)
                {
                    ret=av_hwframe_transfer_data(m_tmpFrame,m_frame,0);
                    if(ret<0)
                    {
                        qDebug()<<"将显存数据转移到内存时出错!";
                    }
                }
                else
                {
                    m_tmpFrame=m_frame;
                }
                if(m_withScale)
                {
                    m_swsFrame->width=m_width;
                    m_swsFrame->height=m_height;
                    m_swsFrame->format=AV_PIX_FMT_YUV420P;
                    ret=sws_scale_frame(m_swsCtx,m_swsFrame,m_tmpFrame);
                    if(ret<0)
                    {
                        qDebug()<<"转换像素格式时出错!";
                    }
                }
                else
                {
                    m_swsFrame=m_tmpFrame;
                }
                av_image_copy_to_buffer(m_imageBuffer,m_imageSize,m_swsFrame->data,m_swsFrame->linesize,AV_PIX_FMT_YUV420P,m_width,m_height,1);
                m_videoBuf->produce((const char*)m_imageBuffer,m_imageSize);
            }
            //一帧音频
            else if(index==m_audioStreamIdx&&m_haveAudio)
            {
                if(m_withResample)
                {
                    m_swrFrame->sample_rate=m_frame->sample_rate;
                    m_swrFrame->ch_layout=m_frame->ch_layout;
                    m_swrFrame->format=m_swrSampleFormat;
                    swr_config_frame(m_swrCtx,m_swrFrame,m_frame);
                    swr_convert_frame(m_swrCtx,m_swrFrame,m_frame);
                }
                else
                {
                    m_swrFrame=m_frame;
                }
                qsizetype size=m_swrFrame->nb_samples*av_get_bytes_per_sample(static_cast<AVSampleFormat>(m_swrFrame->format));
                m_audioBuf->produce((const char*)(m_swrFrame->extended_data[0]),size);
            }
            // av_frame_unref(frame);
        }
    }
    if(m_isWorking)
    {
        emit readyToRead();
    }
}

void AVProducer::destroy()
{//确保进入最终状态
    qDebug()<<"生产者:开始销毁...";

    avformat_close_input(&m_fmtCtx);
    av_packet_free(&m_pkt);
    av_frame_free(&m_frame);
    av_frame_free(&m_tmpFrame);
    av_frame_free(&m_swsFrame);
    av_frame_free(&m_swrFrame);
    avcodec_free_context(&m_videoDecCtx);
    av_buffer_unref(&m_hwDiveceCtx);
    avcodec_free_context(&m_audioDecCtx);
    swr_free(&m_swrCtx);
    sws_free_context(&m_swsCtx);

    if(m_videoBuf)
    {
        // videoBuf->close();
    }
    if(m_audioBuf)
    {
        // audioBuf->close();
    }

    m_stateMachine->stop();
}

void AVProducer::reset()
{
    avformat_close_input(&m_fmtCtx);
    av_packet_free(&m_pkt);
    av_frame_free(&m_frame);
    av_frame_free(&m_tmpFrame);
    av_frame_free(&m_swsFrame);
    av_frame_free(&m_swrFrame);
    avcodec_free_context(&m_videoDecCtx);
    av_buffer_unref(&m_hwDiveceCtx);
    avcodec_free_context(&m_audioDecCtx);
    swr_free(&m_swrCtx);
    sws_free_context(&m_swsCtx);
    qDebug()<<"生产者:异步重置完成。";
    if(m_url.isEmpty())
    {
        emit turnToNoInput();
    }
}

AVProducer::AVProducer(QObject *parent)
    : QObject{parent}
{
    initStateMachine();
}

AVProducer::~AVProducer()
{

}

void AVProducer::initStateMachine()
{
    m_stateMachine=new QStateMachine(this);
    m_stateNoInput=new QState(m_stateMachine);
    m_stateInit=new QState(m_stateMachine);
    m_stateReading=new QState(m_stateMachine);
    m_stateDestroy=new QFinalState(m_stateMachine);
    m_stateReset=new QState(m_stateMachine);
    //设定属性
    QVariant stateVal;
    stateVal.setValue(AVProducerState::NoInput);
    m_stateNoInput->assignProperty(this,"state",stateVal);
    stateVal.setValue(AVProducerState::Init);
    m_stateInit->assignProperty(this,"state",stateVal);
    stateVal.setValue(AVProducerState::Reading);
    m_stateReading->assignProperty(this,"state",stateVal);
    stateVal.setValue(AVProducerState::Reset);
    m_stateReset->assignProperty(this,"state",stateVal);
    //Destroy要自己设置属性
    //设置操作
    connect(m_stateInit,&QState::entered,this,&AVProducer::init);
    connect(m_stateReading,&QState::entered,this,&AVProducer::read);
    connect(m_stateDestroy,&QFinalState::entered,this,&AVProducer::destroy);
    connect(m_stateReset,&QState::entered,this,&AVProducer::reset);
    //设置状态转移
    m_stateNoInput->addTransition(this,&AVProducer::foundInput,m_stateInit);
    m_stateNoInput->addTransition(this,&AVProducer::turnToDestroy,m_stateDestroy);
    m_stateInit->addTransition(this,&AVProducer::initFinished,m_stateReading);
    m_stateInit->addTransition(this,&AVProducer::turnToDestroy,m_stateDestroy);
    m_stateInit->addTransition(this,&AVProducer::turnToReset,m_stateReset);
    m_stateReading->addTransition(this,&AVProducer::readyToRead,m_stateReading);
    m_stateReading->addTransition(this,&AVProducer::turnToDestroy,m_stateDestroy);
    m_stateReading->addTransition(this,&AVProducer::turnToReset,m_stateReset);
    m_stateReset->addTransition(this,&AVProducer::turnToNoInput,m_stateNoInput);
    //应用设置
    m_stateMachine->setInitialState(m_stateNoInput);
}

void AVProducer::respondToMainURL(QUrl url)
{
    if(currentState()==AVProducerState::NoInput)
    {
        m_url=url;
        emit foundInput();
    }
}

void AVProducer::respondToMainDestroy()
{
    setState(AVProducerState::Destroy);
    m_isWorking=false;
    emit turnToDestroy();
}

void AVProducer::respondToMainDisconnect()
{
    m_isWorking=false;
    m_url.clear();
    qDebug()<<"生产者:开始断开连接。";
    emit turnToReset();
}
