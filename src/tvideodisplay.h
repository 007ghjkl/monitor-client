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
    void updateGeometry();

    void drawNoSignal();
    void drawPromt();
public slots:
    void respondToVideoSize(int w,int h);
    void displayFrame(QByteArray d);
    void respondToMainDisconnect();
};

#endif // TVIDEODISPLAY_H
