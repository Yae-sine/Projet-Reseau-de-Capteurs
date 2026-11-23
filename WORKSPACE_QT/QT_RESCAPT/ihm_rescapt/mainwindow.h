#ifndef MAINWINDOW_H
#define MAINWINDOW_H
#include <QMainWindow>
#include <QLabel>
#include <QTimer>
#include <QElapsedTimer>
#include <QComboBox>
#include <QSlider>
#include <QDoubleSpinBox>
#include <QRadioButton>
#include <QPushButton>
#include "socketcan_cpp.h"
#include "widgets.h"
#include "objectgl.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(const QString &iface, QWidget *parent = nullptr);
private slots:
    void onReadTick();      // drain the CAN socket
    void onSendTick();      // periodic commands to the boards
    void onUiTick();        // board online/offline status
    void onImuReset();
private:
    QWidget *buildEnvGroup();
    QWidget *buildWindGroup();
    QWidget *buildImuGroup();
    void handleFrame(const scpp::CanFrame &f);
    void sendFrame(uint32_t id, const uint8_t *d, uint8_t len);

    scpp::SocketCan can;
    bool canOk = false;
    QString ifaceName;
    QElapsedTimer clock;
    qint64 lastSeen[3] = {-100000, -100000, -100000};
    long rxCount = 0, txCount = 0;
    QLabel *boardLed[3];
    QLabel *busLabel;

    // board 2
    Gauge *gPressure, *gVl, *gHumidity, *gTemp;
    QLabel *lVl, *lPressure, *lHumidity, *lTemp, *lAltitude, *lDew, *lMinMax;
    QRadioButton *rbDistance, *rbLum;
    TrendPlot *pPressure, *pTemp, *pHum;
    double hum = NAN, tempHts = NAN, pMin = 1e9, pMax = -1e9;
    // board 1
    Gauge *gWind;
    QLabel *lWind, *lMotor;
    QComboBox *cbMode;
    QSlider *slVelocity;
    QLabel *lVelocity;
    QDoubleSpinBox *sbThreshold;
    TrendPlot *pWind;
    // board 3
    ObjectOpenGL *gl;
    QLabel *lQuat, *lEuler, *lAcc, *lGyr;
};
#endif
