#ifndef SCREENSHOOTER_H
#define SCREENSHOOTER_H

#include <QObject>
#include <QFile>
#include <QDir>
#include <QDateTime>
extern "C"{
#include <libavcodec/avcodec.h>
#include <libswscale/swscale.h>
}

class ScreenShooter : public QObject
{
    Q_OBJECT
public:
    explicit ScreenShooter(QObject *parent = nullptr);
    virtual ~ScreenShooter();
private:
    AVCodecContext *m_enCtx=nullptr;
    AVPacket *m_pkt=nullptr;
    AVFrame *m_limitedFrame=nullptr;
    AVFrame *m_fullFrame=nullptr;
    SwsContext *m_swsCtx=nullptr;
    QString m_picNamePrefix{"监控截图"};
    QFile *m_picFile=nullptr;

    int m_width;
    int m_height;

    bool m_isReady;

signals:
    void screenShotOK(QString dirPath);
public slots:
    void respondToProducer(int w,int h);
    void screenShot(QByteArray pic);
    void destroy();
};

#endif // SCREENSHOOTER_H
