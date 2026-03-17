#include "MainWindow.h"
#include "../ui/ui_MainWindow.h"
#include "DialogConnectToUrl.h"
#include "DialogConnectToOnvif.h"
#include <QScopedPointer>
extern "C" {
#include <libavcodec/avcodec.h>
}
#include <QDebug>
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    initUiConnections();
    //检验ffmpeg
    // QDebug debug=qDebug().nospace();
    // debug<<avcodec_configuration()<<"\n";

    m_videoBuf=new AVBufferPool;
    m_videoBuf->init(50*1024*1024);//50M
    m_audioBuf=new AVBufferPool;
    m_audioBuf->init(10*1024*1024);//10M

    m_avProducerThread=new QThread(this);
    m_videoConsumerThread=new QThread(this);
    m_audioConsumerThread=new QThread(this);
    m_onvifClientThread=new QThread(this);
    m_avProducer=new AVProducer;
    m_videoConsumer=new VideoConsumer;
    m_audioConsumer=new AudioConsumer;
    m_onvifClient=new OnvifClient;
    m_avProducer->setBuffers(m_videoBuf,m_audioBuf);
    m_videoConsumer->setBuffer(m_videoBuf);
    m_audioConsumer->setBuffer(m_audioBuf);
    //参数设置
    connect(this,&MainWindow::connectToURL,m_avProducer,&AVProducer::respondToMainURL);
    connect(m_avProducer,&AVProducer::foundVideoFormat,m_videoConsumer,&VideoConsumer::respondToProducer);
    connect(m_avProducer,&AVProducer::foundAudioFormat,m_audioConsumer,&AudioConsumer::respondToProducer);
    connect(this,&MainWindow::connectToOnvif,m_onvifClient,&OnvifClient::respondToMainURL);
    //断开连接
    connect(this,&MainWindow::notifyDisconnect,m_avProducer,&AVProducer::respondToMainDisconnect,Qt::DirectConnection);
    connect(this,&MainWindow::notifyDisconnect,m_videoConsumer,&VideoConsumer::respondToMainDisconnect,Qt::DirectConnection);
    connect(this,&MainWindow::notifyDisconnect,m_audioConsumer,&AudioConsumer::respondToMainDisconnect,Qt::DirectConnection);
    connect(this,&MainWindow::notifyDisconnect,m_videoBuf,&AVBufferPool::respondToMainDisconnect,Qt::DirectConnection);
    connect(this,&MainWindow::notifyDisconnect,m_audioBuf,&AVBufferPool::respondToMainDisconnect,Qt::DirectConnection);
    connect(this,&MainWindow::notifyDisconnect,ui->display,&VideoDisplay::respondToMainDisconnect,Qt::UniqueConnection);
    //显示
    connect(m_videoConsumer,&VideoConsumer::setDisplaySize,ui->display,&VideoDisplay::respondToVideoSize,Qt::UniqueConnection);
    connect(m_videoConsumer,&VideoConsumer::sendFrame,ui->display,&VideoDisplay::displayFrame,Qt::UniqueConnection);
    //销毁
    connect(this,&MainWindow::destroyAVBufferPools,m_videoBuf,&AVBufferPool::respondToMainDestroy,Qt::DirectConnection);
    connect(this,&MainWindow::destroyAVBufferPools,m_audioBuf,&AVBufferPool::respondToMainDestroy,Qt::DirectConnection);
    connect(this,&MainWindow::destroyAVProducer,m_avProducer,&AVProducer::respondToMainDestroy,Qt::DirectConnection);
    connect(this,&MainWindow::destroyVideoConsumer,m_videoConsumer,&VideoConsumer::respondToMainDestroy,Qt::DirectConnection);
    connect(this,&MainWindow::destroyAudioConsumer,m_audioConsumer,&AudioConsumer::respondToMainDestroy,Qt::DirectConnection);
    connect(m_avProducerThread,&QThread::finished,this,[](){qDebug()<<"avProducerThread退出";});
    connect(m_audioConsumerThread,&QThread::finished,this,[](){qDebug()<<"audioConsumerThread退出";});

    m_avProducer->moveToThread(m_avProducerThread);
    m_videoConsumer->moveToThread(m_videoConsumerThread);
    m_audioConsumer->moveToThread(m_audioConsumerThread);
    m_onvifClient->moveToThread(m_onvifClientThread);

    m_avProducerThread->start();
    m_videoConsumerThread->start();
    m_audioConsumerThread->start();
    m_onvifClientThread->start();
    m_avProducer->startStateMachine();
    m_videoConsumer->startStateMachine();
    m_audioConsumer->startStateMachine();
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

    m_avProducer->deleteLater();
    m_videoConsumer->deleteLater();
    m_audioConsumer->deleteLater();
    m_avProducerThread->quit();
    m_videoConsumerThread->quit();
    m_audioConsumerThread->quit();
    m_onvifClientThread->quit();

    m_avProducerThread->wait(3000);
    m_videoConsumerThread->wait(3000);
    m_audioConsumerThread->wait(3000);
    m_onvifClientThread->wait(3000);

    m_videoBuf->deleteLater();
    m_audioBuf->deleteLater();

    event->accept();
}

void MainWindow::doConnectToUrl()
{
    //"E:/Tools/GICutscenes/output/genshin.mp4" "/home/rainbow/Documents/study/av/genshin.mp4"
    //"rtsp://192.168.232.129:554/live/stream"
    //"rtsp://192.168.1.140:554/stream1" admin:zyn050504

    auto dialog=new DialogConnectToUrl;
    connect(dialog,&DialogConnectToUrl::validUrl,this,[this](QUrl url){
        m_url=url;
        emit connectToURL(m_url);
    });
    connect(dialog,&QDialog::finished,this,[dialog]{dialog->deleteLater();});
    dialog->show();
}

void MainWindow::doDisconnect()
{//缺少状态检查
    emit notifyDisconnect();
}

void MainWindow::initUiConnections()
{
    //action
    connect(ui->actionURL,&QAction::triggered,this,&MainWindow::doConnectToUrl);
    connect(ui->actionOnvif,&QAction::triggered,this,[this]{
        auto dialog=new DialogConnectToOnvif;
        connect(dialog,&DialogConnectToOnvif::validDeviceAddr,this,&MainWindow::connectToOnvif);
        connect(dialog,&QDialog::finished,this,[dialog]{dialog->deleteLater();});
        dialog->show();
    });
    connect(ui->actionDisconnect,&QAction::triggered,this,&MainWindow::doDisconnect);
    connect(ui->actionExit,&QAction::triggered,this,&QMainWindow::close);

    //界面
    connect(ui->comboBoxChangePage,&QComboBox::activated,ui->stackedWidget,&QStackedWidget::setCurrentIndex);
}

