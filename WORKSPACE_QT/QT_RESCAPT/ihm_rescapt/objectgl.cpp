#include "objectgl.h"
#include <GL/gl.h>

ObjectOpenGL::ObjectOpenGL(QWidget *parent) : QOpenGLWidget(parent)
{
    setMinimumSize(320, 260);
    IsometricView();
}

void ObjectOpenGL::initializeGL()
{
    initializeOpenGLFunctions();
    glClearColor(0.f, 0.f, 0.f, 1.f);
    glShadeModel(GL_FLAT);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void ObjectOpenGL::resizeGL(int w, int h) { WindowSize = QSize(w, h); }

void ObjectOpenGL::paintGL()
{
    glViewport(0, 0, WindowSize.width() * devicePixelRatio(), WindowSize.height() * devicePixelRatio());
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    GLfloat ratio = WindowSize.height() ? (GLfloat)WindowSize.width() / WindowSize.height() : 1.f;
    glOrtho((-0.5 + dx) * ratio, (0.5 + dx) * ratio, +0.5 + dy, -0.5 + dy, -1500.0, 1500.0);
    glMatrixMode(GL_MODELVIEW);

    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glLoadIdentity();
    glRotated(xRot / 16.0, 1, 0, 0);
    glRotated(yRot / 16.0, 0, 1, 0);
    glRotated(zRot / 16.0, 0, 0, 1);
    glScalef(1, -1, 1);
    Draw_Frame();
    glPushMatrix();
    glScalef(Zoom, Zoom, Zoom);
    Draw_Box();
    glPopMatrix();
}

#define L1 0.9
#define L2 0.4
#define L3 0.2

void ObjectOpenGL::Draw_Box()
{
    // rotation matrix (body -> earth) from the Madgwick quaternion
    double w = q[0], x = q[1], y = q[2], z = q[3];
    GLdouble m[16] = {
        1 - 2*(y*y + z*z), 2*(x*y + w*z),     2*(x*z - w*y),     0,
        2*(x*y - w*z),     1 - 2*(x*x + z*z), 2*(y*z + w*x),     0,
        2*(x*z + w*y),     2*(y*z - w*x),     1 - 2*(x*x + y*y), 0,
        0, 0, 0, 1 };
    glPushMatrix();
    glMultMatrixd(m);

    glBegin(GL_POLYGON); glColor4ub(0, 0, 255, 160);
    glVertex3d(-L1, -L2, -L3); glVertex3d(-L1, L2, -L3); glVertex3d(L1, L2, 0); glVertex3d(L1, -L2, 0); glEnd();
    glBegin(GL_POLYGON); glColor4ub(51, 255, 0, 160);
    glVertex3d(-L1, -L2, L3); glVertex3d(L1, -L2, 0); glVertex3d(L1, L2, 0); glVertex3d(-L1, L2, L3); glEnd();
    glBegin(GL_POLYGON); glColor4ub(255, 255, 0, 160);
    glVertex3d(-L1, -L2, L3); glVertex3d(-L1, L2, L3); glVertex3d(-L1, L2, -L3); glVertex3d(-L1, -L2, -L3); glEnd();
    glBegin(GL_TRIANGLES); glColor4ub(255, 102, 0, 160);
    glVertex3d(-L1, L2, L3); glVertex3d(L1, L2, 0); glVertex3d(-L1, L2, -L3); glEnd();
    glBegin(GL_TRIANGLES); glColor4ub(0, 255, 255, 160);
    glVertex3d(-L1, -L2, L3); glVertex3d(-L1, -L2, -L3); glVertex3d(L1, -L2, 0); glEnd();
    glPopMatrix();
}

void ObjectOpenGL::Draw_Frame()
{
    glLineWidth(4.0);
    glBegin(GL_LINES);
    glColor4ub(255, 64, 64, 200);  glVertex3d(0, 0, 0); glVertex3d(0.25, 0, 0);
    glColor4ub(64, 255, 64, 200);  glVertex3d(0, 0, 0); glVertex3d(0, 0.25, 0);
    glColor4ub(64, 64, 255, 200);  glVertex3d(0, 0, 0); glVertex3d(0, 0, 0.25);
    glEnd();
}

void ObjectOpenGL::FrontView()     { setRot(0, 0, 0); Zoom = 1; dx = dy = 0; }
void ObjectOpenGL::TopView()       { setRot(-90 * 16, 0, 0); Zoom = 0.5f; dx = dy = 0; }
void ObjectOpenGL::IsometricView() { setRot(62 * 16, 0, 45 * 16); Zoom = 0.5f; dx = dy = 0; }

void ObjectOpenGL::mousePressEvent(QMouseEvent *e) { LastPos = e->pos(); }

void ObjectOpenGL::mouseMoveEvent(QMouseEvent *e)
{
    int mx = e->pos().x() - LastPos.x(), my = e->pos().y() - LastPos.y();
    if (e->buttons() & Qt::LeftButton) {
        dx -= mx / (float)qMax(1, WindowSize.width());
        dy -= my / (float)qMax(1, WindowSize.height());
    }
    if (e->buttons() & Qt::RightButton) { xRot -= 4 * my; yRot += 4 * mx; }
    LastPos = e->pos();
    update();
}

void ObjectOpenGL::wheelEvent(QWheelEvent *e)
{
    int d = e->angleDelta().y();
    if (d < 0) Zoom /= 1 - (d / 120.0) / 10.0;
    if (d > 0) Zoom *= 1 + (d / 120.0) / 10.0;
    update();
}
