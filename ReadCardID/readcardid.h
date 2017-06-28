#ifndef READCARDID_H
#define READCARDID_H

#include "usercontrol/persioninfocontrol.h"
#include "OperateCamera/operatecamera.h"
#include "LinkOperate/linkoperate.h"
#include "QextSerialPort/listenserialthread.h"
#include "CommonSetting.h"

//void signal_handler(int signum);

#ifdef USES_SHUAKAJI_WAIKE//如果用的是ATM刷卡机外壳

#define SHUAKAJI_IOC_MAGIC             	'M'
#define GET_STATE                 		_IOR(SHUAKAJI_IOC_MAGIC,1,int)

#endif

namespace Ui {
class ReadCardID;
}

class ReadCardID : public QWidget
{
    Q_OBJECT
public:
    explicit ReadCardID(QWidget *parent = 0);
    ~ReadCardID();

    void InitForm();

    void OpenDevice();

    void ParseAllSwipCardRecord();
    void CommonLinkOperateCode();

public slots:

#ifdef USES_SHUAKAJI_WAIKE//如果用的是ATM刷卡机外壳
    void slotReadState();
#endif

    void slotReadCardID();
    void slotClearAllRecord();

public:
    Ui::ReadCardID *ui;

    PersionInfoControl *first;
    PersionInfoControl *second;
    PersionInfoControl *third;
    PersionInfoControl *four;
    PersionInfoControl *five;
    PersionInfoControl *sex;

    QList<PersionInfoControl *> list;

    enum WorkMode{
        SwipCardWorkMode,//刷卡工作模式
        StandbyWorkMode,//待机工作模式
    };
    enum WorkMode WorkModeState;
    int TotalSwipCardCount;//刷卡工作模式下保存总的刷卡次数

    int WiegFd;
    unsigned char CardIDArray[3];//一个卡号，由三个字节组成
    QTimer *ReadCardIDTimer;//读卡号定时器
    QSqlQuery query;//数据库操作

    OperateCamera *operate_camera;//操作camera
    LinkOperate *link_operate;//联动操作
    ListenSerialThread *listen_serial;

    QStringList CardIDList;//用来保存加钞、巡检、接警操作的卡号序列
    QStringList CardTypeList;
    QStringList TriggerTimeList;//保存加钞、巡检、接警操作的刷卡触发时间

    QTimer *TimeOutClearTimer;//刷卡超时清零定时器

    qint8 index;

#ifdef USES_SHUAKAJI_WAIKE//如果用的是ATM刷卡机外壳
    quint8 isStop485Bus;//读取SmartuUsb设备或者地址模块的报警信息，会导致韦根26丢失中断，所以刷卡的过程中，需要停止读取SmartuUsb或者地址模块的报警信息
    QTimer *ReadStateTimer;
#endif
};

#endif // READCARDID_H
