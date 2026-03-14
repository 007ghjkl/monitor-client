#ifndef TAVBUFFERPOOL_H
#define TAVBUFFERPOOL_H

#include <QIODevice>
#include <QObject>
#include <QReadWriteLock>
#include <QWaitCondition>
#include <QThread>
class TAVBufferPool : public QIODevice
{
    Q_OBJECT
private:
    bool isWorking;
    QReadWriteLock lock;
    QWaitCondition condFull;
    QWaitCondition condEmpty;

    qsizetype size;
    QByteArray buffer;
    qint64 readPos;//下次从此位置开始读
    qint64 writePos;//下次从此位置开始写
protected:
    qint64 readData(char *data, qint64 maxSize)override;
    qint64 writeData(const char *data, qint64 maxSize)override;
public:
    explicit TAVBufferPool(QObject *parent = nullptr);
    ~TAVBufferPool();
    qint64 bytesAvailable()const override;
    void init(qsizetype total);
    void produce(const char *data,qint64 len);
    void sleep(int milisecond);
public slots:
    void respondToMainDestroy();
    void respondToMainDisconnect();
};

#endif // TAVBUFFERPOOL_H
