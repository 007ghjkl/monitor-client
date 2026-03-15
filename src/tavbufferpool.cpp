#include "tavbufferpool.h"
#include <QDebug>

qint64 TAVBufferPool::readData(char *data, qint64 maxSize)
{
    if(m_isWorking==false)
        return 0;

    m_lock.lock();
    //QAudioSink专用检测
    int available=(m_readPos<=m_writePos)?(m_writePos-m_readPos):(m_writePos-m_readPos+m_size);
    while(available<maxSize)
    {//缓冲区空
        qDebug()<<this<<"缓冲区空";
        m_condEmpty.wait(&m_lock);
        available=(m_readPos<=m_writePos)?(m_writePos-m_readPos):(m_writePos-m_readPos+m_size);
        if(m_isWorking==false)
            break;
    }
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
    qDebug()<<this<<"消费数据:"<<maxSize;
    available=(m_readPos<=m_writePos)?(m_writePos-m_readPos):(m_writePos-m_readPos+m_size);
    while(available<maxSize)
    {//缓冲区空
        qDebug()<<this<<"缓冲区空";
        m_condEmpty.wait(&m_lock);
        available=(m_readPos<=m_writePos)?(m_writePos-m_readPos):(m_writePos-m_readPos+m_size);
        if(m_isWorking==false)
            break;
    }
    m_lock.unlock();
    m_condFull.wakeAll();
    return maxSize;
}

qint64 TAVBufferPool::writeData(const char *data, qint64 maxSize)
{
    if(m_isWorking==false)
        return 0;

    m_lock.lock();
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
    qDebug()<<this<<"生产数据:"<<maxSize;
    int available=(m_writePos<=m_readPos)?(m_readPos-m_writePos):(m_readPos-m_writePos+m_size);
    while(available<maxSize)
    {//缓冲区满
        qDebug()<<this<<"缓冲区满";
        m_condFull.wait(&m_lock);
        available=(m_writePos<=m_readPos)?(m_readPos-m_writePos):(m_readPos-m_writePos+m_size);
        if(m_isWorking==false)
            break;
    }
    m_lock.unlock();
    m_condEmpty.wakeAll();

    emit readyRead();
    emit bytesWritten(maxSize);
    return maxSize;
}

TAVBufferPool::TAVBufferPool(QObject *parent)
    : QIODevice{parent}
{

}

TAVBufferPool::~TAVBufferPool()
{
    // lock.unlock();
}

qint64 TAVBufferPool::bytesAvailable() const
{
    qint64 ret=QIODevice::bytesAvailable();    
    ret+=(m_writePos-m_readPos>0)?(m_writePos-m_readPos):(m_size-m_readPos+m_writePos);
    // qDebug()<<this<<"可读大小:"<<ret;
    return ret;
}

void TAVBufferPool::init(qsizetype total)
{
    m_isWorking=true;
    m_readPos=0;
    m_writePos=0;
    m_size=total;
    m_buffer.resize(m_size);
}

void TAVBufferPool::produce(const char *data, qint64 len)
{
    writeData(data,len);
}

void TAVBufferPool::sleep(int milisecond)
{
    QMutexLocker locker(&m_lock);
    m_condEmpty.wait(&m_lock,milisecond);
    qDebug()<<this<<"睡醒了。";
}

void TAVBufferPool::respondToMainDestroy()
{
    m_isWorking=false;
    // qDebug()<<"avBufs:"<<isWorking;
    m_condEmpty.wakeAll();
    m_condFull.wakeAll();
}

void TAVBufferPool::respondToMainDisconnect()
{
    m_isWorking=false;
    m_condEmpty.wakeAll();
    m_condFull.wakeAll();
    QThread::usleep(100);
    m_condEmpty.wakeAll();
    m_condFull.wakeAll();
    m_buffer.clear();
    m_buffer.resize(m_size);
    m_readPos=m_writePos=0;
    QThread::usleep(100);
    m_isWorking=true;
}
