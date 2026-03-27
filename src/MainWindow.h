#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <QMainWindow>
#include <QThread>
#include <QCloseEvent>
#include <QUrl>
extern "C"{
#include <libavformat/avformat.h>
}
#include "AVProducer.h"
#include "VideoConsumer.h"
#include "AudioConsumer.h"
#include "OnvifClient.h"
QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void doConnectToUrl();
    void doDisconnect();

private:
    Ui::MainWindow *ui;
    QUrl m_url;
    void initUiConnections();
    QThread *m_avProducerThread;
    QThread *m_videoConsumerThread;
    QThread *m_audioConsumerThread;
    QThread *m_onvifClientThread;
    AVProducer *m_avProducer;
    VideoConsumer *m_videoConsumer;
    AudioConsumer *m_audioConsumer;
    OnvifClient *m_onvifClient;
    AVBufferPool *m_videoBuf,*m_audioBuf;

    void closeEvent(QCloseEvent* event) override;
signals:
    void connectToURL(QUrl url);
    void destroyAVBufferPools();
    void destroyAVProducer();
    void destroyVideoConsumer();
    void destroyAudioConsumer();
    void notifyDisconnect();
    void notifyScreenShot();
    void connectToOnvif(QUrl deviceServiceAddr);
};
#endif // MAINWINDOW_H
