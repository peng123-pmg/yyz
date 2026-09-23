QT       += core gui network mqtt widgets

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
CONFIG += c++17

TARGET = smart_home
TEMPLATE = app

SOURCES += \
    main.cpp \
    mainwindow.cpp \
    camera.cpp \
    fandialog.cpp

HEADERS += \
    HardwareController.h \
    Beeper.h \
    Vibrator.h \
    light.h \
    Fan.h \
    temp_hum.h \
    mainwindow.h \
    camera.h \
    fandialog.h

FORMS += \
    mainwindow.ui \
    camera.ui \
    fandialog.ui

RESOURCES += \
    images.qrc

qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
