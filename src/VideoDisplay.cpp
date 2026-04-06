#include "VideoDisplay.h"
#include <QPainter>
#include <QPainterPath>
#include <QDebug>
#include <QSurfaceFormat>
#include <QApplication>
#include <QMouseEvent>
// #define DISPLAY_DEBUG
VideoDisplay::VideoDisplay(QWidget* parent):QOpenGLWidget(parent),m_isControlBarVisible(true),m_isFullScreen(false)
{
    m_isPlaying=false;
    m_textNoSignal="无信号";
    m_textPromt="提示";
    // setAttribute(Qt::WA_TranslucentBackground,false);
    // setAttribute(Qt::WA_AlwaysStackOnTop,false);
    // QSurfaceFormat format=this->format();
    // format.setAlphaBufferSize(0); // 强制关闭 Alpha 缓冲区
    // format.setDepthBufferSize(24);
    // format.setStencilBufferSize(8);
    // format.setRenderableType(QSurfaceFormat::OpenGL);
    // format.setProfile(QSurfaceFormat::CoreProfile);
    // setFormat(format);
    // setAutoFillBackground(true);

    setMouseTracking(true);
    setupControlBar();
    m_hideTimer = new QTimer(this);
    m_hideTimer->setInterval(3000);
    m_hideTimer->start();
    connect(m_hideTimer, &QTimer::timeout, this, &VideoDisplay::hideControlBar);
    qApp->installEventFilter(this);//监听全局鼠标事件以防鼠标在子控件上时不触发
}

VideoDisplay::~VideoDisplay()
{
    makeCurrent();
    m_vao->destroy();
    m_vbo->destroy();
    if(m_yTexture)
    {
        m_yTexture->destroy();
        m_uTexture->destroy();
        m_vTexture->destroy();
    }
    doneCurrent();
}

void VideoDisplay::initializeGL()
{
    initializeOpenGLFunctions();
    initShaders();
    initGeometry();
    //有输入再initTextures()
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // qDebug() << "Current Format Alpha Size:" << context()->format().alphaBufferSize();
}

void VideoDisplay::paintGL()
{
    if(m_isPlaying==false)
    {
        drawNoSignal();
        return;
    }
#ifdef DISPLAY_DEBUG
    qDebug()<<"显示1帧...";
#endif
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    updateDisplayGeometry();

    m_prog->bind();
    // 设置缩放
    m_prog->setUniformValue("u_scale",m_scale);
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        qDebug() << "OpenGL error after setData scale:" << err;
    }
    // 绑定Y纹理
    glActiveTexture(GL_TEXTURE0);
    m_yTexture->bind();
    m_yTexture->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt8,m_yData.constData(), nullptr);
    m_prog->setUniformValue("tex_y", 0);
    err = glGetError();
    if (err != GL_NO_ERROR) {
        qDebug() << "OpenGL error after setData Y:" << err;
    }
    // 绑定U纹理
    glActiveTexture(GL_TEXTURE1);
    m_uTexture->bind();
    m_uTexture->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt8,m_uData.constData(), nullptr);
    m_prog->setUniformValue("tex_u", 1);
    // 绑定V纹理
    glActiveTexture(GL_TEXTURE2);
    m_vTexture->bind();
    m_vTexture->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt8,m_vData.constData(), nullptr);
    m_prog->setUniformValue("tex_v", 2);
    // 绘制矩形
    m_vao->bind();
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glFlush();
    m_vao->release();
    m_yTexture->release();
    m_uTexture->release();
    m_vTexture->release();
    m_prog->release();

    if(!m_textPromt.isEmpty())
    {
        drawPromt();
    }
}

void VideoDisplay::resizeGL(int w, int h)
{
    updateControlBarGeometry();
    if(m_isPlaying==false)
        return;
    updateDisplayGeometry();
}

