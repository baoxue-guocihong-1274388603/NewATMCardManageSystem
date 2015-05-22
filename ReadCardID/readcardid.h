#ifndef READCARDID_H
#define READCARDID_H

#include "OperateCamera/operatecamera.h"
#include "LinkOperate/linkoperate.h"
#include "CommonSetting.h"

class ReadCardID : public QObject
{
    Q_OBJECT
public:
    explicit ReadCardID(QObject *parent = 0);
    ~ReadCardID();
    void OpenDevice();
    void ParseAllSwipCardRecord();
    void CommonLinkOperateCode();

public slots:
    void slotReadCardID();
    void slotClearAllRecord();

public:
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
    QThread link_operate_thread;//实际执行联动工作的线程

    QStringList CardIDList;//用来保存加钞、巡检、接警操作的卡号序列
    QStringList CardTypeList;
    QStringList TriggerTimeList;//保存加钞、巡检、接警操作的刷卡触发时间

    QTimer *TimeOutClearTimer;//刷卡超时清零定时器
};

#endif // READCARDID_H
