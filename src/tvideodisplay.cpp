#include "tvideodisplay.h"
#include <QPainter>
#include <QPainterPath>
#include <QDebug>
TVideoDisplay::TVideoDisplay(QWidget* parent):QOpenGLWidget(parent)
{
    isPlaying=false;
    textNoSignal="无信号";
    textPromt="提示";
}

TVideoDisplay::~TVideoDisplay()
{
    makeCurrent();
    vao->destroy();
    vbo->destroy();
    if(yTexture)
    {
        yTexture->destroy();
        uTexture->destroy();
        vTexture->destroy();
    }
    doneCurrent();
}

void TVideoDisplay::initializeGL()
{
    initializeOpenGLFunctions();
    initShaders();
    initGeometry();
    //有输入再initTextures()
    glClear(GL_COLOR_BUFFER_BIT);
}

void TVideoDisplay::paintGL()
{
    if(isPlaying==false)
    {
        drawNoSignal();
        return;
    }
    qDebug()<<"显示1帧...";
    glClear(GL_COLOR_BUFFER_BIT);
    updateGeometry();

    prog->bind();

    // 绑定Y纹理
    glActiveTexture(GL_TEXTURE0);
    yTexture->bind();
    yTexture->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt8,yData.constData(), nullptr);
    prog->setUniformValue("tex_y", 0);
    // 绑定U纹理
    glActiveTexture(GL_TEXTURE1);
    uTexture->bind();
    uTexture->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt8,uData.constData(), nullptr);
    prog->setUniformValue("tex_u", 1);
    // 绑定V纹理
    glActiveTexture(GL_TEXTURE2);
    vTexture->bind();
    vTexture->setData(QOpenGLTexture::Red, QOpenGLTexture::UInt8,vData.constData(), nullptr);
    prog->setUniformValue("tex_v", 2);
    // 绘制矩形
    vao->bind();
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
    glFlush();
    vao->release();

    prog->release();

    if(!textPromt.isEmpty())
    {
        drawPromt();
    }
}

void TVideoDisplay::resizeGL(int w, int h)
{
    if(isPlaying==false)
        return;
    updateGeometry();
}

void TVideoDisplay::initShaders()
{
    vsh=R"(
    attribute vec2 aPos;
    attribute vec2 aTexCoord;
    varying vec2 texCoord;
    void main(void)
    {
        gl_Position = vec4(aPos,0.0,1.0);
        texCoord=aTexCoord;
    }
    )";
    fsh=R"(
    varying vec2 texCoord;
    uniform sampler2D tex_y;
    uniform sampler2D tex_u;
    uniform sampler2D tex_v;
    const mat3 yuv2rgb=mat3( 1,       1,         1,
                             0,       -0.39465,  2.03211,
                             1.13983, -0.58060,  0);
    // out vec4 DebugColor;
    void main(void)
    {
        vec3 yuv;
        vec3 rgb;
        yuv.x = texture2D(tex_y, texCoord).r;
        yuv.y = texture2D(tex_u, texCoord).r - 0.5;
        yuv.z = texture2D(tex_v, texCoord).r - 0.5;
        rgb=yuv2rgb*yuv;
        gl_FragColor = vec4(rgb, 1.0);

        // DebugColor = vec4(texture2D(tex_y, texCoord).rrr, 1.0);
    }
    )";
    prog=new QOpenGLShaderProgram(this);
    if(!(prog->addShaderFromSourceCode(QOpenGLShader::Vertex,vsh)))
    {
        qDebug() << "顶点着色器:" << prog->log();
        return;
    }
    if(!(prog->addShaderFromSourceCode(QOpenGLShader::Fragment,fsh)))
    {
        qDebug() << "片元着色器:" << prog->log();
        return;
    }
    if(!(prog->link()))
    {
        qDebug() << "着色器链接时:" << prog->log();
        return;
    }
}

void TVideoDisplay::initGeometry()
{
    GLfloat vertices[] = {
        // 位置          // 纹理坐标
        -1.0f,  1.0f,   0.0f, 0.0f,  // 左上
        -1.0f, -1.0f,   0.0f, 1.0f,  // 左下
        1.0f,  1.0f,   1.0f, 0.0f,  // 右上
        1.0f, -1.0f,   1.0f, 1.0f   // 右下
    };
    vao=new QOpenGLVertexArrayObject(this);
    vao->create();
    vao->bind();
    vbo.reset(new QOpenGLBuffer);
    vbo->create();
    vbo->bind();
    vbo->allocate(vertices, sizeof(vertices));

    prog->bind();    
    prog->enableAttributeArray(0);// 位置属性
    prog->setAttributeBuffer(0, GL_FLOAT, 0, 2, 4 * sizeof(GLfloat));    
    prog->enableAttributeArray(1);// 纹理坐标属性
    prog->setAttributeBuffer(1, GL_FLOAT, 2 * sizeof(GLfloat), 2, 4 * sizeof(GLfloat));
    prog->release();

    vbo->release();
    vao->release();
}

