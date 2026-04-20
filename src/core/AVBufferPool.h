#ifndef AVBUFFERPOOL_H
#define AVBUFFERPOOL_H

#include <QIODevice>
#include <QObject>
#include <QMutex>
#include <QWaitCondition>
#include <QThread>
#include <QAudioFormat>
class AVBufferPool : public QIODevice
{
    Q_OBJECT
protected:
    qint64 writeData(const char *data, qint64 maxSize)override;
    bool m_isWorking;
    QWaitCondition m_condFull;
    QByteArray m_buffer;
    qsizetype m_size;
    QMutex m_lock;
    qint64 m_readPos;//下次从此位置开始读
    qint64 m_writePos;//下次从此位置开始写
    quint64 m_readAvailable;
public:
    explicit AVBufferPool(QObject *parent = nullptr);
    virtual ~AVBufferPool();
    qint64 bytesAvailable()const override;
    void init(qsizetype total);
public slots:
    void respondToMainDestroy();
    void resetBuffer();
    void respondToProducer();
};

class AudioBufferPool:public AVBufferPool
{
    Q_OBJECT
public:
    explicit AudioBufferPool(QObject *parent=nullptr);
    virtual ~AudioBufferPool()=default;
protected:
    qint64 readData(char *data,qint64 maxSize)override;
    qint64 writeData(const char *data,qint64 maxSize)override;
private:
    qreal m_cacheTime;
    qreal m_renderTime;
    quint32 m_bytesPerSecond;
signals:
    void notifyCacheTime(qreal cacheTime);
    void notifyRenderTime(qreal renderTime);
public slots:
    void respondToProducer(QAudioFormat f);
};

class VideoBufferPool:public AVBufferPool
{
    Q_OBJECT
public:
    explicit VideoBufferPool(QObject *parent=nullptr);
    virtual ~VideoBufferPool()=default;
    void setThrehold(short ms);
protected:
    qint64 readData(char *data,qint64 maxSize)override;
private:
    qreal m_renderTime;
    qreal m_syncTime;
    qreal m_timeBase;//帧率的倒数
    quint64 m_pts;
    short m_threholdMs;
signals:
    void notifySyncDiscard();
    void notifySyncDelay(quint32 delayMs);
public slots:
    void updateSyncTime(qreal syncTime);
};
#endif // AVBUFFERPOOL_H
