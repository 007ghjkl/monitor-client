#include "tavbufferpool.h"
#include <QDebug>

qint64 TAVBufferPool::readData(char *data, qint64 maxSize)
{
    if(isWorking==false)
        return 0;

    lock.lockForRead();
    //QAudioSink专用检测
    int available=(readPos<=writePos)?(writePos-readPos):(writePos-readPos+size);
    while(available<maxSize)
    {//缓冲区空
        qDebug()<<this<<"缓冲区空";
        condEmpty.wait(&lock);
        available=(readPos<=writePos)?(writePos-readPos):(writePos-readPos+size);
        if(isWorking==false)
            break;
    }
    bool reachEnd=(readPos+maxSize>size-1);
    if(reachEnd)
    {
        memcpy(data,buffer.data()+readPos,size-readPos);
        memcpy(data+size-readPos,buffer.data(),maxSize-(size-readPos));
        readPos=maxSize-(size-readPos);
    }
    else
    {
        memcpy(data,buffer.data()+readPos,maxSize);
        readPos+=maxSize;
    }
    qDebug()<<this<<"消费数据:"<<maxSize;
    available=(readPos<=writePos)?(writePos-readPos):(writePos-readPos+size);
    while(available<maxSize)
    {//缓冲区空
        qDebug()<<this<<"缓冲区空";
        condEmpty.wait(&lock);
        available=(readPos<=writePos)?(writePos-readPos):(writePos-readPos+size);
        if(isWorking==false)
            break;
    }
    lock.unlock();
    condFull.wakeAll();
    return maxSize;
}

qint64 TAVBufferPool::writeData(const char *data, qint64 maxSize)
{
    if(isWorking==false)
        return 0;

    lock.lockForWrite();
    bool reachEnd=(writePos+maxSize>size-1);
    if(reachEnd)
    {
        memcpy(buffer.data()+writePos,data,size-writePos);
        memcpy(buffer.data(),data+size-writePos,maxSize-(size-writePos));
        writePos+=maxSize-size;
    }
    else
    {
        memcpy(buffer.data()+writePos,data,maxSize);
        writePos+=maxSize;
    }
    qDebug()<<this<<"生产数据:"<<maxSize;
    int available=(writePos<=readPos)?(readPos-writePos):(readPos-writePos+size);
    while(available<maxSize)
    {//缓冲区满
        qDebug()<<this<<"缓冲区满";
        condFull.wait(&lock);
        available=(writePos<=readPos)?(readPos-writePos):(readPos-writePos+size);
        if(isWorking==false)
            break;
    }
    lock.unlock();
    condEmpty.wakeAll();

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
    ret+=(writePos-readPos>0)?(writePos-readPos):(size-readPos+writePos);
    // qDebug()<<this<<"可读大小:"<<ret;
    return ret;
}

void TAVBufferPool::init(qsizetype total)
{
    isWorking=true;
    readPos=0;
    writePos=0;
    size=total;
    buffer.resize(size);
}

void TAVBufferPool::produce(const char *data, qint64 len)
{
    writeData(data,len);
}

void TAVBufferPool::sleep(int milisecond)
{
    QReadLocker locker(&lock);
    condEmpty.wait(&lock,milisecond);
    qDebug()<<this<<"睡醒了。";
}

void TAVBufferPool::respondToMainDestroy()
{
    isWorking=false;
    // qDebug()<<"avBufs:"<<isWorking;
    condEmpty.wakeAll();
    condFull.wakeAll();
}

void TAVBufferPool::respondToMainDisconnect()
{
    isWorking=false;
    condEmpty.wakeAll();
    condFull.wakeAll();
    QThread::usleep(100);
    condEmpty.wakeAll();
    condFull.wakeAll();
    buffer.clear();
    buffer.resize(size);
    readPos=writePos=0;
    QThread::usleep(100);
    isWorking=true;
}
