#include "MainWindow.h"

#include "DialogConnectToOnvif.h"
// #include <QScopedPointer>
#include <QSystemTrayIcon>
extern "C" {
#include <libavcodec/avcodec.h>
}
#include <QDebug>
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    m_labCurrentSyncDiscard=new QLabel("当前同步丢帧:",this);
    m_labCurrentSyncDelay=new QLabel("当前同步延迟:",this);
    m_labAvgSyncDiscard=new QLabel("平均同步丢帧:",this);
    m_labAvgSyncDelay=new QLabel("平均同步延迟:",this);
    m_streamsMenu=new QMenu;
    ui->statusbar->addWidget(m_labCurrentSyncDiscard);
    ui->statusbar->addWidget(m_labCurrentSyncDelay);
    ui->statusbar->addWidget(m_labAvgSyncDiscard);
    ui->statusbar->addWidget(m_labAvgSyncDelay);
    m_syncDiscardQueue.reserve(5);
    m_syncDelayMsQueue.reserve(5);
    for(int i=0;i<5;++i)
    {
        m_syncDiscardQueue.enqueue(0);
        m_syncDelayMsQueue.enqueue(0);
    }
    //设置ui连接
    initGuiConnections();
    //检验ffmpeg
    // QDebug debug=qDebug().nospace();
    // debug<<avcodec_configuration()<<"\n";
    //创建线程
    m_avProducerThread=new QThread(this);
    m_videoConsumerThread=new QThread(this);
    m_audioConsumerThread=new QThread(this);
    m_onvifClientThread=new QThread(this);
    m_screenShooterThread=new QThread(this);
    //创建缓冲区
    m_videoBuf=new VideoBufferPool;
    m_videoBuf->init(50*1024*1024);//50M
    m_videoBuf->setThrehold(50);
    m_audioBuf=new AudioBufferPool;
    m_audioBuf->init(10*1024*1024);//10M
    //创建工作对象
    m_avProducer=new AVProducer;
    m_videoConsumer=new VideoConsumer;
    m_audioConsumer=new AudioConsumer;
    m_onvifClient=new OnvifClient;
    m_screenShooter=new ScreenShooter;
    m_avProducer->setBuffers(m_videoBuf,m_audioBuf);
    m_videoConsumer->setBuffer(m_videoBuf);
    m_audioConsumer->setBuffer(m_audioBuf);
    m_ptzStatusUpdater=new QTimer(this);
    m_durationDisplayUpdater=new QTimer(this);
    //移动到相应线程
    m_avProducer->moveToThread(m_avProducerThread);
    m_videoBuf->moveToThread(m_avProducerThread);
    m_videoConsumer->moveToThread(m_videoConsumerThread);
    m_audioBuf->moveToThread(m_avProducerThread);
    m_audioConsumer->moveToThread(m_audioConsumerThread);
    m_onvifClient->moveToThread(m_onvifClientThread);
    m_screenShooter->moveToThread(m_screenShooterThread);
    //设置连接
    initCoreConnections();
    //元数据
    m_avInfoModel=new QStandardItemModel(this);
    ui->treeViewAVInfo->header()->setDefaultAlignment(Qt::AlignHCenter);
    ui->treeViewAVInfo->setModel(m_avInfoModel);
    connect(m_avProducer,&AVProducer::metaTreeDone,this,[this](auto root){
        fillMetaTreeRecursive(m_avInfoModel->invisibleRootItem(),root);
        m_avInfoModel->setHorizontalHeaderLabels({"属性","值"});
        ui->treeViewAVInfo->expandAll();
    });
    m_onvifInfoModel=new QStandardItemModel(this);
    ui->treeViewOnvifInfo->header()->setDefaultAlignment(Qt::AlignHCenter);
    ui->treeViewOnvifInfo->setModel(m_onvifInfoModel);
    connect(m_onvifClient,&OnvifClient::metaTreeDone,this,[this](auto root){
        fillMetaTreeRecursive(m_onvifInfoModel->invisibleRootItem(),root);
        m_onvifInfoModel->setHorizontalHeaderLabels({"属性","值"});
        ui->treeViewOnvifInfo->expandAll();
    });

    m_avProducerThread->start();
    m_avProducerThread->setPriority(QThread::HighestPriority);
    m_videoConsumerThread->start();
    m_audioConsumerThread->start();
    m_onvifClientThread->start();
    m_screenShooterThread->start();

    //bool videoBufferRet;
    QMetaObject::invokeMethod(m_videoBuf,&QIODevice::open,Qt::QueuedConnection,QIODeviceBase::ReadWrite);
    //bool audioBufferRet;
    QMetaObject::invokeMethod(m_audioBuf,&QIODevice::open,Qt::QueuedConnection,QIODeviceBase::ReadWrite);
    m_avProducer->startStateMachine();
    m_videoConsumer->startStateMachine();
    m_audioConsumer->startStateMachine();
    m_ptzStatusUpdater->start(1000);
    m_durationDisplayUpdater->start(1000);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    emit destroyAVProducer();
    QThread::usleep(50);
    emit destroyVideoConsumer();
    QThread::usleep(50);
    emit destroyAudioConsumer();
    QThread::usleep(50);
    emit destroyAVBufferPools();

    QMetaObject::invokeMethod(m_videoBuf,&QIODevice::close,Qt::QueuedConnection);
    QMetaObject::invokeMethod(m_audioBuf,&QIODevice::close,Qt::QueuedConnection);

    m_avProducer->deleteLater();
    m_videoConsumer->deleteLater();
    m_audioConsumer->deleteLater();
    m_videoBuf->deleteLater();
    m_audioBuf->deleteLater();
    m_onvifClient->deleteLater();
    m_avProducerThread->quit();
    m_videoConsumerThread->quit();
    m_audioConsumerThread->quit();
    m_onvifClientThread->quit();
    m_screenShooterThread->quit();

    m_avProducerThread->wait(3000);
    m_videoConsumerThread->wait(3000);
    m_audioConsumerThread->wait(3000);
    m_onvifClientThread->wait(3000);
    m_screenShooterThread->wait(3000);

    event->accept();
}

