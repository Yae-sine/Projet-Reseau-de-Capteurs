#ifndef OBJECTGL_H
#define OBJECTGL_H
// 3D view of the IMU board (port of QT_OPENGL example to QOpenGLWidget)
#include <QOpenGLWidget>
#include <QOpenGLFunctions>
#include <QMouseEvent>
#include <QWheelEvent>

class ObjectOpenGL : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit ObjectOpenGL(QWidget *parent = nullptr);
    void setQuaternion(double w, double x, double y, double z) { q[0]=w; q[1]=x; q[2]=y; q[3]=z; update(); }
public slots:
    void FrontView();
    void TopView();
    void IsometricView();
protected:
    void initializeGL() override;
    void paintGL() override;
    void resizeGL(int w, int h) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;
private:
    void Draw_Frame();
    void Draw_Box();
    void setRot(int x, int y, int z) { xRot = x; yRot = y; zRot = z; update(); }
    QSize WindowSize;
    QPoint LastPos;
    float dx = 0, dy = 0, Zoom = 0.5f;
    int xRot = 0, yRot = 0, zRot = 0;   // in 1/16 degree
    double q[4] = {1, 0, 0, 0};
};
#endif
