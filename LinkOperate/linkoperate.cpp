#include "linkoperate.h"
#define WITH_DEBUG1

LinkOperate::LinkOperate(QObject *parent) :
    QObject(parent)
{
    OpenDevice();

//    DoorTimer = new QTimer;
//    DoorTimer->setInterval(1000);
//    connect(DoorTimer,SIGNAL(timeout()),this,SLOT(slotDoorState()));
//    //检测防拆是否被打开
//    DoorTimer->start();

    BuzzerTimer = new QTimer;
    BuzzerTimer->setInterval(5000);
    connect(BuzzerTimer,SIGNAL(timeout()),this,SLOT(slotBuzzerOff()));

    PowerLedTimer = new QTimer;
    quint32 PowerLedTime = CommonSetting::ReadSettings("/bin/config.ini","time/PowerLedTime").toUInt() * 500;//电源指示灯闪烁频率
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
//    delete DoorTimer;
    delete BuzzerTimer;
    delete PowerLedTimer;
    delete RelayTimer;
}

void LinkOperate::OpenDevice()
{
    DoorFd = open("/dev/s5pv210_door",O_RDWR);
    if(DoorFd == -1){
        qDebug() << "/dev/s5pv210_door failed.Exit the Program";
        exit(0);
    }

    BuzzerFd = open("/dev/s5pv210_buzzer",O_RDWR);
    if(BuzzerFd == -1){
        qDebug() << "/dev/s5pv210_buzzer failed.Exit the Program";
        exit(0);
    }

    PowerLedFd = open("/dev/s5pv210_power_led",O_RDWR);
    if(PowerLedFd == -1){
        qDebug() << "/dev/s5pv210_led failed.Exit the Program";
        exit(0);
    }

    RelayFd = open("/dev/s5pv210_relay",O_RDWR);
    if(RelayFd == -1){
        qDebug() << "/dev/s5pv210_relay failed.Exit the Program";
        exit(0);
    }
}

void LinkOperate::slotDoorState()
{
    char DoorState;
    static bool isAlarm = false;

    read(DoorFd,&DoorState,1);
    if(DoorState != 0 && !isAlarm){
        this->BuzzerOn();
        QTimer::singleShot(60*1000,this,SLOT(slotBuzzerOff()));
        isAlarm = true;
    }else if(DoorState == 0 && isAlarm){
        this->slotBuzzerOff();
        isAlarm = false;
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
    ioctl(PowerLedFd,flag ? 1 : 0);
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
    ioctl(PowerLedFd,1);
}

void LinkOperate::RelayPowerOn()
{
    ioctl(RelayFd,1);
}
