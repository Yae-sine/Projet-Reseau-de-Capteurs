#include "mainwindow.h"
#include "can_protocol.h"
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGroupBox>
#include <QStatusBar>
#include <QMenuBar>
#include <QButtonGroup>
#include <QtMath>
#include <cmath>
#include <linux/can.h>

static int16_t  s16(const uint8_t *b, int i) { return (int16_t)((b[i] << 8) | b[i + 1]); }
static uint16_t u16(const uint8_t *b, int i) { return (uint16_t)((b[i] << 8) | b[i + 1]); }
static uint32_t u32(const uint8_t *b, int i) { return ((uint32_t)b[i] << 24) | ((uint32_t)b[i+1] << 16) | ((uint32_t)b[i+2] << 8) | b[i+3]; }

MainWindow::MainWindow(const QString &iface, QWidget *parent) : QMainWindow(parent), ifaceName(iface)
{
    setWindowTitle("Réseau de Capteurs - IHM (bus CAN " + iface + ")");
    clock.start();

    QWidget *central = new QWidget;
    QGridLayout *grid = new QGridLayout(central);
    grid->addWidget(buildEnvGroup(), 0, 0, 1, 2);
    grid->addWidget(buildWindGroup(), 1, 0);
    grid->addWidget(buildImuGroup(), 1, 1);
    setCentralWidget(central);

    // status bar : one "LED" per board
    const char *names[3] = {"Carte 1 Moteur/Anémo", "Carte 2 VL6180X/Env", "Carte 3 IMU"};
    for (int i = 0; i < 3; i++) {
        boardLed[i] = new QLabel(names[i]);
        boardLed[i]->setMargin(3);
        statusBar()->addWidget(boardLed[i]);
    }
    busLabel = new QLabel;
    statusBar()->addPermanentWidget(busLabel);

    QMenu *view = menuBar()->addMenu("&Vue 3D");
    view->addAction("Face", gl, SLOT(FrontView()));
    view->addAction("Dessus", gl, SLOT(TopView()));
    view->addAction("Isométrique", gl, SLOT(IsometricView()));

    canOk = (can.open(iface.toStdString(), 1) == scpp::STATUS_OK);   // 1 ms read timeout

    QTimer *tRead = new QTimer(this);
    connect(tRead, &QTimer::timeout, this, &MainWindow::onReadTick);
    tRead->start(10);
    QTimer *tSend = new QTimer(this);
    connect(tSend, &QTimer::timeout, this, &MainWindow::onSendTick);
    tSend->start(200);                                   // periodic exchange IHM -> boards
    QTimer *tUi = new QTimer(this);
    connect(tUi, &QTimer::timeout, this, &MainWindow::onUiTick);
    tUi->start(250);
}

//------------------------------------------------------------------ UI
static QLabel *mkLabel(const QString &t = "--")
{
    QLabel *l = new QLabel(t);
    l->setStyleSheet("font-family: monospace; font-size: 11pt;");
    return l;
}