void VideoDisplay::initShaders()
{
    m_vsh=R"(
    attribute vec2 aPos;
    attribute vec2 aTexCoord;
    varying vec2 texCoord;
    uniform vec2 u_scale;
    void main(void)
    {
        vec2 scaledPos = aPos * u_scale;
        gl_Position = vec4(scaledPos,0.0,1.0);
        texCoord=aTexCoord;
    }
    )";
    m_fsh=R"(
    varying vec2 texCoord;
    uniform sampler2D tex_y;
    uniform sampler2D tex_u;
    uniform sampler2D tex_v;
    const mat3 bt709=mat3(
        1.16438356e+00, 3.15967490e-17, 1.79274107e+00,
        1.16438356e+00,-2.13248614e-01,-5.32909329e-01,
        1.16438356e+00, 2.11240179e+00,-5.08708812e-17
    );
    //out vec4 DebugColor;
    void main(void)
    {
        vec3 yuv;
        vec3 rgb;
        yuv.x = texture2D(tex_y, texCoord).r;
        yuv.y = texture2D(tex_u, texCoord).r - 0.5;
        yuv.z = texture2D(tex_v, texCoord).r - 0.5;
        rgb=yuv*bt709;
        gl_FragColor = vec4(rgb, 1.0);

        //DebugColor = vec4(texture2D(tex_y, texCoord).rrr, 1.0);
    }
    )";
    m_prog=new QOpenGLShaderProgram(this);
    if(!(m_prog->addShaderFromSourceCode(QOpenGLShader::Vertex,m_vsh)))
    {
        qDebug() << "顶点着色器:" << m_prog->log();
        return;
    }
    if(!(m_prog->addShaderFromSourceCode(QOpenGLShader::Fragment,m_fsh)))
    {
        qDebug() << "片元着色器:" << m_prog->log();
        return;
    }
    if(!(m_prog->link()))
    {
        qDebug() << "着色器链接时:" << m_prog->log();
        return;
    }
}

void VideoDisplay::initGeometry()
{
    GLfloat vertices[] = {
        // 位置          // 纹理坐标
        -1.0f,  1.0f,   0.0f, 0.0f,  // 左上
        -1.0f, -1.0f,   0.0f, 1.0f,  // 左下
        1.0f,  1.0f,   1.0f, 0.0f,  // 右上
        1.0f, -1.0f,   1.0f, 1.0f   // 右下
    };
    m_vao=new QOpenGLVertexArrayObject(this);
    m_vao->create();
    m_vao->bind();
    m_vbo.reset(new QOpenGLBuffer);
    m_vbo->create();
    m_vbo->bind();
    m_vbo->allocate(vertices, sizeof(vertices));

    m_prog->bind();
    m_prog->enableAttributeArray(0);// 位置属性
    m_prog->setAttributeBuffer(0, GL_FLOAT, 0, 2, 4 * sizeof(GLfloat));
    m_prog->enableAttributeArray(1);// 纹理坐标属性
    m_prog->setAttributeBuffer(1, GL_FLOAT, 2 * sizeof(GLfloat), 2, 4 * sizeof(GLfloat));
    m_prog->release();

    m_vbo->release();
    m_vao->release();
}

void VideoDisplay::initTextures()
{
    m_yTexture.reset(new QOpenGLTexture(QOpenGLTexture::Target2D));
    m_yTexture->setFormat(QOpenGLTexture::R8_UNorm);//单通道纹理
    m_yTexture->setMinificationFilter(QOpenGLTexture::Linear);//双线性插值
    m_yTexture->setMagnificationFilter(QOpenGLTexture::Linear);
    m_yTexture->setWrapMode(QOpenGLTexture::ClampToEdge);//纹理坐标超出[0,1]范围时，使用边缘值而不是重复纹理
    m_yTexture->setSize(m_videoWidth, m_videoHeight);// 全分辨率
    m_yTexture->allocateStorage(QOpenGLTexture::Red, QOpenGLTexture::UInt8);

    m_uTexture.reset(new QOpenGLTexture(QOpenGLTexture::Target2D));
    m_uTexture->setFormat(QOpenGLTexture::R8_UNorm);
    m_uTexture->setMinificationFilter(QOpenGLTexture::Linear);
    m_uTexture->setMagnificationFilter(QOpenGLTexture::Linear);
    m_uTexture->setWrapMode(QOpenGLTexture::ClampToEdge);
    m_uTexture->setSize(m_videoWidth/2,m_videoHeight/2);//半分辨率
    m_uTexture->allocateStorage(QOpenGLTexture::Red,QOpenGLTexture::UInt8);

    m_vTexture.reset(new QOpenGLTexture(QOpenGLTexture::Target2D));
    m_vTexture->setFormat(QOpenGLTexture::R8_UNorm);
    m_vTexture->setMinificationFilter(QOpenGLTexture::Linear);
    m_vTexture->setMagnificationFilter(QOpenGLTexture::Linear);
    m_vTexture->setWrapMode(QOpenGLTexture::ClampToEdge);
    m_vTexture->setSize(m_videoWidth/2,m_videoHeight/2);//半分辨率
    m_vTexture->allocateStorage(QOpenGLTexture::Red,QOpenGLTexture::UInt8);
}

