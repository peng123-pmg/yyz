QT += widgets

CONFIG += c++17

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    camera.cpp \
    devicecontrol.cpp \
    fandialog.cpp \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    Beeper.h \
    Fan.h \
    Vibrator.h \
    camera.h \
    devicecontrol.h \
    fandialog.h \
    hardware_def.h \
    leds.h \
    mainwindow.h \
    phtotosens.h \
    temp_hum.h

FORMS += \
    camera.ui \
    fandialog.ui \
    mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    images.qrc