QWidget *MainWindow::buildEnvGroup()
{
    QGroupBox *g = new QGroupBox("Carte 2 : VL6180X (distance / luminosité) + LPS22HB (pression) + HTS221 (humidité)");
    QGridLayout *l = new QGridLayout(g);
    gVl = new Gauge("Distance", "mm", 0, 200);
    gPressure = new Gauge("Pression", "hPa", 950, 1050);
    gHumidity = new Gauge("Humidité", "%", 0, 100);
    gTemp = new Gauge("Température", "°C", -10, 50);
    l->addWidget(gVl, 0, 0); l->addWidget(gPressure, 0, 1); l->addWidget(gHumidity, 0, 2); l->addWidget(gTemp, 0, 3);

    rbDistance = new QRadioButton("Mesure de distance");
    rbLum = new QRadioButton("Mesure de luminosité");
    rbDistance->setChecked(true);
    QButtonGroup *bg = new QButtonGroup(g);
    bg->addButton(rbDistance); bg->addButton(rbLum);
    connect(rbDistance, &QRadioButton::toggled, this, [this](bool) { onSendTick(); });
    QVBoxLayout *mode = new QVBoxLayout;
    mode->addWidget(rbDistance); mode->addWidget(rbLum);
    l->addLayout(mode, 1, 0);

    lVl = mkLabel(); lPressure = mkLabel(); lHumidity = mkLabel(); lTemp = mkLabel();
    lAltitude = mkLabel(); lDew = mkLabel(); lMinMax = mkLabel();
    QGridLayout *txt = new QGridLayout;
    txt->addWidget(new QLabel("VL6180X :"), 0, 0);      txt->addWidget(lVl, 0, 1);
    txt->addWidget(new QLabel("Pression :"), 1, 0);     txt->addWidget(lPressure, 1, 1);
    txt->addWidget(new QLabel("Min / Max :"), 2, 0);    txt->addWidget(lMinMax, 2, 1);
    txt->addWidget(new QLabel("Altitude estimée :"), 3, 0); txt->addWidget(lAltitude, 3, 1);
    txt->addWidget(new QLabel("Humidité :"), 0, 2);     txt->addWidget(lHumidity, 0, 3);
    txt->addWidget(new QLabel("Température :"), 1, 2);  txt->addWidget(lTemp, 1, 3);
    txt->addWidget(new QLabel("Point de rosée :"), 2, 2); txt->addWidget(lDew, 2, 3);
    l->addLayout(txt, 1, 1, 1, 3);

    pPressure = new TrendPlot("Pression (hPa)", QColor(80, 170, 255));
    pTemp = new TrendPlot("Température (°C)", QColor(255, 120, 80));
    pHum = new TrendPlot("Humidité (%)", QColor(100, 220, 140));
    QHBoxLayout *plots = new QHBoxLayout;
    plots->addWidget(pPressure); plots->addWidget(pTemp); plots->addWidget(pHum);
    l->addLayout(plots, 2, 0, 1, 4);
    return g;
}

QWidget *MainWindow::buildWindGroup()
{
    QGroupBox *g = new QGroupBox("Carte 1 : Anémomètre + Moteur Dynamixel");
    QGridLayout *l = new QGridLayout(g);
    gWind = new Gauge("Vitesse du vent", "km/h", 0, 60);
    l->addWidget(gWind, 0, 0, 4, 1);

    cbMode = new QComboBox;
    cbMode->addItems({"Arrêt", "Manuel", "Automatique (seuil vent)"});
    slVelocity = new QSlider(Qt::Horizontal);
    slVelocity->setRange(-500, 500);
    lVelocity = mkLabel("0");
    connect(slVelocity, &QSlider::valueChanged, this, [this](int v) {
        lVelocity->setText(QString("%1 (%2 tr/min)").arg(v).arg(v * 0.229, 0, 'f', 1)); });
    sbThreshold = new QDoubleSpinBox;
    sbThreshold->setRange(0, 200); sbThreshold->setValue(10); sbThreshold->setSuffix(" km/h");
    connect(sbThreshold, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this,
            [this](double v) { gWind->setMarker(v); });
    gWind->setMarker(10);
    QPushButton *stop = new QPushButton("STOP");
    connect(stop, &QPushButton::clicked, this, [this] { cbMode->setCurrentIndex(0); slVelocity->setValue(0); onSendTick(); });

    l->addWidget(new QLabel("Mode moteur :"), 0, 1); l->addWidget(cbMode, 0, 2);
    l->addWidget(new QLabel("Vitesse manuelle :"), 1, 1); l->addWidget(slVelocity, 1, 2);
    l->addWidget(lVelocity, 2, 2);
    l->addWidget(new QLabel("Seuil vent (auto) :"), 3, 1); l->addWidget(sbThreshold, 3, 2);
    lWind = mkLabel(); lMotor = mkLabel();
    l->addWidget(lWind, 4, 0, 1, 3);
    l->addWidget(lMotor, 5, 0, 1, 2);
    l->addWidget(stop, 5, 2);
    pWind = new TrendPlot("Vent (km/h)", QColor(200, 200, 90));
    l->addWidget(pWind, 6, 0, 1, 3);
    return g;
}