void VideoDisplay::updateDisplayGeometry()
{
    // glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float widgetAspect = (float)width() / height();
    float videoAspect = (float)m_videoWidth / m_videoHeight;

    if (widgetAspect > videoAspect) {
        // 窗口更宽，左右黑边
        m_scale.setX(videoAspect / widgetAspect);
        m_scale.setY(1.0f);
    } else {
        // 窗口更高，上下黑边
        m_scale.setX(1.0f);
        m_scale.setY(widgetAspect / videoAspect);
    }
}

void VideoDisplay::drawNoSignal()
{
    // isPlaying=false;
    setPalette(QPalette(QColorConstants::Black));
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    // setAutoFillBackground(true);
    int W= width();
    int H= height();
    // qDebug()<<W<<H;
    QPainter painter(this);
    QFont font;
    font.setPointSize(30);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(QRectF(W/4,H/4,W/2,H/2),Qt::AlignCenter,m_textNoSignal);
    // setAutoFillBackground(false);
}

void VideoDisplay::drawPromt()
{
    QPainter painter(this);
    QPointF pos(12,24);

    QPen pen=painter.pen();
    pen.setWidth(3);
    pen.setColor(Qt::black);
    painter.setPen(pen);
    QFont font=painter.font();
    font.setPointSize(12);
    painter.setFont(font);
    QPainterPath path;
    path.addText(pos,font,m_textPromt);
    painter.drawPath(path);//黑边

    pen.setColor(Qt::white);
    pen.setWidth(1);
    painter.setPen(pen);
    painter.drawText(pos,m_textPromt);//白芯
}

