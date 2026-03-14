#include "tavproducer.h"
#include <QElapsedTimer>

bool TAVProducer::openCodecContext(int *streamIdx, AVCodecContext **decCtx, AVFormatContext *fmtCtx, AVMediaType type)
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

void TAVProducer::init()
{
    // QElapsedTimer timer;
    // timer.start();
    // av_log_set_callback(avLogCallBack);
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
    if(avformat_open_input(&fmtCtx,url.toLocal8Bit().constData(),nullptr,&options)<0)
    {
        qDebug()<<"打开"<<url<<"时出错!";
        QCoreApplication::exit(-1);
    }
    av_dict_free(&options);
    //检索视频流信息
    if(avformat_find_stream_info(fmtCtx,nullptr)<0)
    {
        qDebug()<<"检索视频流信息时出错!";
        QCoreApplication::exit(-1);
    }
    haveVideo=(videoBuf!=nullptr);
    haveAudio=(audioBuf!=nullptr);
    //初始化视频解码器
    if(openCodecContext(&videoStreamIdx,&videoDecCtx,fmtCtx,AVMEDIA_TYPE_VIDEO))
    {
        width=videoDecCtx->width;
        height=videoDecCtx->height;
        fps=(qreal)(videoDecCtx->framerate.num)/(videoDecCtx->framerate.den);
        pixFmt=videoDecCtx->pix_fmt;
        aVideoFrameSize=av_image_alloc(aVideoFrameData,aVideoFrameLinesize,width,height,pixFmt,1);
        if(aVideoFrameSize<0)
        {
            qDebug()<<"分配一帧视频的内存对齐时出错!";
            QCoreApplication::exit(-1);
        }
        if(haveVideo)
        {
            emit foundVideoFormat(width,height,fps);
        }
    }
    //初始化音频解码器
    if(openCodecContext(&audioStreamIdx,&audioDecCtx,fmtCtx,AVMEDIA_TYPE_AUDIO))
    {
        withResample=true;
        QAudioFormat format;
        swrCtx=swr_alloc();
        if(!swrCtx)
        {
            qDebug()<<"分配SwrContext空间时出错！";
            QCoreApplication::exit(-1);
        }
        // qDebug()<<"每帧采样点数:"<<audioDecCtx->frame_size;
        //采样率
        format.setSampleRate(audioDecCtx->sample_rate);
        av_opt_set_int(swrCtx, "in_sample_rate",audioDecCtx->sample_rate,0);
        av_opt_set_int(swrCtx, "out_sample_rate",audioDecCtx->sample_rate,0);
        // qDebug()<<"采样率："<<audioDecCtx->sample_rate;
        //声道数
        int channelCount=audioDecCtx->ch_layout.nb_channels;
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
        av_opt_set_chlayout(swrCtx,"in_chlayout",&(audioDecCtx->ch_layout),0);
        av_opt_set_chlayout(swrCtx,"out_chlayout",&(audioDecCtx->ch_layout),0);
        // qDebug()<<"声道数："<<audioDecCtx->ch_layout.nb_channels;
        //采样格式
        auto raw=audioDecCtx->sample_fmt;
        // qDebug()<<"输入采样格式："<<raw;
        av_opt_set_sample_fmt(swrCtx,"in_sample_fmt",raw,0);
        if(raw==AV_SAMPLE_FMT_U8||raw==AV_SAMPLE_FMT_U8P)
        {
            format.setSampleFormat(QAudioFormat::UInt8);
            av_opt_set_sample_fmt(swrCtx,"out_sample_fmt",AV_SAMPLE_FMT_U8,0);
        }
        else if(raw==AV_SAMPLE_FMT_S16||raw==AV_SAMPLE_FMT_S16P)
        {
            format.setSampleFormat(QAudioFormat::Int16);
            av_opt_set_sample_fmt(swrCtx,"out_sample_fmt",AV_SAMPLE_FMT_S16,0);
        }
        else if(raw==AV_SAMPLE_FMT_S32||raw==AV_SAMPLE_FMT_S32P||raw==AV_SAMPLE_FMT_S64||raw==AV_SAMPLE_FMT_S64P)
        {
            format.setSampleFormat(QAudioFormat::Int32);
            av_opt_set_sample_fmt(swrCtx,"out_sample_fmt",AV_SAMPLE_FMT_S32,0);
        }
        else if(raw==AV_SAMPLE_FMT_FLT||raw==AV_SAMPLE_FMT_FLTP||raw==AV_SAMPLE_FMT_DBL||raw==AV_SAMPLE_FMT_DBLP)
        {
            format.setSampleFormat(QAudioFormat::Float);
            av_opt_set_sample_fmt(swrCtx,"out_sample_fmt",AV_SAMPLE_FMT_FLT,0);
        }
        if(raw==AV_SAMPLE_FMT_U8||raw==AV_SAMPLE_FMT_S16||raw==AV_SAMPLE_FMT_S32||raw==AV_SAMPLE_FMT_FLT)
        {
            withResample=false;
        }
        // qDebug()<<"输出采样格式："<<format.sampleFormat();
        if(swr_init(swrCtx)<0)
        {
            qDebug()<<"SwrContext初始化时出错！";
            QCoreApplication::exit(-1);
        }
        if(haveAudio)
        {
            emit foundAudioFormat(format);
        }

    }
    //打印输入流信息
    av_dump_format(fmtCtx,0,url.toLocal8Bit().constData(),0);
    //分配AVPacket和AVFrame空间
    pkt=av_packet_alloc();
    if(!pkt)
    {
        qDebug()<<"分配AVPacket空间时出错！";
        QCoreApplication::exit(-1);
    }
    frame=av_frame_alloc();
    if(!frame)
    {
        qDebug()<<"分配AVFrame空间时出错！";
        QCoreApplication::exit(-1);
    }
    //调试部分
    qDebug()<<"生产者:初始化完成。";
    isWorking=true;
    emit initFinished();
    // qDebug()<<"生产者init()耗时"<<timer.elapsed();
}