QWidget *MainWindow::buildImuGroup()
{
    QGroupBox *g = new QGroupBox("Carte 3 : Centrale inertielle MPU9250 (Madgwick)");
    QGridLayout *l = new QGridLayout(g);
    gl = new ObjectOpenGL;
    l->addWidget(gl, 0, 0, 1, 2);
    lQuat = mkLabel(); lEuler = mkLabel(); lAcc = mkLabel(); lGyr = mkLabel();
    l->addWidget(new QLabel("Quaternion :"), 1, 0); l->addWidget(lQuat, 1, 1);
    l->addWidget(new QLabel("Roulis/Tangage/Lacet :"), 2, 0); l->addWidget(lEuler, 2, 1);
    l->addWidget(new QLabel("Accélération (mg) :"), 3, 0); l->addWidget(lAcc, 3, 1);
    l->addWidget(new QLabel("Gyroscope (°/s) :"), 4, 0); l->addWidget(lGyr, 4, 1);
    QPushButton *reset = new QPushButton("Réinitialiser l'orientation");
    connect(reset, &QPushButton::clicked, this, &MainWindow::onImuReset);
    l->addWidget(reset, 5, 0, 1, 2);
    return g;
}

//------------------------------------------------------------------ CAN
void MainWindow::sendFrame(uint32_t id, const uint8_t *d, uint8_t len)
{
    if (!canOk) return;
    scpp::CanFrame f;
    f.id = id; f.len = len;
    memcpy(f.data, d, len);
    if (can.write(f) == scpp::STATUS_OK) txCount++;
}

void MainWindow::onSendTick()
{
    uint8_t d[8];
    // board 1 : motor command
    int16_t v = (int16_t)slVelocity->value();
    uint16_t thr = (uint16_t)(sbThreshold->value() * 100.0);
    d[0] = (uint8_t)cbMode->currentIndex();
    d[1] = v >> 8; d[2] = v & 0xFF;
    d[3] = thr >> 8; d[4] = thr & 0xFF;
    sendFrame(CAN_ID_MOTOR_CMD, d, 5);
    // board 2 : VL6180X mode
    d[0] = rbLum->isChecked() ? 1 : 0;
    sendFrame(CAN_ID_VL_CMD, d, 1);
}

void MainWindow::onImuReset()
{
    uint8_t d[1] = {1};
    sendFrame(CAN_ID_IMU_CMD, d, 1);
}

void MainWindow::onReadTick()
{
    if (!canOk) return;
    scpp::CanFrame f;
    for (int n = 0; n < 200 && can.read(f) == scpp::STATUS_OK; n++) {
        rxCount++;
        handleFrame(f);
    }
}

