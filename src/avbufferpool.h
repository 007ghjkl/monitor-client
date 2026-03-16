#ifndef AVBUFFERPOOL_H
#define AVBUFFERPOOL_H

#include <QIODevice>
#include <QObject>
#include <QMutex>
#include <QWaitCondition>
#include <QThread>
class AVBufferPool : public QIODevice
{
    Q_OBJECT
private:
    bool m_isWorking;
    QMutex m_lock;
    QWaitCondition m_condFull;
    QWaitCondition m_condEmpty;

    qsizetype m_size;
    QByteArray m_buffer;
    qint64 m_readPos;//下次从此位置开始读
    qint64 m_writePos;//下次从此位置开始写
protected:
    qint64 readData(char *data, qint64 maxSize)override;
    qint64 writeData(const char *data, qint64 maxSize)override;
public:
    explicit AVBufferPool(QObject *parent = nullptr);
    ~AVBufferPool();
    qint64 bytesAvailable()const override;
    void init(qsizetype total);
    void produce(const char *data,qint64 len);
    void sleep(int milisecond);
public slots:
    void respondToMainDestroy();
    void respondToMainDisconnect();
};

#endif // AVBUFFERPOOL_H