void TAVProducer::read()
{
    int ret=0;
    int index;
    if(av_read_frame(fmtCtx,pkt)>=0)
    {
        index=pkt->stream_index;
        //AVPacket->解码器
        if(index==videoStreamIdx)
        {
            ret=avcodec_send_packet(videoDecCtx,pkt);
        }
        else if(index==audioStreamIdx)
        {
            ret=avcodec_send_packet(audioDecCtx,pkt);
        }
        if(ret<0)
        {
            av_make_error_string(errorStr,AV_ERROR_MAX_STRING_SIZE,ret);
            qDebug()<<"将AVPacket包传入解码器时出现错误:"<<errorStr;
        }
        av_packet_unref(pkt);
        //解码器->AVFrame
        while(ret>=0)
        {
            if(index==videoStreamIdx)
            {
                ret=avcodec_receive_frame(videoDecCtx,frame);
            }
            else if(index==audioStreamIdx)
            {
                ret=avcodec_receive_frame(audioDecCtx,frame);
            }
            if (ret == AVERROR_EOF || ret == AVERROR(EAGAIN))
            {//本包已结束
                break;
            }
            if(ret<0)
            {
                av_make_error_string(errorStr,AV_ERROR_MAX_STRING_SIZE,ret);
                qDebug()<<"将解码器输出传入AVFrame帧时出现错误:"<<errorStr;
            }
            //写入数据
            if(index==videoStreamIdx)
            {
                if(haveVideo)
                {
                    av_image_copy(aVideoFrameData,aVideoFrameLinesize,frame->data,frame->linesize,pixFmt,width,height);
                    videoBuf->produce((const char*)(aVideoFrameData[0]),aVideoFrameSize);
                }
            }
            else if(index==audioStreamIdx)
            {
                if(haveAudio)
                {
                    AVFrame* aFrame=nullptr;
                    // withResample=true;
                    if(withResample)
                    {
                        aFrame=av_frame_alloc();
                        av_opt_get_int(swrCtx,"out_sample_rate",0,(int64_t*)(&(aFrame->sample_rate)));
                        av_opt_get_chlayout(swrCtx,"out_chlayout",0,&(aFrame->ch_layout));
                        av_opt_get_sample_fmt(swrCtx,"out_sample_fmt",0,(AVSampleFormat*)(&(aFrame->format)));
                        ret=swr_convert_frame(swrCtx,aFrame,frame);
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
                        aFrame=frame;
                    }
                    qsizetype lineSize=aFrame->nb_samples*av_get_bytes_per_sample(static_cast<AVSampleFormat>(aFrame->format))*aFrame->ch_layout.nb_channels;
                    audioBuf->produce((const char*)(aFrame->extended_data[0]),lineSize);//线程安全地写
                    if(withResample)
                    {
                        av_frame_free(&aFrame);
                    }
                }
            }
            // av_frame_unref(frame);
        }
    }
    if(isWorking)
    {
        emit readyToRead();
    }
}

