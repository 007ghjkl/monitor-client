#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <QMainWindow>
#include <QThread>
#include <QCloseEvent>
extern "C"{
#include <libavformat/avformat.h>
}
#include "tavbufferpool.h"
#include "tavproducer.h"
#include "tvideoconsumer.h"
#include "taudioconsumer.h"
// #include "tvideodisplay.h"
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
    void initUiConnections();
    QThread *avProducerThread;
    QThread *videoConsumerThread;
    QThread *audioConsumerThread;
    TAVProducer *avProducer;
    TVideoConsumer *videoConsumer;
    TAudioConsumer *audioConsumer;
    TAVBufferPool *videoBuf,*audioBuf;

    void closeEvent(QCloseEvent* event) override;
signals:
    void connectToURL(QString url);
    void destroyAVBufferPools();
    void destroyAVProducer();
    void destroyVideoConsumer();
    void destroyAudioConsumer();
    void notifyDisconnect();
};
#endif // MAINWINDOW_H