void MainWindow::doSetFullScreen(bool value)
{
    static QMargins centralMargins;
    static int centralSpacing;
    static Qt::WindowStates rawState;
    static QByteArray mainwindowState;
    if(value)
    {
        centralMargins=centralWidget()->layout()->contentsMargins();
        centralSpacing=centralWidget()->layout()->spacing();
        rawState=windowState();
        mainwindowState=saveState();
        menuBar()->setVisible(false);
        statusBar()->setVisible(false);
        centralWidget()->layout()->setContentsMargins(0,0,0,0);
        centralWidget()->layout()->setSpacing(0);
        removeDockWidget(ui->dockWidget);
        setWindowState(Qt::WindowFullScreen);
    }
    else
    {
        restoreState(mainwindowState);
        centralWidget()->layout()->setContentsMargins(centralMargins);
        centralWidget()->layout()->setSpacing(centralSpacing);
        menuBar()->setVisible(true);
        statusBar()->setVisible(true);
        setWindowState(rawState);
    }
    emit notifyFullScreen(value);
}

void MainWindow::doRequestAbsMove()
{
    qreal p=ui->spbPTZMoveAbsP->value();
    qreal vp=ui->spbPTZMoveVp->value();
    if(p>=ui->labPTZMoveValueP->text().toDouble())
    {
        ui->spbPTZMoveVp->setValue(qFabs(vp));
    }
    else
    {
        ui->spbPTZMoveVp->setValue(-qFabs(vp));
    }
    qreal t=ui->spbPTZMoveAbsT->value();
    qreal vt=ui->spbPTZMoveVt->value();
    if(t>=ui->labPTZMoveValueT->text().toDouble())
    {
        ui->spbPTZMoveVt->setValue(qFabs(vt));
    }
    else
    {
        ui->spbPTZMoveVt->setValue(-qFabs(vt));
    }
    emit notifyAbsMove(
        ui->spbPTZMoveVp->text().toDouble(),
        ui->spbPTZMoveVt->text().toDouble(),
        ui->spbPTZMoveAbsP->text().toDouble(),
        ui->spbPTZMoveAbsT->text().toDouble(),
        ui->spbPTZMoveAbsZ->text().toDouble()
        );
}