void TAVProducer::destroy()
{//确保进入最终状态
    qDebug()<<"生产者:开始销毁...";

    avformat_close_input(&fmtCtx);
    av_packet_free(&pkt);
    av_frame_free(&frame);
    avcodec_free_context(&videoDecCtx);
    if(aVideoFrameData[0])
    {
        av_freep(&aVideoFrameData[0]);
    }
    avcodec_free_context(&audioDecCtx);
    swr_free(&swrCtx);

    if(videoBuf)
    {
        // videoBuf->close();
    }
    if(audioBuf)
    {
        // audioBuf->close();
    }

    stateMachine->stop();
}

void TAVProducer::reset()
{
    avformat_close_input(&fmtCtx);
    av_packet_free(&pkt);
    av_frame_free(&frame);
    avcodec_free_context(&videoDecCtx);
    if(aVideoFrameData[0])
    {
        av_freep(&aVideoFrameData[0]);
    }
    avcodec_free_context(&audioDecCtx);
    swr_free(&swrCtx);
    qDebug()<<"生产者:异步重置完成。";
    if(url.isNull())
    {
        emit turnToNoInput();
    }
}

TAVProducer::TAVProducer(QObject *parent)
    : QObject{parent}
{
    initStateMachine();
}

TAVProducer::~TAVProducer()
{

}

void TAVProducer::initStateMachine()
{
    stateMachine=new QStateMachine(this);
    stateNoInput=new QState(stateMachine);
    stateInit=new QState(stateMachine);
    stateReading=new QState(stateMachine);
    stateDestroy=new QFinalState(stateMachine);
    stateReset=new QState(stateMachine);
    //设定属性
    QVariant stateVal;
    stateVal.setValue(AVProducerState::NoInput);
    stateNoInput->assignProperty(this,"state",stateVal);
    stateVal.setValue(AVProducerState::Init);
    stateInit->assignProperty(this,"state",stateVal);
    stateVal.setValue(AVProducerState::Reading);
    stateReading->assignProperty(this,"state",stateVal);
    stateVal.setValue(AVProducerState::Reset);
    stateReset->assignProperty(this,"state",stateVal);
    //Destroy要自己设置属性
    //设置操作
    connect(stateInit,&QState::entered,this,&TAVProducer::init);
    connect(stateReading,&QState::entered,this,&TAVProducer::read);
    connect(stateDestroy,&QFinalState::entered,this,&TAVProducer::destroy);
    connect(stateReset,&QState::entered,this,&TAVProducer::reset);
    //设置状态转移
    stateNoInput->addTransition(this,&TAVProducer::foundInput,stateInit);
    stateNoInput->addTransition(this,&TAVProducer::turnToDestroy,stateDestroy);
    stateInit->addTransition(this,&TAVProducer::initFinished,stateReading);
    stateInit->addTransition(this,&TAVProducer::turnToDestroy,stateDestroy);
    stateInit->addTransition(this,&TAVProducer::turnToReset,stateReset);
    stateReading->addTransition(this,&TAVProducer::readyToRead,stateReading);
    stateReading->addTransition(this,&TAVProducer::turnToDestroy,stateDestroy);
    stateReading->addTransition(this,&TAVProducer::turnToReset,stateReset);
    stateReset->addTransition(this,&TAVProducer::turnToNoInput,stateNoInput);
    //应用设置
    stateMachine->setInitialState(stateNoInput);
}

void TAVProducer::respondToMainURL(QString url)
{
    if(currentState()==AVProducerState::NoInput)
    {
        this->url=url;
        emit foundInput();
    }
}

void TAVProducer::respondToMainDestroy()
{
    setState(AVProducerState::Destroy);
    isWorking=false;
    emit turnToDestroy();
}

void TAVProducer::respondToMainDisconnect()
{
    isWorking=false;
    url.clear();
    qDebug()<<"生产者:开始断开连接。";
    emit turnToReset();
}


void avLogCallBack(void *ptr, int level, const char *fmt, va_list vl)
{
    if (level > AV_LOG_INFO)
        return;
    char line[256];
    std::vsnprintf(line,256,fmt,vl);
    qDebug()<<line;
}