void TVideoDisplay::initTextures()
{
    yTexture.reset(new QOpenGLTexture(QOpenGLTexture::Target2D));
    yTexture->setFormat(QOpenGLTexture::R8_UNorm);//单通道纹理
    yTexture->setMinificationFilter(QOpenGLTexture::Linear);//双线性插值
    yTexture->setMagnificationFilter(QOpenGLTexture::Linear);
    yTexture->setWrapMode(QOpenGLTexture::ClampToEdge);//纹理坐标超出[0,1]范围时，使用边缘值而不是重复纹理
    yTexture->setSize(videoWidth, videoHeight);// 全分辨率
    yTexture->allocateStorage(QOpenGLTexture::Red, QOpenGLTexture::UInt8);

    uTexture.reset(new QOpenGLTexture(QOpenGLTexture::Target2D));
    uTexture->setFormat(QOpenGLTexture::R8_UNorm);
    uTexture->setMinificationFilter(QOpenGLTexture::Linear);
    uTexture->setMagnificationFilter(QOpenGLTexture::Linear);
    uTexture->setWrapMode(QOpenGLTexture::ClampToEdge);
    uTexture->setSize(videoWidth/2,videoHeight/2);//半分辨率
    uTexture->allocateStorage(QOpenGLTexture::Red,QOpenGLTexture::UInt8);

    vTexture.reset(new QOpenGLTexture(QOpenGLTexture::Target2D));
    vTexture->setFormat(QOpenGLTexture::R8_UNorm);
    vTexture->setMinificationFilter(QOpenGLTexture::Linear);
    vTexture->setMagnificationFilter(QOpenGLTexture::Linear);
    vTexture->setWrapMode(QOpenGLTexture::ClampToEdge);
    vTexture->setSize(videoWidth/2,videoHeight/2);//半分辨率
    vTexture->allocateStorage(QOpenGLTexture::Red,QOpenGLTexture::UInt8);
}

void TVideoDisplay::updateGeometry()
{
    glClear(GL_COLOR_BUFFER_BIT);

    int widgetWidth = width()*2;
    int widgetHeight = height()*2;
    float widgetAspect = (float)widgetWidth / widgetHeight;
    float videoAspect = (float)videoWidth / videoHeight;

    if (widgetAspect > videoAspect) {
        // 窗口更宽，左右黑边
        viewportHeight = widgetHeight;
        viewportWidth = widgetHeight * videoAspect;
        viewportX = (widgetWidth - viewportWidth)/2;
        viewportY = 0;
    } else {
        // 窗口更高，上下黑边
        viewportWidth = widgetWidth;
        viewportHeight = widgetWidth / videoAspect;
        viewportX = 0;
        viewportY = (widgetHeight - viewportHeight)/2;
    }
    // qDebug()<<widgetWidth<<widgetHeight<<viewportWidth<<viewportHeight<<viewportX<<viewportY;
    // 设置视口到视频区域
    glViewport(viewportX,viewportY, viewportWidth, viewportHeight);
}

void TVideoDisplay::drawNoSignal()
{
    // isPlaying=false;
    setPalette(QPalette(QColorConstants::Black));

    setAutoFillBackground(true);
    int W= width();
    int H= height();
    // qDebug()<<W<<H;
    QPainter painter(this);
    QFont font;
    font.setPointSize(30);
    font.setBold(true);
    painter.setFont(font);
    painter.drawText(QRectF(W/4,H/4,W/2,H/2),Qt::AlignCenter,textNoSignal);
    setAutoFillBackground(false);
}

void TVideoDisplay::drawPromt()
{
    QPainter painter(this);
    painter.setViewport(viewportX/2, viewportY/2, viewportWidth, viewportHeight);
    painter.setWindow(0,0,viewportWidth,viewportHeight);
    // qDebug()<<viewportWidth<<viewportHeight<<viewportX<<viewportY;
    QPointF pos(12,24);

    QPen pen=painter.pen();
    pen.setWidth(3);
    pen.setColor(Qt::black);
    painter.setPen(pen);
    QFont font=painter.font();
    font.setPointSize(12);
    painter.setFont(font);
    QPainterPath path;
    path.addText(pos,font,textPromt);
    painter.drawPath(path);//黑边

    pen.setColor(Qt::white);
    pen.setWidth(1);
    painter.setPen(pen);
    painter.drawText(pos,textPromt);//白芯
}

void TVideoDisplay::respondToVideoSize(int w, int h)
{
    videoWidth=w;
    videoHeight=h;
    isPlaying=(w>0&&h>0);
    if(isPlaying)
    {
        initTextures();
    }
    qDebug()<<"成功获取显示分辨率:"<<w<<"x"<<h;
    // update();
}

void TVideoDisplay::displayFrame(QByteArray d)
{
    // qDebug()<<"屏幕将显示:"<<d.size();
    yData=d.first(videoWidth*videoHeight);
    uData=d.sliced(videoWidth*videoHeight,videoWidth*videoHeight/4);
    vData=d.last(videoWidth*videoHeight/4);
    update();
}

void TVideoDisplay::respondToMainDisconnect()
{
    isPlaying=false;
    videoWidth=videoHeight=0;
    yData.clear();
    uData.clear();
    vData.clear();
    update();
}
