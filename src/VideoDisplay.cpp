#include "VideoDisplay.h"
#include <QPainter>
#include <QPainterPath>
#include <QDebug>
#include <QSurfaceFormat>
VideoDisplay::VideoDisplay(QWidget* parent):QOpenGLWidget(parent)
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
    qDebug()<<"显示1帧...";
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    updateGeometry();

    m_prog->bind();
    // 设置缩放
    m_prog->setUniformValue("u_scale",m_scale);
    // 绑定Y纹理
    glActiveTexture(GL_TEXTURE0);
    m_yTexture->bind();
    m_yTexture->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt8,m_yData.constData(), nullptr);
    m_prog->setUniformValue("tex_y", 0);
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

    m_prog->release();

    if(!m_textPromt.isEmpty())
    {
        drawPromt();
    }
}

void VideoDisplay::resizeGL(int w, int h)
{
    if(m_isPlaying==false)
        return;
    updateGeometry();
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
    // out vec4 DebugColor;
    void main(void)
    {
        vec3 yuv;
        vec3 rgb;
        yuv.x = texture2D(tex_y, texCoord).r;
        yuv.y = texture2D(tex_u, texCoord).r - 0.5;
        yuv.z = texture2D(tex_v, texCoord).r - 0.5;
        rgb=yuv*bt709;
        gl_FragColor = vec4(rgb, 1.0);

        // DebugColor = vec4(texture2D(tex_y, texCoord).rrr, 1.0);
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

void VideoDisplay::updateGeometry()
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
