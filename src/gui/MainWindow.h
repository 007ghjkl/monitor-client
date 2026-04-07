#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <QMainWindow>
#include <QThread>
#include <QCloseEvent>
#include <QUrl>
#include <QStandardItemModel>
extern "C"{
#include <libavformat/avformat.h>
}
#include "../core/AVProducer.h"
#include "../core/VideoConsumer.h"
#include "../core/AudioConsumer.h"
#include "../core/OnvifClient.h"
#include "../core/ScreenShooter.h"
#include "../utils/MetaTreeNode.h"
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
    void doSetFullScreen(bool value);
private:
    Ui::MainWindow *ui;
    QUrl m_url;
    void initUiConnections();
    QThread *m_avProducerThread;
    QThread *m_videoConsumerThread;
    QThread *m_audioConsumerThread;
    QThread *m_onvifClientThread;
    QThread *m_screenShooterThread;
    AVProducer *m_avProducer;
    VideoConsumer *m_videoConsumer;
    AudioConsumer *m_audioConsumer;
    OnvifClient *m_onvifClient;
    ScreenShooter *m_screenShooter;
    AVBufferPool *m_videoBuf,*m_audioBuf;

    void fillMetaTreeRecursive(QStandardItem *parent,QSharedPointer<MetaTreeNode> node);
    QStandardItemModel *m_avInfoModel;
    QStandardItemModel *m_onvifInfoModel;

    bool m_isFullScreen;

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
    void notifyFullScreen(bool value);
};
#endif // MAINWINDOW_H
