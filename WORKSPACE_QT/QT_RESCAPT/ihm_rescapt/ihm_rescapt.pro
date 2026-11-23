# IHM Reseau de Capteurs -- Qt5 or Qt6, Linux SocketCAN
QT += core gui widgets
greaterThan(QT_MAJOR_VERSION, 5): QT += openglwidgets
lessThan(QT_MAJOR_VERSION, 6): QT += opengl
CONFIG += c++17
TARGET = ihm_rescapt
TEMPLATE = app
INCLUDEPATH += .
SOURCES += main.cpp mainwindow.cpp objectgl.cpp widgets.cpp socketcan_cpp.cpp
HEADERS += mainwindow.h objectgl.h widgets.h socketcan_cpp.h socketcan_cpp_export.h can_protocol.h
LIBS += -lGL
