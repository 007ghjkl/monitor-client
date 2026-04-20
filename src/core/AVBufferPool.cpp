#include "AVBufferPool.h"
#include <QDebug>
// #define BUFFER_DEBUG

qint64 AVBufferPool::writeData(const char *data, qint64 maxSize)
{
    m_lock.lock();
    if(!m_isWorking)
    {
        m_lock.unlock();
        return 0;
    }
    //如果转一圈正好首尾相接，那就直接覆盖
    int available=(m_writePos<m_readPos)?(m_readPos-m_writePos):(m_readPos-m_writePos+m_size);
    while(available<maxSize)
    {//缓冲区满
#ifdef BUFFER_DEBUG
        qDebug()<<this<<"缓冲区满";
#endif
        m_condFull.wait(&m_lock);
        available=(m_writePos<m_readPos)?(m_readPos-m_writePos):(m_readPos-m_writePos+m_size);
        if(!m_isWorking)
        {
            m_lock.unlock();
            return 0;
        }
    }
    bool reachEnd=(m_writePos+maxSize>m_size-1);
    if(reachEnd)
    {
        memcpy(m_buffer.data()+m_writePos,data,m_size-m_writePos);
        memcpy(m_buffer.data(),data+m_size-m_writePos,maxSize-(m_size-m_writePos));
        m_writePos+=maxSize-m_size;
    }
    else
    {
        memcpy(m_buffer.data()+m_writePos,data,maxSize);
        m_writePos+=maxSize;
    }
#ifdef BUFFER_DEBUG
    qDebug()<<this<<"生产数据:"<<maxSize;
#endif
    m_readAvailable=(m_readPos<=m_writePos)?(m_writePos-m_readPos):(m_writePos-m_readPos+m_size);
    m_lock.unlock();

    emit readyRead();
    emit bytesWritten(maxSize);
    return maxSize;
}

AVBufferPool::AVBufferPool(QObject *parent)
    : QIODevice{parent}
{

}

AVBufferPool::~AVBufferPool()
{
}

qint64 AVBufferPool::bytesAvailable() const
{
    qint64 ret=QIODevice::bytesAvailable();    
    ret+=m_readAvailable;
    return ret;
}

void AVBufferPool::init(qsizetype total)
{
    m_readPos=0;
    m_writePos=0;
    m_size=total;
    m_buffer.resize(m_size);
}

void AVBufferPool::respondToMainDestroy()
{
    m_condFull.wakeAll();

    m_lock.tryLock(100);
    m_isWorking=false;
    m_lock.unlock();
}

void AVBufferPool::resetBuffer()
{
    m_condFull.wakeAll();

    m_lock.tryLock(100);
    m_isWorking=false;
    m_readPos=m_writePos=0;
    m_lock.unlock();
}

void AVBufferPool::respondToProducer()
{
    m_lock.lock();
    m_isWorking=true;
    m_lock.unlock();
}

AudioBufferPool::AudioBufferPool(QObject *parent)
    :m_cacheTime(0),m_renderTime(0),m_bytesPerSecond(0)
{

}

qint64 AudioBufferPool::readData(char *data, qint64 maxSize)
{
    m_lock.lock();
    if(!m_isWorking)
    {
        m_lock.unlock();
        return 0;
    }
    if(0==maxSize)
    {
#ifdef BUFFER_DEBUG
        qDebug()<<this<<"尝试消费空数据";
#endif
        m_lock.unlock();
        return 0;
    }
    qint64 bytes=maxSize;
    //音频可以切分
    if(m_readAvailable<bytes)
    {
        bytes=m_readAvailable;
    }
    bool reachEnd=(m_readPos+bytes>m_size-1);
    if(reachEnd)
    {
        memcpy(data,m_buffer.data()+m_readPos,m_size-m_readPos);
        memcpy(data+m_size-m_readPos,m_buffer.data(),bytes-(m_size-m_readPos));
        m_readPos=bytes-(m_size-m_readPos);
    }
    else
    {
        memcpy(data,m_buffer.data()+m_readPos,bytes);
        m_readPos+=bytes;
    }
#ifdef BUFFER_DEBUG
    qDebug()<<this<<"消费数据:"<<bytes;
#endif
    m_readAvailable=(m_readPos<=m_writePos)?(m_writePos-m_readPos):(m_writePos-m_readPos+m_size);
    m_lock.unlock();
    m_condFull.wakeAll();

    m_renderTime+=bytes*1.0f/m_bytesPerSecond;
    emit notifyRenderTime(m_renderTime);
    //qDebug()<<"renderTime:"<<m_renderTime<<"加了"<<bytes*1.0f/m_bytesPerSecond;
    return bytes;
}

