#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <QMainWindow>
#include <QThread>
#include <QCloseEvent>
#include <QUrl>
#include <QStandardItemModel>
#include <QQueue>
#include "../../ui/ui_MainWindow.h"
extern "C"{
#include <libavformat/avformat.h>
}
#include "../core/AVProducer.h"
#include "../core/VideoConsumer.h"
#include "../core/AudioConsumer.h"
#include "../core/OnvifClient.h"
#include "../core/ScreenShooter.h"
#include "../utils/MetaTreeNode.h"
#include "DirectionalPad.h"
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
    void doSetFullScreen(bool value);
    void doRequestAbsMove();
    void doRequestConMove(DirectionRegion r);
    void doRequestStopOrReset(DirectionRegion r);
    void makeUpStreamList(QMap<QString,QUrl> streams);
    void doSwitchStream();
private:
    Ui::MainWindow *ui;
    QLabel *m_labCurrentSyncDiscard;
    QLabel *m_labCurrentSyncDelay;
    QLabel *m_labAvgSyncDiscard;
    QLabel *m_labAvgSyncDelay;
    QMenu *m_streamsMenu;
    QList<QAction*> m_streamsList{};
    QMap<QString,QUrl> m_streamsMap{};

    void initGuiConnections();
    void initCoreConnections();
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
    VideoBufferPool *m_videoBuf;
    AudioBufferPool *m_audioBuf;

    void fillMetaTreeRecursive(QStandardItem *parent,QSharedPointer<MetaTreeNode> node);
    QStandardItemModel *m_avInfoModel;
    QStandardItemModel *m_onvifInfoModel;

    QTimer *m_ptzStatusUpdater;
    QTimer *m_durationDisplayUpdater;
    QQueue<quint8> m_syncDiscardQueue{};
    QQueue<quint16> m_syncDelayMsQueue{};
    quint8 m_currentSyncDiscard{};
    quint16 m_currentSyncDelayMs{};
    quint32 m_sumSyncDiscard{};
    quint32 m_sumSyncDelayMs{};

    void closeEvent(QCloseEvent* event) override;
signals:
    void connectToUrl(QUrl url);
    void destroyAVBufferPools();
    void destroyAVProducer();
    void destroyVideoConsumer();
    void destroyAudioConsumer();
    void notifyScreenShot();
    void changeAVUrl(QUrl url);
    void connectToOnvif(QUrl deviceServiceAddr,QString userName,QString password);
    void notifyFullScreen(bool value);
    void notifyDiscoverDevices();
    void notifyAbsMove(qreal vp,qreal vt,qreal p,qreal t,qreal z);
    void notifyConMove(qreal vp,qreal vt);
    void notifyStopMove();
    void notifySuspend();
};
#endif // MAINWINDOW_H