void MainWindow::doRequestConMove(DirectionRegion r)
{
    qreal vp=0,vt=0;
    switch(r)
    {
        case DirectionRegion::Up:
        {
            vt=qFabs(ui->spbPTZMoveVt->value());
            if(vt<0.05){vt=1;}
            break;
        }
        case DirectionRegion::Right:
        {
            vp=qFabs(ui->spbPTZMoveVp->value());
            if(vp<0.05){vp=1;}
            break;
        }
        case DirectionRegion::Down:
        {
            vt=-qFabs(ui->spbPTZMoveVt->value());
            if(vt>-0.05){vt=-1;}
            break;
        }
        case DirectionRegion::Left:
        {
            vp=-qFabs(ui->spbPTZMoveVp->value());
            if(vp>-0.05){vp=-1;}
            break;
        }
        default:
            break;
    }
    emit notifyConMove(vp,vt);
    ui->spbPTZMoveVp->setValue(vp);
    ui->spbPTZMoveVt->setValue(vt);
}

void MainWindow::doRequestStopOrReset(DirectionRegion r)
{
    if(DirectionRegion::None==r){return;}
    if(DirectionRegion::Center==r)
    {
        ui->spbPTZMoveAbsP->setValue(0);
        ui->spbPTZMoveAbsT->setValue(0);
        doRequestAbsMove();
        return;
    }
    emit notifyStopMove();
}

void MainWindow::makeUpStreamList(QMap<QString, QUrl> streams)
{
    m_streamsMap=streams;
    for(auto i=streams.cbegin(),end=streams.cend();i!=end;++i)
    {
        auto action=new QAction(i.key(),this);
        m_streamsList.append(action);
        action->setCheckable(true);
        connect(action,&QAction::triggered,this,&MainWindow::doSwitchStream);
    }
    m_streamsMenu->addActions(m_streamsList);
    ui->actionSwitchStream->setMenu(m_streamsMenu);
    m_streamsList[0]->setChecked(true);
    emit connectToUrl(m_streamsMap[m_streamsList[0]->text()]);
}

void MainWindow::doSwitchStream()
{
    auto action=qobject_cast<QAction*>(sender());
    for(auto &act:m_streamsList)
    {
        act->setChecked(false);
    }
    action->setChecked(true);
    emit changeAVUrl(m_streamsMap[action->text()]);
}

void MainWindow::initGuiConnections()
{
    //action
    connect(ui->actionOnvif,&QAction::triggered,this,[this]{
        //状态检查
        emit notifyDiscoverDevices();
        auto dialog=new DialogConnectToOnvif;
        connect(dialog,&DialogConnectToOnvif::validDevice,this,&MainWindow::connectToOnvif);
        connect(dialog,&QDialog::finished,this,[dialog]{dialog->deleteLater();});
        connect(m_onvifClient,&OnvifClient::matchDiscovery,dialog,&DialogConnectToOnvif::showMatchDevice);
        dialog->show();
    });
    connect(ui->actionExit,&QAction::triggered,this,&QMainWindow::close);
    connect(ui->actionScreenShot,&QAction::triggered,this,&MainWindow::notifyScreenShot);
    connect(ui->actionFullScreen,&QAction::triggered,this,[this]{
        doSetFullScreen(true);
    });
    connect(ui->actionSwitchDockWidget,&QAction::triggered,this,[this]{
        static QByteArray savedState;
        if(ui->dockWidget->isFloating()){return;}
        if(ui->dockWidget->isVisible())
        {
            savedState=saveState();
            removeDockWidget(ui->dockWidget);
            //qDebug()<<"dock:visible->hidden,saved state:"<<savedState;
        }
        else
        {
            restoreState(savedState);
            //qDebug()<<"dock:hidden->visible,saved state:"<<savedState;
        }
    });
    connect(ui->actionPause,&QAction::triggered,this,&MainWindow::notifySuspend);
    //界面
    connect(ui->comboBoxChangePage,&QComboBox::currentIndexChanged,ui->stackedWidget,&QStackedWidget::setCurrentIndex);
    //全屏
    connect(ui->display,&VideoDisplay::requestForFullScreen,this,&MainWindow::doSetFullScreen);
    connect(this,&MainWindow::notifyFullScreen,ui->display,&VideoDisplay::respondToMainFullScreen);
    //PTZ
    ui->spbPTZMoveVp->setRange(-1,1);
    ui->spbPTZMoveVt->setRange(-1,1);
    ui->spbPTZMoveVz->setRange(-1,1);
}

