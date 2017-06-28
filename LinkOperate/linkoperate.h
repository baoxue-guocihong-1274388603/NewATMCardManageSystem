#ifndef LINKOPERATE_H
#define LINKOPERATE_H

#include <QObject>
#include <QTimer>
#include <QDebug>
#include "CommonSetting.h"
#include <signal.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <sys/mman.h>

#define USES_SHUAKAJI_WAIKE
//#define USES_JIAYOUZHAN_WAIKE


#ifdef USES_JIAYOUZHAN_WAIKE

#define LED_IOCTL_BASE   'W'

#define Led1_Red_On      _IOWR(LED_IOCTL_BASE, 0, int)
#define Led1_Green_On    _IOWR(LED_IOCTL_BASE, 1, int)
#define Led2_Red_On      _IOWR(LED_IOCTL_BASE, 2, int)
#define Led2_Green_On    _IOWR(LED_IOCTL_BASE, 3, int)
#define Led3_Red_On      _IOWR(LED_IOCTL_BASE, 4, int)
#define Led3_Green_On    _IOWR(LED_IOCTL_BASE, 5, int)

#define Led1_Red_Off     _IOWR(LED_IOCTL_BASE, 6, int)
#define Led1_Green_Off   _IOWR(LED_IOCTL_BASE, 7, int)
#define Led2_Red_Off     _IOWR(LED_IOCTL_BASE, 8, int)
#define Led2_Green_Off   _IOWR(LED_IOCTL_BASE, 9, int)
#define Led3_Red_Off     _IOWR(LED_IOCTL_BASE, 10, int)
#define Led3_Green_Off   _IOWR(LED_IOCTL_BASE, 11, int)

#endif

class LinkOperate : public QObject
{
    Q_OBJECT
public:
    explicit LinkOperate(QObject *parent = 0);
    ~LinkOperate();
    void OpenDevice();
    void BuzzerOn();
    void PowerLedOn();
    void RelayPowerOn();

public slots:
    void slotBuzzerOff();
    void slotPowerLedState();
    void slotRelayPowerOff();

public:
    int BuzzerFd;//蜂鸣器
    int PowerLedFd;//电源指示灯
    int RelayFd;//开关量

    QTimer *BuzzerTimer;
    QTimer *PowerLedTimer;
    QTimer *RelayTimer;//用来控制继电器
};

#endif // LINKOPERATE_H