void VideoDisplay::setupControlBar()
{
    // 创建控制栏遮罩 Widget
    m_controlBar = new QWidget(this);
    m_controlBar->setObjectName("ControlBar");
    // 半透明深色背景
    m_controlBar->setStyleSheet(R"(
        #ControlBar {
            background-color: rgba(30, 30, 30, 200);
        }
    )");

    // 使用不透明度效果进行淡入淡出动画
    m_opacityEffect = new QGraphicsOpacityEffect(m_controlBar);
    m_controlBar->setGraphicsEffect(m_opacityEffect);
    m_opacityEffect->setOpacity(1.0);

    m_opacityAnimation = new QPropertyAnimation(m_opacityEffect, "opacity", this);
    m_opacityAnimation->setEasingCurve(QEasingCurve::InOutQuad);

    // 播放/暂停 按钮
    m_playBtn = new QPushButton(m_controlBar);
    m_playBtn->setIcon(QPixmap(":/svg/resume.svg"));
    m_playBtn->setToolTip("播放/暂停");
    connect(m_playBtn, &QPushButton::clicked, this,[this]{
        if(m_isPlaying)
        {
            m_playBtn->setIcon(QPixmap(":/svg/pause.svg"));
            qDebug() << ">>> [操作] 点击了播放";
        }
        else
        {
            m_playBtn->setIcon(QPixmap(":/svg/resume.svg"));
            qDebug() << ">>> [操作] 点击了暂停";
        }
    });

    // 停止 按钮
    m_stopBtn = new QPushButton(m_controlBar);
    m_stopBtn->setIcon(QPixmap(":/svg/stop_playing.svg"));
    m_stopBtn->setToolTip("停止");
    connect(m_stopBtn, &QPushButton::clicked, this,[this]{
        m_playBtn->setIcon(QPixmap(":/svg/resume.svg"));
        m_progressSlider->setValue(0);
        qDebug() << ">>> [操作] 点击了停止播放";
    });

    // 进度条
    m_progressSlider = new QSlider(Qt::Horizontal, m_controlBar);
    m_progressSlider->setRange(0, 100);
    m_progressSlider->setValue(30); // 假装有个进度
    connect(m_progressSlider, &QSlider::valueChanged, this,[this](auto value){
        qDebug() << ">>> [操作] 进度条拖动至:" << value << "%";
    });

    // 时间显示
    m_timeLabel = new QLabel("01:23 / 05:00", m_controlBar);
    m_timeLabel->setStyleSheet("color: white; font-size: 12px;");

    // 音量图标
    m_volumeIconBtn = new QPushButton(m_controlBar);
    m_volumeIconBtn->setIcon(style()->standardIcon(QStyle::SP_MediaVolume));
    // m_volumeIconBtn->setFlat(true); // 融合到背景

    // 音量条
    m_volumeSlider = new QSlider(Qt::Horizontal, m_controlBar);
    m_volumeSlider->setRange(0, 100);
    m_volumeSlider->setValue(80);
    m_volumeSlider->setFixedWidth(100);
    connect(m_volumeSlider, &QSlider::valueChanged, this,[](auto value){
        qDebug() << ">>> [操作] 音量调整至:" << value;
    });

    // 全屏按钮
    m_fullScreenBtn = new QPushButton(m_controlBar);
    m_fullScreenBtn->setIcon(QPixmap(":/svg/fullscreen_enter.svg"));
    m_fullScreenBtn->setToolTip("全屏");
    // m_fullScreenBtn->setFlat(true);
    connect(m_fullScreenBtn, &QPushButton::clicked, this,[this]{
        emit requestForFullScreen(!m_isFullScreen);
    });

    // 设置按钮样式，让它们在深色背景上更好看
    QString btnStyle = "QPushButton {border: none;width:30px;height:30px; border-radius: 15px;} "
                       "QPushButton:hover { background-color: rgba(255, 255, 255, 50);}";
    m_playBtn->setStyleSheet(btnStyle);
    m_stopBtn->setStyleSheet(btnStyle);
    m_volumeIconBtn->setStyleSheet(btnStyle);
    m_fullScreenBtn->setStyleSheet(btnStyle);
    QString sliderStyle=R"(
        QSlider::groove:horizontal
        {
            height:4px;
            border-radius:2px;
            background:rgb(64,64,64);
        }
        QSlider::handle:horizontal
        {
            width: 10px; /* 设置滑块宽度 */
            height: 10px; /* 设置滑块高度 */
            margin-top: -2.5px; /* 向上偏移 */
            margin-bottom: -2.5px; /* 向下偏移 */
            border-radius: 5px; /* 圆角效果 */
            background: #3498db; /* 滑块颜色 */
        }
        QSlider::sub-page:horizontal
        {
            background: #3498db;
            border-radius:2px;
        }
)";
    m_progressSlider->setStyleSheet(sliderStyle);
    m_volumeSlider->setStyleSheet(sliderStyle);

    // 布局组装
    QHBoxLayout *hLayout = new QHBoxLayout(m_controlBar);
    hLayout->setContentsMargins(15, 5, 15, 5);
    hLayout->setSpacing(5);
    hLayout->addWidget(m_playBtn);
    hLayout->addWidget(m_stopBtn);
    hLayout->addWidget(m_progressSlider);
    hLayout->addWidget(m_timeLabel);
    hLayout->addSpacing(5);
    hLayout->addWidget(m_volumeIconBtn);
    hLayout->addWidget(m_volumeSlider);
    hLayout->addWidget(m_fullScreenBtn);

}

