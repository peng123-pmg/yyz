QT       += core gui sql
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets
CONFIG += c++11

# ====== OpenCV armv7 交叉编译配置（STM32MP1，带 face 模块） ======
OPENCV_PATH = /home/hqyj/opencv/build_armv7/install

INCLUDEPATH += $${OPENCV_PATH}/include/opencv4
LIBS += -L$${OPENCV_PATH}/lib

LIBS += \
    -lopencv_core \
    -lopencv_highgui \
    -lopencv_imgproc \
    -lopencv_imgcodecs \
    -lopencv_videoio \
    -lopencv_features2d \
    -lopencv_flann \
    -lopencv_calib3d \
    -lopencv_objdetect \
    -lopencv_dnn \
    -lopencv_face

# 如果报 __atomic_xxx 未定义，把下面这行取消注释
# LIBS += -latomic
# =================================================

SOURCES += \
    camera.c \
    main.cpp \
    mainwindow.cpp \
    admindialog.cpp \
    facedb.cpp \
    facerecognizerservice.cpp \
    cvqt_utils.cpp \
    face_recognizer_sdk.cpp

HEADERS += \
    camera.h \
    mainwindow.h \
    admindialog.h \
    facedb.h \
    facerecognizerservice.h \
    cvqt_utils.h \
    face_recognizer_sdk.h

FORMS += \
    mainwindow.ui

qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
