#include "linkoperate.h"

LinkOperate::LinkOperate(QObject *parent) :
    QObject(parent)
{
    OpenDevice();

    BuzzerTimer = new QTimer;
    BuzzerTimer->setInterval(8000);
    connect(BuzzerTimer,SIGNAL(timeout()),this,SLOT(slotBuzzerOff()));

    PowerLedTimer = new QTimer;
    quint32 PowerLedTime = 500;//电源指示灯闪烁频率
    PowerLedTimer->setInterval(PowerLedTime);
    connect(PowerLedTimer,SIGNAL(timeout()),this,SLOT(slotPowerLedState()));
    PowerLedTimer->start();

    RelayTimer = new QTimer;
    quint32 time = CommonSetting::ReadSettings("/bin/config.ini","time/RelayOnTime").toUInt() * 1000;//继电器通电时间
    RelayTimer->setInterval(time);
    connect(RelayTimer,SIGNAL(timeout()),this,SLOT(slotRelayPowerOff()));
}

LinkOperate::~LinkOperate()
{
    delete BuzzerTimer;
    delete PowerLedTimer;
    delete RelayTimer;
}

void LinkOperate::OpenDevice()
{

    BuzzerFd = open("/dev/s5pv210_buzzer",O_RDWR);
    if(BuzzerFd == -1){
        qDebug() << "/dev/s5pv210_buzzer failed.Exit the Program";
    }

    PowerLedFd = open("/dev/s5pv210_led",O_RDWR);
    if(PowerLedFd == -1){
        qDebug() << "/dev/s5pv210_led failed.Exit the Program";
    }

    RelayFd = open("/dev/s5pv210_relay",O_RDWR);
    if(RelayFd == -1){
        qDebug() << "/dev/s5pv210_relay failed.Exit the Program";
    }
}

void LinkOperate::slotBuzzerOff()
{
    BuzzerTimer->stop();
    ioctl(BuzzerFd,0);
}

void LinkOperate::slotPowerLedState()
{
    static bool flag = true;
    flag = !flag;
#ifdef USES_SHUAKAJI_WAIKE  //如果用的是刷卡机外壳
    ioctl(PowerLedFd,flag ? 1 : 0);
#endif
#ifdef USES_JIAYOUZHAN_WAIKE//如果用的是加油站外壳
    ioctl(PowerLedFd,flag ? 5 : 11);
#endif
}

void LinkOperate::slotRelayPowerOff()
{
    RelayTimer->stop();
    ioctl(RelayFd,0);
}

void LinkOperate::BuzzerOn()
{
    ioctl(BuzzerFd,1,3000);
}

void LinkOperate::PowerLedOn()
{
#ifdef USES_SHUAKAJI_WAIKE  //如果用的是刷卡机外壳
    ioctl(PowerLedFd,1);
#endif
#ifdef USES_JIAYOUZHAN_WAIKE//如果用的是加油站外壳
    ioctl(PowerLedFd,5);
#endif
}

void LinkOperate::RelayPowerOn()
{
    ioctl(RelayFd,1);
}
