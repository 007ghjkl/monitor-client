#include "avproducer.h"
#include <QElapsedTimer>

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
        m_aVideoFrameSize=av_image_alloc(m_aVideoFrameData,m_aVideoFrameLinesize,m_width,m_height,m_pixFmt,1);
        if(m_aVideoFrameSize<0)
        {
            qDebug()<<"分配一帧视频的内存对齐时出错!";
            QCoreApplication::exit(-1);
        }
        if(m_haveVideo)
        {
            emit foundVideoFormat(m_width,m_height,m_fps);
        }
    }
    //初始化音频解码器
    if(openCodecContext(&m_audioStreamIdx,&m_audioDecCtx,m_fmtCtx,AVMEDIA_TYPE_AUDIO))
    {
        m_withResample=true;
        QAudioFormat format;
        m_swrCtx=swr_alloc();
        if(!m_swrCtx)
        {
            qDebug()<<"分配SwrContext空间时出错！";
            QCoreApplication::exit(-1);
        }
        // qDebug()<<"每帧采样点数:"<<audioDecCtx->frame_size;
        //采样率
        format.setSampleRate(m_audioDecCtx->sample_rate);
        av_opt_set_int(m_swrCtx, "in_sample_rate",m_audioDecCtx->sample_rate,0);
        av_opt_set_int(m_swrCtx, "out_sample_rate",m_audioDecCtx->sample_rate,0);
        // qDebug()<<"采样率："<<m_audioDecCtx->sample_rate;
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
        av_opt_set_chlayout(m_swrCtx,"in_chlayout",&(m_audioDecCtx->ch_layout),0);
        av_opt_set_chlayout(m_swrCtx,"out_chlayout",&(m_audioDecCtx->ch_layout),0);
        // qDebug()<<"声道数："<<m_audioDecCtx->ch_layout.nb_channels;
        //采样格式
        auto raw=m_audioDecCtx->sample_fmt;
        // qDebug()<<"输入采样格式："<<raw;
        av_opt_set_sample_fmt(m_swrCtx,"in_sample_fmt",raw,0);
        if(raw==AV_SAMPLE_FMT_U8||raw==AV_SAMPLE_FMT_U8P)
        {
            format.setSampleFormat(QAudioFormat::UInt8);
            av_opt_set_sample_fmt(m_swrCtx,"out_sample_fmt",AV_SAMPLE_FMT_U8,0);
        }
        else if(raw==AV_SAMPLE_FMT_S16||raw==AV_SAMPLE_FMT_S16P)
        {
            format.setSampleFormat(QAudioFormat::Int16);
            av_opt_set_sample_fmt(m_swrCtx,"out_sample_fmt",AV_SAMPLE_FMT_S16,0);
        }
        else if(raw==AV_SAMPLE_FMT_S32||raw==AV_SAMPLE_FMT_S32P||raw==AV_SAMPLE_FMT_S64||raw==AV_SAMPLE_FMT_S64P)
        {
            format.setSampleFormat(QAudioFormat::Int32);
            av_opt_set_sample_fmt(m_swrCtx,"out_sample_fmt",AV_SAMPLE_FMT_S32,0);
        }
        else if(raw==AV_SAMPLE_FMT_FLT||raw==AV_SAMPLE_FMT_FLTP||raw==AV_SAMPLE_FMT_DBL||raw==AV_SAMPLE_FMT_DBLP)
        {
            format.setSampleFormat(QAudioFormat::Float);
            av_opt_set_sample_fmt(m_swrCtx,"out_sample_fmt",AV_SAMPLE_FMT_FLT,0);
        }
        if(raw==AV_SAMPLE_FMT_U8||raw==AV_SAMPLE_FMT_S16||raw==AV_SAMPLE_FMT_S32||raw==AV_SAMPLE_FMT_FLT)
        {
            m_withResample=false;
        }
        // qDebug()<<"输出采样格式："<<format.sampleFormat();
        if(swr_init(m_swrCtx)<0)
        {
            qDebug()<<"SwrContext初始化时出错！";
            QCoreApplication::exit(-1);
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
            //写入数据
            if(index==m_videoStreamIdx)
            {
                if(m_haveVideo)
                {
                    av_image_copy(m_aVideoFrameData,m_aVideoFrameLinesize,m_frame->data,m_frame->linesize,m_pixFmt,m_width,m_height);
                    m_videoBuf->produce((const char*)(m_aVideoFrameData[0]),m_aVideoFrameSize);
                }
            }
            else if(index==m_audioStreamIdx)
            {
                if(m_haveAudio)
                {
                    AVFrame* aFrame=nullptr;
                    // withResample=true;
                    if(m_withResample)
                    {
                        aFrame=av_frame_alloc();
                        av_opt_get_int(m_swrCtx,"out_sample_rate",0,(int64_t*)(&(aFrame->sample_rate)));
                        av_opt_get_chlayout(m_swrCtx,"out_chlayout",0,&(aFrame->ch_layout));
                        av_opt_get_sample_fmt(m_swrCtx,"out_sample_fmt",0,(AVSampleFormat*)(&(aFrame->format)));
                        ret=swr_convert_frame(m_swrCtx,aFrame,m_frame);
                        if(ret!=0)
                        {
                            qDebug()<<"重采样AVFrame时出错！";
                        }
                    }
                    else
                    {
                        // ret=av_frame_ref(aFrame,frame);
                        // if(ret<0)
                        // {
                        //     qDebug()<<"设置引用计数时出错!"<<ret;
                        // }
                        aFrame=m_frame;
                    }
                    qsizetype lineSize=aFrame->nb_samples*av_get_bytes_per_sample(static_cast<AVSampleFormat>(aFrame->format))*aFrame->ch_layout.nb_channels;
                    m_audioBuf->produce((const char*)(aFrame->extended_data[0]),lineSize);//线程安全地写
                    if(m_withResample)
                    {
                        av_frame_free(&aFrame);
                    }
                }
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
    avcodec_free_context(&m_videoDecCtx);
    if(m_aVideoFrameData[0])
    {
        av_freep(&m_aVideoFrameData[0]);
    }
    avcodec_free_context(&m_audioDecCtx);
    swr_free(&m_swrCtx);

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
    avcodec_free_context(&m_videoDecCtx);
    if(m_aVideoFrameData[0])
    {
        av_freep(&m_aVideoFrameData[0]);
    }
    avcodec_free_context(&m_audioDecCtx);
    swr_free(&m_swrCtx);
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