void VideoDisplay::updateControlBarGeometry()
{
    // 让遮罩居于窗口底部
    int barHeight = 50;
    int margin = 0; // 如果想悬浮可以设为>0，这里为了类似真实播放器设为0，并让它适应宽度
    m_controlBar->setGeometry(margin, this->height() - barHeight - margin,
                              this->width() - 2 * margin, barHeight);
}

bool VideoDisplay::event(QEvent *e)
{
    if(QEvent::MouseButtonDblClick==e->type())
    {
        emit requestForFullScreen(!m_isFullScreen);
    }
    if(QEvent::KeyPress==e->type()&&m_isFullScreen)
    {
        auto keyEvent=static_cast<QKeyEvent*>(e);
        if(Qt::Key_Escape==keyEvent->key())
        {
            emit requestForFullScreen(false);
        }
    }
    return QOpenGLWidget::event(e);
}

bool VideoDisplay::eventFilter(QObject *watched, QEvent *event)
{
    // 全局捕获鼠标移动事件，来重置隐藏定时器
    if (event->type() == QEvent::MouseMove) {
        // 只有当鼠标位置确实在播放器内时才处理
        QMouseEvent *mouseEvent = static_cast<QMouseEvent*>(event);
        if (this->rect().contains(this->mapFromGlobal(mouseEvent->globalPosition().toPoint()))) {
            showControlBar();
        }
    }
    return QWidget::eventFilter(watched, event);
}

void VideoDisplay::showControlBar()
{
    m_hideTimer->start(); // 重置定时器
    if (m_isControlBarVisible){return;}
    m_opacityAnimation->stop();
    m_opacityAnimation->setDuration(200); // 200ms 淡入
    m_opacityAnimation->setStartValue(m_opacityEffect->opacity());
    m_opacityAnimation->setEndValue(1.0);
    m_opacityAnimation->start();
    m_isControlBarVisible = true;
}

void VideoDisplay::hideControlBar()
{
    if (!m_isControlBarVisible){return;}
    m_opacityAnimation->stop();
    m_opacityAnimation->setDuration(300); // 300ms 淡出
    m_opacityAnimation->setStartValue(m_opacityEffect->opacity());
    m_opacityAnimation->setEndValue(0.0);
    m_opacityAnimation->start();
    m_isControlBarVisible = false;
}

void VideoDisplay::respondToVideoSize(int w, int h)
{
    m_videoWidth=w;
    m_videoHeight=h;
    m_isPlaying=(w>0&&h>0);
    if(m_isPlaying)
    {
        initTextures();
    }
    qDebug()<<"成功获取显示分辨率:"<<w<<"x"<<h;
    // update();
}

void VideoDisplay::displayFrame(QByteArray d)
{
    // qDebug()<<"屏幕将显示:"<<d.size();
    m_yData=d.first(m_videoWidth*m_videoHeight);
    m_uData=d.sliced(m_videoWidth*m_videoHeight,m_videoWidth*m_videoHeight/4);
    m_vData=d.last(m_videoWidth*m_videoHeight/4);
    update();
}

void VideoDisplay::respondToMainDisconnect()
{
    m_isPlaying=false;
    m_videoWidth=m_videoHeight=0;
    m_yData.clear();
    m_uData.clear();
    m_vData.clear();
    update();
}

void VideoDisplay::respondToMainFullScreen(bool value)
{
    if(value)
    {
        m_fullScreenBtn->setIcon(QPixmap(":/svg/fullscreen_exit.svg"));
        m_fullScreenBtn->setToolTip("退出全屏");
        qDebug() << ">>> [操作] 进入全屏";
    }
    else
    {
        m_fullScreenBtn->setIcon(QPixmap(":/svg/fullscreen_enter.svg"));
        m_fullScreenBtn->setToolTip("全屏");
        qDebug() << ">>> [操作] 退出全屏";
    }
    m_isFullScreen=value;
}
