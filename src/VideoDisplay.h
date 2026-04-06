#ifndef VIDEODISPLAY_H
#define VIDEODISPLAY_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include <QScopedPointer>
#include <QPushButton>
#include <QSlider>
#include <QLabel>
#include <QGraphicsOpacityEffect>
#include <QPropertyAnimation>
#include <QStyle>
#include <QHBoxLayout>
#include <QTimer>
class VideoDisplay : public QOpenGLWidget,protected QOpenGLFunctions
{
    Q_OBJECT
public:
    VideoDisplay(QWidget* parent);
    virtual ~VideoDisplay();
protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w,int h) override;
private:
    int m_videoWidth;
    int m_videoHeight;
    QByteArray m_yData;
    QByteArray m_uData;
    QByteArray m_vData;
    QOpenGLShaderProgram *m_prog;//着色器程序
    QOpenGLVertexArrayObject *m_vao;//顶点数组对象
    QScopedPointer<QOpenGLBuffer> m_vbo;//顶点缓冲对象
    QScopedPointer<QOpenGLTexture> m_yTexture;
    QScopedPointer<QOpenGLTexture> m_uTexture;
    QScopedPointer<QOpenGLTexture> m_vTexture;
    QVector2D m_scale;
    QByteArray m_vsh;
    QByteArray m_fsh;

    bool m_isPlaying;
    QString m_textNoSignal;
    QString m_textPromt;

    void initShaders();
    void initGeometry();
    void initTextures();
    void updateDisplayGeometry();

    void drawNoSignal();
    void drawPromt();

    QWidget *m_controlBar;
    QPropertyAnimation *m_opacityAnimation;
    QGraphicsOpacityEffect *m_opacityEffect;
    bool m_isControlBarVisible;
    QPushButton *m_playBtn;
    QPushButton *m_stopBtn;
    QSlider *m_progressSlider;
    QLabel *m_timeLabel;
    QPushButton *m_volumeIconBtn;
    QSlider *m_volumeSlider;
    QPushButton *m_fullScreenBtn;
    void setupControlBar();
    void updateControlBarGeometry();

    bool m_isFullScreen;
    bool event(QEvent *e)override;

    QTimer *m_hideTimer;
    bool eventFilter(QObject *watched, QEvent *event) override;
private slots:
    void showControlBar();
    void hideControlBar();
public slots:
    void respondToVideoSize(int w,int h);
    void displayFrame(QByteArray d);
    void respondToMainDisconnect();
    void respondToMainFullScreen(bool value);
signals:
    void requestForFullScreen(bool value);
};

#endif // VIDEODISPLAY_H