void MainWindow::handleFrame(const scpp::CanFrame &f)
{
    if (f.id & (CAN_EFF_FLAG | CAN_RTR_FLAG | CAN_ERR_FLAG)) return;
    const uint8_t *d = f.data;
    qint64 now = clock.elapsed();
    switch (f.id & CAN_SFF_MASK) {
    case CAN_ID_WIND_STATUS: {
        if (f.len < 8) return;
        lastSeen[0] = now;
        double w = u16(d, 0) / 100.0;
        gWind->setValue(w);
        pWind->addSample(w);
        lWind->setText(QString("Vent %1 km/h (%2 impulsions / période)").arg(w, 0, 'f', 2).arg(u16(d, 2)));
        static const char *modes[3] = {"arrêt", "manuel", "auto"};
        lMotor->setText(QString("Moteur : %1, mode %2, consigne %3")
                        .arg(d[5] ? "EN ROTATION" : "arrêté").arg(modes[d[4] % 3]).arg(s16(d, 6)));
        break; }
    case CAN_ID_VL6180X: {
        if (f.len < 5) return;
        lastSeen[1] = now;
        uint32_t v = u32(d, 1);
        if (d[0] == 0) { gVl->setTitle("Distance", "mm"); gVl->setRange(0, 200);
                         lVl->setText(QString("distance = %1 mm").arg(v)); }
        else           { gVl->setTitle("Luminosité", "lux"); gVl->setRange(0, 1000);
                         lVl->setText(QString("luminosité = %1 lux").arg(v)); }
        gVl->setValue(v);
        break; }
    case CAN_ID_PRESSURE: {
        if (f.len < 6) return;
        lastSeen[1] = now;
        double p = u32(d, 0) / 100.0, t = s16(d, 4) / 100.0;
        pMin = qMin(pMin, p); pMax = qMax(pMax, p);
        gPressure->setValue(p);
        pPressure->addSample(p);
        lPressure->setText(QString("%1 hPa (T LPS = %2 °C)").arg(p, 0, 'f', 2).arg(t, 0, 'f', 2));
        lMinMax->setText(QString("%1 / %2 hPa").arg(pMin, 0, 'f', 2).arg(pMax, 0, 'f', 2));
        // international barometric formula (sea level 1013.25 hPa)
        double alt = 44330.0 * (1.0 - std::pow(p / 1013.25, 1.0 / 5.255));
        lAltitude->setText(QString("%1 m").arg(alt, 0, 'f', 1));
        break; }
    case CAN_ID_HUMIDITY: {
        if (f.len < 4) return;
        lastSeen[1] = now;
        hum = u16(d, 0) / 100.0; tempHts = s16(d, 2) / 100.0;
        gHumidity->setValue(hum); gTemp->setValue(tempHts);
        pHum->addSample(hum); pTemp->addSample(tempHts);
        lHumidity->setText(QString("%1 %RH").arg(hum, 0, 'f', 1));
        lTemp->setText(QString("%1 °C").arg(tempHts, 0, 'f', 2));
        if (hum > 0) {     // Magnus formula
            double gam = std::log(hum / 100.0) + 17.62 * tempHts / (243.12 + tempHts);
            lDew->setText(QString("%1 °C").arg(243.12 * gam / (17.62 - gam), 0, 'f', 1));
        }
        break; }
    case CAN_ID_IMU_QUAT: {
        if (f.len < 8) return;
        lastSeen[2] = now;
        double w = s16(d, 0) / 10000.0, x = s16(d, 2) / 10000.0, y = s16(d, 4) / 10000.0, z = s16(d, 6) / 10000.0;
        gl->setQuaternion(w, x, y, z);
        double roll  = qRadiansToDegrees(std::atan2(2 * (w * x + y * z), 1 - 2 * (x * x + y * y)));
        double pitch = qRadiansToDegrees(std::asin(qBound(-1.0, 2 * (w * y - z * x), 1.0)));
        double yaw   = qRadiansToDegrees(std::atan2(2 * (w * z + x * y), 1 - 2 * (y * y + z * z)));
        lQuat->setText(QString("%1 %2 %3 %4").arg(w, 7, 'f', 4).arg(x, 7, 'f', 4).arg(y, 7, 'f', 4).arg(z, 7, 'f', 4));
        lEuler->setText(QString("%1° %2° %3°").arg(roll, 7, 'f', 1).arg(pitch, 7, 'f', 1).arg(yaw, 7, 'f', 1));
        break; }
    case CAN_ID_IMU_ACCEL:
        if (f.len < 6) return;
        lastSeen[2] = now;
        lAcc->setText(QString("%1 %2 %3").arg(s16(d, 0), 6).arg(s16(d, 2), 6).arg(s16(d, 4), 6));
        break;
    case CAN_ID_IMU_GYRO:
        if (f.len < 6) return;
        lastSeen[2] = now;
        lGyr->setText(QString("%1 %2 %3").arg(s16(d, 0) / 10.0, 7, 'f', 1).arg(s16(d, 2) / 10.0, 7, 'f', 1).arg(s16(d, 4) / 10.0, 7, 'f', 1));
        break;
    default: break;
    }
}

void MainWindow::onUiTick()
{
    qint64 now = clock.elapsed();
    for (int i = 0; i < 3; i++) {
        bool on = now - lastSeen[i] < 1500;
        boardLed[i]->setStyleSheet(on ? "background:#2e7d32;color:white;" : "background:#8e2424;color:white;");
    }
    busLabel->setText(canOk ? QString("%1 : RX %2 / TX %3").arg(ifaceName).arg(rxCount).arg(txCount)
                            : QString("Impossible d'ouvrir %1 (voir README)").arg(ifaceName));
}
