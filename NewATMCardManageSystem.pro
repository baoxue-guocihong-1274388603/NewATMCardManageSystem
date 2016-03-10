#-------------------------------------------------
#
# Project created by QtCreator 2013-09-09T15:16:17
#
#-------------------------------------------------

QT       += core  network xml sql

TARGET = NewATMCardManageSystem
TEMPLATE = app


SOURCES += main.cpp\
    QextSerialPort/qextserialport.cpp \
    QextSerialPort/qextserialport_unix.cpp \
    OperateCamera/operatecamera.cpp \
    ReadCardID/readcardid.cpp \
    LinkOperate/linkoperate.cpp \
    QextSerialPort/listenserialthread.cpp \
    TcpThread/tcpcommunicate.cpp \
    usercontrol/persioninfocontrol.cpp \
    downloadcertificatepic.cpp

HEADERS  += \
    QextSerialPort/qextserialport.h \
    QextSerialPort/qextserialport_global.h \
    QextSerialPort/qextserialport_p.h \
    OperateCamera/operatecamera.h \
    ReadCardID/readcardid.h \
    LinkOperate/linkoperate.h \
    QextSerialPort/listenserialthread.h \
    TcpThread/tcpcommunicate.h \
    usercontrol/persioninfocontrol.h \
    downloadcertificatepic.h

DEFINES += WITH_OPENSSL

DESTDIR=bin
MOC_DIR=temp/moc
RCC_DIR=temp/rcc
UI_DIR=temp/ui
OBJECTS_DIR=temp/obj

FORMS += \
    ReadCardID/readcardid.ui \
    usercontrol/persioninfocontrol.ui
