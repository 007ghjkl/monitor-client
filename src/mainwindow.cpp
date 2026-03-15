#include "mainwindow.h"
#include "../ui/ui_mainwindow.h"
#include <QInputDialog>
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

    m_videoBuf=new TAVBufferPool;
    m_videoBuf->init(50*1024*1024);//50M
    m_audioBuf=new TAVBufferPool;
    m_audioBuf->init(10*1024*1024);//10M

    m_avProducerThread=new QThread(this);
    m_videoConsumerThread=new QThread(this);
    m_audioConsumerThread=new QThread(this);
    m_avProducer=new TAVProducer;
    m_videoConsumer=new TVideoConsumer;
    m_audioConsumer=new TAudioConsumer;
    m_avProducer->setBuffers(m_videoBuf,m_audioBuf);
    m_videoConsumer->setBuffer(m_videoBuf);
    m_audioConsumer->setBuffer(m_audioBuf);
    //参数设置
    connect(this,&MainWindow::connectToURL,m_avProducer,&TAVProducer::respondToMainURL);
    connect(m_avProducer,&TAVProducer::foundVideoFormat,m_videoConsumer,&TVideoConsumer::respondToProducer);
    connect(m_avProducer,&TAVProducer::foundAudioFormat,m_audioConsumer,&TAudioConsumer::respondToProducer);
    //断开连接
    connect(this,&MainWindow::notifyDisconnect,m_avProducer,&TAVProducer::respondToMainDisconnect,Qt::DirectConnection);
    connect(this,&MainWindow::notifyDisconnect,m_videoConsumer,&TVideoConsumer::respondToMainDisconnect,Qt::DirectConnection);
    connect(this,&MainWindow::notifyDisconnect,m_audioConsumer,&TAudioConsumer::respondToMainDisconnect,Qt::DirectConnection);
    connect(this,&MainWindow::notifyDisconnect,m_videoBuf,&TAVBufferPool::respondToMainDisconnect,Qt::DirectConnection);
    connect(this,&MainWindow::notifyDisconnect,m_audioBuf,&TAVBufferPool::respondToMainDisconnect,Qt::DirectConnection);
    connect(this,&MainWindow::notifyDisconnect,ui->display,&TVideoDisplay::respondToMainDisconnect,Qt::UniqueConnection);
    //显示
    connect(m_videoConsumer,&TVideoConsumer::setDisplaySize,ui->display,&TVideoDisplay::respondToVideoSize,Qt::UniqueConnection);
    connect(m_videoConsumer,&TVideoConsumer::sendFrame,ui->display,&TVideoDisplay::displayFrame,Qt::UniqueConnection);
    //销毁
    connect(this,&MainWindow::destroyAVBufferPools,m_videoBuf,&TAVBufferPool::respondToMainDestroy,Qt::DirectConnection);
    connect(this,&MainWindow::destroyAVBufferPools,m_audioBuf,&TAVBufferPool::respondToMainDestroy,Qt::DirectConnection);
    connect(this,&MainWindow::destroyAVProducer,m_avProducer,&TAVProducer::respondToMainDestroy,Qt::DirectConnection);
    connect(this,&MainWindow::destroyVideoConsumer,m_videoConsumer,&TVideoConsumer::respondToMainDestroy,Qt::DirectConnection);
    connect(this,&MainWindow::destroyAudioConsumer,m_audioConsumer,&TAudioConsumer::respondToMainDestroy,Qt::DirectConnection);
    connect(m_avProducerThread,&QThread::finished,this,[](){qDebug()<<"avProducerThread退出";});
    connect(m_audioConsumerThread,&QThread::finished,this,[](){qDebug()<<"audioConsumerThread退出";});

    m_avProducer->moveToThread(m_avProducerThread);
    m_videoConsumer->moveToThread(m_videoConsumerThread);
    m_audioConsumer->moveToThread(m_audioConsumerThread);

    m_avProducerThread->start();
    m_videoConsumerThread->start();
    m_audioConsumerThread->start();
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

    m_avProducerThread->wait(3000);
    m_videoConsumerThread->wait(3000);
    m_audioConsumerThread->wait(3000);

    m_videoBuf->deleteLater();
    m_audioBuf->deleteLater();

    event->accept();
}

void MainWindow::doConnectToUrl()
{//缺少状态检查
    bool ok=false;
    //"E:/Tools/GICutscenes/output/genshin.mp4" "/home/rainbow/Documents/study/av/genshin.mp4"
    //"rtsp://192.168.232.129:554/live/stream"
    //"rtsp://admin:zyn050504@192.168.1.140:554/stream1"
    QString url=QInputDialog::getText(this,"输入URL","URL",QLineEdit::Normal,"E:/Tools/GICutscenes/output/genshin.mp4",&ok);
    // QString url=QInputDialog::getText(this,"输入URL","URL",QLineEdit::Normal,"rtsp://192.168.232.129:554/live/stream",&ok);
    // QString url=QInputDialog::getText(this,"输入URL","URL",QLineEdit::Normal,"rtsp://admin:zyn050504@192.168.1.140:554/stream1",&ok);
    if(ok)
    {
        emit connectToURL(url);
    }
}

void MainWindow::doDisconnect()
{//缺少状态检查
    emit notifyDisconnect();
}

void MainWindow::initUiConnections()
{
    connect(ui->actionURL,&QAction::triggered,this,&MainWindow::doConnectToUrl);
    connect(ui->actionDisconnect,&QAction::triggered,this,&MainWindow::doDisconnect);
    connect(ui->actionExit,&QAction::triggered,this,&QMainWindow::close);
}