qint64 AudioBufferPool::writeData(const char *data, qint64 maxSize)
{
    qint64 bytes=AVBufferPool::writeData(data,maxSize);
    m_cacheTime+=bytes*1.0f/m_bytesPerSecond;
    emit notifyCacheTime(m_cacheTime);
    //qDebug()<<"cacheTime:"<<m_cacheTime<<"加了"<<bytes*1.0f/m_bytesPerSecond;
    return bytes;
}

void AudioBufferPool::respondToProducer(QAudioFormat f)
{
    m_bytesPerSecond=f.sampleRate()*f.bytesPerSample()*f.channelCount();
}

VideoBufferPool::VideoBufferPool(QObject *parent)
    :m_renderTime(0),m_timeBase(0),m_pts(0),m_threholdMs(0)
{

}

void VideoBufferPool::setThrehold(short ms)
{
    m_threholdMs=ms;
}

qint64 VideoBufferPool::readData(char *data, qint64 maxSize)
{
    int diffMs;
    while(qAbs(diffMs=(m_renderTime-m_syncTime)*1000)>m_threholdMs)
    {
        if(diffMs>0)
        {//视频超前，延时
            emit notifySyncDelay(diffMs);
            m_renderTime+=diffMs*1000.0f;
            qDebug()<<"延时"<<diffMs;
            QThread::msleep(diffMs);
        }
        else if(diffMs<0)
        {//视频落后
            QMutexLocker locker(&m_lock);
            if(m_readAvailable<2*maxSize)
            {//无法丢帧
                break;
            }
            emit notifySyncDiscard();
            qDebug()<<"丢一帧";
            m_readPos+=maxSize;
            m_readAvailable=(m_readPos<=m_writePos)?(m_writePos-m_readPos):(m_writePos-m_readPos+m_size);
            ++m_pts;
            m_renderTime=m_pts*m_timeBase;
        }
    }
    m_lock.lock();
    if(!m_isWorking)
    {
        m_lock.unlock();
        return 0;
    }
    //视频以readyRead()作通知，保证缓冲区不会空
    bool reachEnd=(m_readPos+maxSize>m_size-1);
    if(reachEnd)
    {
        memcpy(data,m_buffer.data()+m_readPos,m_size-m_readPos);
        memcpy(data+m_size-m_readPos,m_buffer.data(),maxSize-(m_size-m_readPos));
        m_readPos=maxSize-(m_size-m_readPos);
    }
    else
    {
        memcpy(data,m_buffer.data()+m_readPos,maxSize);
        m_readPos+=maxSize;
    }
#ifdef BUFFER_DEBUG
    qDebug()<<this<<"消费数据:"<<maxSize;
#endif
    m_readAvailable=(m_readPos<=m_writePos)?(m_writePos-m_readPos):(m_writePos-m_readPos+m_size);
    m_lock.unlock();
    m_condFull.wakeAll();

    ++m_pts;
    m_renderTime=m_pts*m_timeBase;
    return maxSize;
}

void VideoBufferPool::updateSyncTime(qreal syncTime)
{
    m_syncTime=syncTime;
}
