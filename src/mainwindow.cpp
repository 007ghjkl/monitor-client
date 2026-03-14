#include "mainwindow.h"
#include "../ui/ui_mainwindow.h"
#include <QInputDialog>
extern "C" {
#include <libavcodec/avcodec.h>
#include <openssl/sha.h>
}
#include <QDebug>
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    initUiConnections();
    //检验ffmpeg、OpenSSL
    // QDebug debug=qDebug().nospace();
    // debug<<avcodec_configuration()<<"\n";
    // const char* str = "ffplay rtsp://admin:zyn050504@192.168.1.140:554/stream1";
    // unsigned char hash[SHA256_DIGEST_LENGTH];
    // SHA256((const unsigned char*)str, strlen(str), hash);
    // for(int i=0;i<SHA256_DIGEST_LENGTH;i++)
    // {
    //     debug<<Qt::hex<<hash[i];
    // }

    videoBuf=new TAVBufferPool;
    videoBuf->init(50*1024*1024);//50M
    audioBuf=new TAVBufferPool;
    audioBuf->init(10*1024*1024);//10M

    avProducerThread=new QThread(this);
    videoConsumerThread=new QThread(this);
    audioConsumerThread=new QThread(this);
    avProducer=new TAVProducer;
    videoConsumer=new TVideoConsumer;
    audioConsumer=new TAudioConsumer;
    avProducer->setBuffers(videoBuf,audioBuf);
    videoConsumer->setBuffer(videoBuf);
    audioConsumer->setBuffer(audioBuf);
    //参数设置
    connect(this,&MainWindow::connectToURL,avProducer,&TAVProducer::respondToMainURL);
    connect(avProducer,&TAVProducer::foundVideoFormat,videoConsumer,&TVideoConsumer::respondToProducer);
    connect(avProducer,&TAVProducer::foundAudioFormat,audioConsumer,&TAudioConsumer::respondToProducer);
    //断开连接
    connect(this,&MainWindow::notifyDisconnect,avProducer,&TAVProducer::respondToMainDisconnect,Qt::DirectConnection);
    connect(this,&MainWindow::notifyDisconnect,videoConsumer,&TVideoConsumer::respondToMainDisconnect,Qt::DirectConnection);
    connect(this,&MainWindow::notifyDisconnect,audioConsumer,&TAudioConsumer::respondToMainDisconnect,Qt::DirectConnection);
    connect(this,&MainWindow::notifyDisconnect,videoBuf,&TAVBufferPool::respondToMainDisconnect,Qt::DirectConnection);
    connect(this,&MainWindow::notifyDisconnect,audioBuf,&TAVBufferPool::respondToMainDisconnect,Qt::DirectConnection);
    connect(this,&MainWindow::notifyDisconnect,ui->display,&TVideoDisplay::respondToMainDisconnect,Qt::UniqueConnection);
    //显示
    connect(videoConsumer,&TVideoConsumer::setDisplaySize,ui->display,&TVideoDisplay::respondToVideoSize,Qt::UniqueConnection);
    connect(videoConsumer,&TVideoConsumer::sendFrame,ui->display,&TVideoDisplay::displayFrame,Qt::UniqueConnection);
    //销毁
    connect(this,&MainWindow::destroyAVBufferPools,videoBuf,&TAVBufferPool::respondToMainDestroy,Qt::DirectConnection);
    connect(this,&MainWindow::destroyAVBufferPools,audioBuf,&TAVBufferPool::respondToMainDestroy,Qt::DirectConnection);
    connect(this,&MainWindow::destroyAVProducer,avProducer,&TAVProducer::respondToMainDestroy,Qt::DirectConnection);
    connect(this,&MainWindow::destroyVideoConsumer,videoConsumer,&TVideoConsumer::respondToMainDestroy,Qt::DirectConnection);
    connect(this,&MainWindow::destroyAudioConsumer,audioConsumer,&TAudioConsumer::respondToMainDestroy,Qt::DirectConnection);
    connect(avProducerThread,&QThread::finished,this,[](){qDebug()<<"avProducerThread退出";});
    connect(audioConsumerThread,&QThread::finished,this,[](){qDebug()<<"audioConsumerThread退出";});

    avProducer->moveToThread(avProducerThread);
    videoConsumer->moveToThread(videoConsumerThread);
    audioConsumer->moveToThread(audioConsumerThread);

    avProducerThread->start();
    videoConsumerThread->start();
    audioConsumerThread->start();
    avProducer->startStateMachine();
    videoConsumer->startStateMachine();
    audioConsumer->startStateMachine();
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

    avProducer->deleteLater();
    videoConsumer->deleteLater();
    audioConsumer->deleteLater();
    avProducerThread->quit();
    videoConsumerThread->quit();
    audioConsumerThread->quit();

    avProducerThread->wait(3000);
    videoConsumerThread->wait(3000);
    audioConsumerThread->wait(3000);

    videoBuf->deleteLater();
    audioBuf->deleteLater();

    event->accept();
}

void MainWindow::doConnectToUrl()
{//缺少状态检查
    bool ok=false;
    //"E:/Tools/GICutscenes/output/genshin.mp4"
    //"rtsp://192.168.232.129:554/live/stream"
    //"rtsp://admin:zyn050504@192.168.1.140:554/stream1"
    // QString url=QInputDialog::getText(this,"输入URL","URL",QLineEdit::Normal,"E:/Tools/GICutscenes/output/genshin.mp4",&ok);
    // QString url=QInputDialog::getText(this,"输入URL","URL",QLineEdit::Normal,"rtsp://192.168.232.129:554/live/stream",&ok);
    QString url=QInputDialog::getText(this,"输入URL","URL",QLineEdit::Normal,"rtsp://admin:zyn050504@192.168.1.140:554/stream1",&ok);
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

