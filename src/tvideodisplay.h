#ifndef TVIDEODISPLAY_H
#define TVIDEODISPLAY_H

#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOpenGLTexture>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include <QScopedPointer>

class TVideoDisplay : public QOpenGLWidget,protected QOpenGLFunctions
{
    Q_OBJECT
public:
    TVideoDisplay(QWidget* parent);
    virtual ~TVideoDisplay();
protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w,int h) override;
private:
    int videoWidth;
    int videoHeight;
    int viewportX, viewportY, viewportWidth, viewportHeight;
    QByteArray yData;
    QByteArray uData;
    QByteArray vData;
    QOpenGLShaderProgram *prog;//着色器程序
    QOpenGLVertexArrayObject *vao;//顶点数组对象
    QScopedPointer<QOpenGLBuffer> vbo;//顶点缓冲对象
    QScopedPointer<QOpenGLTexture> yTexture;
    QScopedPointer<QOpenGLTexture> uTexture;
    QScopedPointer<QOpenGLTexture> vTexture;
    QByteArray vsh;
    QByteArray fsh;

    bool isPlaying;
    QString textNoSignal;
    QString textPromt;

    void initShaders();
    void initGeometry();
    void initTextures();
    void updateGeometry();

    void drawNoSignal();
    void drawPromt();
public slots:
    void respondToVideoSize(int w,int h);
    void displayFrame(QByteArray d);
    void respondToMainDisconnect();
};

#endif // TVIDEODISPLAY_H