void MainWindow::initCoreConnections()
{
    //音视频连接
    connect(this,&MainWindow::connectToUrl,m_avProducer,&AVProducer::respondToMainUrl);
    connect(m_avProducer,&AVProducer::activeBuffers,m_videoBuf,&AVBufferPool::respondToProducer);
    connect(m_avProducer,&AVProducer::activeBuffers,m_audioBuf,&AVBufferPool::respondToProducer);
    connect(m_avProducer,&AVProducer::foundVideoFormat,m_videoConsumer,&VideoConsumer::respondToProducer);
    connect(m_avProducer,&AVProducer::foundAudioFormat,m_audioConsumer,&AudioConsumer::respondToProducer);
    connect(m_avProducer,&AVProducer::foundAudioFormat,m_audioBuf,&AudioBufferPool::respondToProducer);
    connect(m_avProducer,&AVProducer::foundVideoFormat,m_screenShooter,&ScreenShooter::respondToProducer);
    //音视频同步
    connect(m_audioBuf,&AudioBufferPool::notifyRenderTime,m_videoBuf,&VideoBufferPool::updateSyncTime);
    connect(m_audioBuf,&AudioBufferPool::notifyCacheTime,ui->display,&VideoDisplay::updateCacheTimeLabel);
    connect(m_audioBuf,&AudioBufferPool::notifyRenderTime,ui->display,&VideoDisplay::updateRenderTimeLabel);
    connect(m_videoBuf,&VideoBufferPool::notifySyncDiscard,this,[this]{
        ++m_currentSyncDiscard;
    });
    connect(m_videoBuf,&VideoBufferPool::notifySyncDelay,this,[this](quint16 ms){
        m_currentSyncDelayMs+=ms;
    });
    connect(m_durationDisplayUpdater,&QTimer::timeout,this,[this]{
        m_durationDisplayUpdater->start(1000);
        m_labCurrentSyncDiscard->setText("当前同步丢帧:"+QString::number(m_currentSyncDiscard));
        m_labCurrentSyncDelay->setText("当前同步延迟:"+QString::number(m_currentSyncDelayMs)+"ms");
        m_sumSyncDiscard=m_sumSyncDiscard-m_syncDiscardQueue.dequeue()+m_currentSyncDiscard;
        if(m_sumSyncDiscard<0){m_sumSyncDiscard=0;}
        m_sumSyncDelayMs=m_sumSyncDelayMs-m_syncDelayMsQueue.dequeue()+m_currentSyncDelayMs;
        if(m_sumSyncDelayMs<0){m_sumSyncDelayMs=0;}
        m_labAvgSyncDiscard->setText("平均同步丢帧:"+QString::number(m_sumSyncDiscard/5));
        m_labAvgSyncDelay->setText("平均同步延迟:"+QString::number(m_sumSyncDelayMs/5)+"ms");
        ui->statusbar->update();
        m_syncDiscardQueue.enqueue(m_currentSyncDiscard);
        m_syncDelayMsQueue.enqueue(m_currentSyncDelayMs);
        m_currentSyncDiscard=0;
        m_currentSyncDelayMs=0;
    });
    //显示
    connect(m_avProducer,&AVProducer::foundVideoFormat,ui->display,&VideoDisplay::respondToVideoSize);
    connect(this,&MainWindow::connectToOnvif,ui->display,&VideoDisplay::prepareToConnect);
    connect(m_videoConsumer,&VideoConsumer::sendFrame,ui->display,&VideoDisplay::displayFrame);
    //音量
    connect(ui->display,&VideoDisplay::changeVolume,m_audioConsumer,&AudioConsumer::setVolume);
    //暂停
    connect(ui->display,&VideoDisplay::requestSuspend,this,&MainWindow::notifySuspend);
    connect(this,&MainWindow::notifySuspend,ui->display,&VideoDisplay::respondToMainSuspend);
    connect(this,&MainWindow::notifySuspend,m_avProducer,&AVProducer::respondToMainSuspend,Qt::DirectConnection);
    connect(this,&MainWindow::notifySuspend,m_audioConsumer,&AudioConsumer::respondToMainSuspend);
    //截屏
    connect(this,&MainWindow::notifyScreenShot,m_videoConsumer,&VideoConsumer::respondToMainScreenShot);
    connect(m_videoConsumer,&VideoConsumer::notifyScreenShot,m_screenShooter,&ScreenShooter::screenShot);
    connect(m_screenShooter,&ScreenShooter::screenShotOK,this,[](QString dirName){
        QSystemTrayIcon tray;
        tray.setIcon(QIcon::fromTheme(QIcon::ThemeIcon::HelpAbout));
        tray.show();
        tray.showMessage("截屏保存成功","保存在"+dirName,QSystemTrayIcon::Information,3000);
    });
    //断开连接
    // connect(this,&MainWindow::notifyDisconnectAV,m_avProducer,&AVProducer::respondToMainDisconnect,Qt::DirectConnection);
    // connect(this,&MainWindow::notifyDisconnectAV,m_videoConsumer,&VideoConsumer::respondToMainDisconnect,Qt::DirectConnection);
    // connect(this,&MainWindow::notifyDisconnectAV,m_audioConsumer,&AudioConsumer::respondToMainDisconnect,Qt::DirectConnection);
    // connect(this,&MainWindow::notifyDisconnectAV,m_videoBuf,&AVBufferPool::respondToMainDisconnect,Qt::DirectConnection);
    // connect(this,&MainWindow::notifyDisconnectAV,m_audioBuf,&AVBufferPool::respondToMainDisconnect,Qt::DirectConnection);
    // connect(this,&MainWindow::notifyDisconnectAV,ui->display,&VideoDisplay::respondToMainDisconnect,Qt::UniqueConnection);
    // connect(this,&MainWindow::notifyDisconnectAV,m_screenShooter,&ScreenShooter::destroy);
    //切换码流
    connect(this,&MainWindow::changeAVUrl,m_avProducer,&AVProducer::respondToMainChangeUrl,Qt::DirectConnection);
    connect(this,&MainWindow::changeAVUrl,m_videoConsumer,&VideoConsumer::turnToReset);
    connect(this,&MainWindow::changeAVUrl,m_audioConsumer,&AudioConsumer::turnToReset);
    connect(this,&MainWindow::changeAVUrl,m_videoBuf,&VideoBufferPool::resetBuffer,Qt::DirectConnection);
    connect(this,&MainWindow::changeAVUrl,m_audioBuf,&AudioBufferPool::resetBuffer,Qt::DirectConnection);
    connect(this,&MainWindow::changeAVUrl,ui->display,&VideoDisplay::prepareToReconnect);
    //销毁
    connect(this,&MainWindow::destroyAVBufferPools,m_videoBuf,&AVBufferPool::respondToMainDestroy,Qt::DirectConnection);
    connect(this,&MainWindow::destroyAVBufferPools,m_audioBuf,&AVBufferPool::respondToMainDestroy,Qt::DirectConnection);
    connect(this,&MainWindow::destroyAVProducer,m_avProducer,&AVProducer::respondToMainDestroy,Qt::DirectConnection);
    connect(this,&MainWindow::destroyVideoConsumer,m_videoConsumer,&VideoConsumer::respondToMainDestroy,Qt::DirectConnection);
    connect(this,&MainWindow::destroyAudioConsumer,m_audioConsumer,&AudioConsumer::respondToMainDestroy,Qt::DirectConnection);

    //Onvif设备连接
    connect(this,&MainWindow::notifyDiscoverDevices,m_onvifClient,&OnvifClient::discoverDevices);
    connect(this,&MainWindow::connectToOnvif,m_onvifClient,&OnvifClient::respondToMainDevice);
    //鉴权失败
    connect(m_onvifClient,&OnvifClient::authFailed,this,[this]{
        m_onvifInfoModel->clear();
        QMessageBox::critical(this,"错误","Onvif鉴权失败!");
    });
    //设置码流地址
    connect(m_onvifClient,&OnvifClient::sendStreamUris,this,&MainWindow::makeUpStreamList);
    //PTZ
    connect(m_onvifClient,&OnvifClient::sendPTZProfile,this,[this](QList<qreal> defaultSpeed,QList<QPair<qreal,qreal>> range){
        ui->spbPTZMoveVp->setValue(defaultSpeed.at(0));
        ui->spbPTZMoveVt->setValue(defaultSpeed.at(1));
        ui->spbPTZMoveVz->setValue(defaultSpeed.at(2));
        ui->spbPTZMoveAbsP->setRange(range.at(0).first,range.at(0).second);
        ui->spbPTZMoveAbsT->setRange(range.at(1).first,range.at(1).second);
        ui->spbPTZMoveAbsZ->setRange(range.at(2).first,range.at(2).second);
    });
    connect(m_ptzStatusUpdater,&QTimer::timeout,this,[this]{
        m_ptzStatusUpdater->start(1000);
    });
    connect(m_ptzStatusUpdater,&QTimer::timeout,m_onvifClient,&OnvifClient::queryPTZStatus);
    connect(m_onvifClient,&OnvifClient::reportPTZStatus,this,[this](qreal p,qreal t,qreal z){
        ui->labPTZMoveValueP->setText(QString::number(p));
        ui->labPTZMoveValueT->setText(QString::number(t));
        ui->labPTZMoveValueZ->setText(QString::number(z));
    });
    connect(ui->btnPTZAbsMove,&QPushButton::clicked,this,&MainWindow::doRequestAbsMove);
    connect(this,&MainWindow::notifyAbsMove,m_onvifClient,&OnvifClient::absoluteMove);
    connect(ui->dPad,&DirectionalPad::press,this,&MainWindow::doRequestConMove);
    connect(ui->dPad,&DirectionalPad::release,this,&MainWindow::doRequestStopOrReset);
    connect(this,&MainWindow::notifyConMove,m_onvifClient,&OnvifClient::continuousMove);
    connect(this,&MainWindow::notifyStopMove,m_onvifClient,&OnvifClient::stopMove);

}

void MainWindow::fillMetaTreeRecursive(QStandardItem *parent, QSharedPointer<MetaTreeNode> node)
{
    if(node.isNull())
    {
        parent->model()->clear();
        return;
    }
    auto keyItem=new QStandardItem(node->key());
    keyItem->setEditable(false);
    auto valueItem=new QStandardItem(node->value().toString());
    valueItem->setEditable(false);
    valueItem->setData(node->value(),Qt::ToolTipRole);
    parent->appendRow({keyItem,valueItem});
    auto children=node->childen();
    if(children.isEmpty())
    {
        return;
    }
    foreach(auto child,children)
    {
        fillMetaTreeRecursive(keyItem,child);
    }
}

