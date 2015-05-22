#include "readcardid.h"
#define WITH_DEBUG1

ReadCardID::ReadCardID(QObject *parent) :
    QObject(parent)
{
    WorkModeState = ReadCardID::StandbyWorkMode;
    TotalSwipCardCount = 0;

    OpenDevice();

    operate_camera = new OperateCamera(this);

    link_operate = new LinkOperate;
    link_operate->moveToThread(&link_operate_thread);
    link_operate_thread.start();

    quint32 MaxTime = CommonSetting::ReadSettings("/bin/config.ini","time/MaxTime").toUInt() * 1000;
    TimeOutClearTimer = new QTimer(this);
    TimeOutClearTimer->setInterval(MaxTime);
    connect(TimeOutClearTimer,SIGNAL(timeout()),this,SLOT(slotClearAllRecord()));

    ReadCardIDTimer = new QTimer(this);
    ReadCardIDTimer->setInterval(500);
    connect(ReadCardIDTimer,SIGNAL(timeout()),this,SLOT(slotReadCardID()));
    ReadCardIDTimer->start();

    //表示启动完成
    link_operate->BuzzerOn();
    link_operate->BuzzerTimer->start();
}

ReadCardID::~ReadCardID()
{
    ::close(WiegFd);
}

void ReadCardID::OpenDevice()
{
    WiegFd = open("/dev/s5pv210_wieg1",O_RDWR | O_NONBLOCK);//打开外部韦根设备文件
    if(WiegFd < 0){
        qDebug() << "cannot open device /dev/s5pv210_wieg1";
        exit(EXIT_FAILURE);
    }
}

void ReadCardID::slotReadCardID()
{
    unsigned char *ptr = CardIDArray;
    ssize_t ret;
    size_t len = sizeof(CardIDArray);
    while(len != 0 && (ret = read(WiegFd,ptr,len)) != 0){
        if(ret == -1){
            if(errno == EINTR)
                continue;
            return;
        }
        len -= ret;
        ptr += ret;
    }

    QString TriggerTime = CommonSetting::GetCurrentDateTime();
    QString CardIDHeader = QString::number(CardIDArray[0],10);
    while(CardIDHeader.length() < 3){
        CardIDHeader = QString("0") + CardIDHeader;
    }
    QString CardIDTail = QString("%1").arg((CardIDArray[1] << 8) + CardIDArray[2]);
    while(CardIDTail.length() < 5){
        CardIDTail = QString("0") + CardIDTail;
    }
    QString CardID = CardIDHeader + CardIDTail;

    qDebug() << QString("TriggerTime:") << TriggerTime;
    qDebug() << QString("CardID:") << CardID;

    //待机工作模式下刷一张卡要检测卡类型
    if(WorkModeState == ReadCardID::StandbyWorkMode){
        //如果该卡在数据库中不存在,此次操作无影响；如果数据库中存在,就要判断该卡的动作号是什么
        query.exec(tr("SELECT [动作号] FROM [功能表] WHERE [功能号] = (SELECT [功能号] FROM [卡号表] WHERE [卡号] = \"%1\")").arg(CardID));
        while(query.next()){
            QString ActionNum =
                    query.value(0).toString();
            if(ActionNum == "2"){//拍照直接上传,不进行工作模式切换
                operate_camera->StartCamera(CardID,TriggerTime);

                QString StrId = QUuid::createUuid().toString();
                QString DirName = StrId.mid(1,StrId.length() - 2);
                CommonSetting::CreateFolder("/opt",DirName);

                QString Base64 = QString("/opt/Base64_") + CardID + QString("_") + TriggerTime.split(" ").at(0) + "\\ " + TriggerTime.split(" ").at(1) + QString(".txt");
                system(tr("mv %1 /opt/%2").arg(Base64).arg(DirName).toAscii().data());
                system(tr("mv /opt/%1 /mnt").arg(DirName).toAscii().data());
            }else if((ActionNum == "0")){//什么事情都不做

            }else if((ActionNum == "1")){//切换工作模式为刷卡工作模式,并拍照、保存卡号和触发时间、启动清零定时器、自增刷卡次数
                //设置为刷卡工作模式
                link_operate->PowerLedTimer->stop();
                link_operate->PowerLedOn();//电源指示灯常亮
                WorkModeState = ReadCardID::SwipCardWorkMode;

                operate_camera->StartCamera(CardID,TriggerTime);//启动摄像头拍照
                CardIDList << CardID;//保存卡号和触发时间
                TriggerTimeList << TriggerTime;

                //启动清零定时器、自增刷卡次数
                TimeOutClearTimer->start();
                TotalSwipCardCount++;
            }
        }
    }else if(WorkModeState == ReadCardID::SwipCardWorkMode){
        operate_camera->StartCamera(CardID,TriggerTime);//启动摄像头拍照
        CardIDList << CardID;//保存卡号和触发时间
        TriggerTimeList << TriggerTime;
        TotalSwipCardCount++;
        if(TotalSwipCardCount > 10){
            qDebug() << "刷卡张数大于10张\n";
            slotClearAllRecord();
            return;
        }
        //比较当前功能卡是否和第一张功能卡是同一张
        if(CardIDList.at(0) == CardID){
            TimeOutClearTimer->stop();
            link_operate->PowerLedTimer->start();//进入待机工作模式
            WorkModeState = ReadCardID::StandbyWorkMode;
            TotalSwipCardCount = 0;
            //添加解析刷卡记录
            ParseAllSwipCardRecord();
        }
    }
}

void ReadCardID::ParseAllSwipCardRecord()
{
    qDebug() << "进入解析刷卡记录";
    qDebug() << "卡号过滤之前:" << CardIDList;
    //除了第一张卡和最后一张卡之外,如果中间的卡号存在重复,则只保留一个卡号
    for(int i = 1; i < CardIDList.count() - 1; i++){
        for(int j = i + 1; j < CardIDList.count() - 1; j++){
            if(CardIDList.at(i) == CardIDList.at(j)){
                if(TriggerTimeList.at(i) != TriggerTimeList.at(j)){//用来过滤用户刷卡过快时同一张多次刷卡只保存了一张照片的情况
                    system(tr("rm -rf \"/opt/Base64_%1_%2.txt\"").arg(CardIDList.at(j)).arg(TriggerTimeList.at(j)).toAscii().data());
                }
                CardIDList.removeAt(j);
                TriggerTimeList.removeAt(j);
                j--;
            }
        }
    }
    qDebug() << "卡号过滤之后:" << CardIDList;

    //获取卡类型
    int count = CardIDList.count();
    QStringList tempCardIDList = CardIDList;
    QStringList tempTriggerTimeList = TriggerTimeList;
    for(int i = 0; i < count; i++){
        bool flag = false;
        query.exec(tr("SELECT [功能号] from [卡号表] where [卡号] = \"%1\"").arg(tempCardIDList.at(i)));
        while(query.next()){
            flag = true;
            CardTypeList << query.value(0).toString();
        }
        if(!flag){
            system(tr("rm -rf /opt/Base64_%1*.txt").arg(tempCardIDList.at(i)).toAscii().data());
            qDebug() << "卡号没有注册:" << tempCardIDList.at(i);
            CardIDList.removeOne(tempCardIDList.at(i));
            tempTriggerTimeList.removeOne(tempTriggerTimeList.at(i));
        }
    }
    qDebug() << "删除无效卡之后:" << CardIDList;

    //判断卡号是否使能和过期
    for(int i = 0; i < CardIDList.count(); i++){
        query.exec(tr("SELECT [状态],[有效期限] FROM [卡号表] WHERE [卡号] = \"%1\"").arg(CardIDList.at(i)));
        while(query.next()){
            int CardIDStatus = query.value(0).toInt();
            QString CardIDVaildPeriod = query.value(1).toString();
            QSqlQuery sq;
            sq.exec(tr("SELECT julianday('now')- julianday('%1')").arg(CardIDVaildPeriod));
            while(sq.next()){
                if((sq.value(0).toInt() > 0) || (CardIDStatus != 1)){
                    qDebug() << "解析失败,卡号过期:" << CardIDList.at(i);
                    slotClearAllRecord();
                    return;
                }
            }
        }
    }

    //从数据库中读取此次操作需要满足的刷卡序列
    query.exec(tr("SELECT [刷卡序列] FROM [功能表] WHERE [功能号] = (SELECT [功能号] FROM [卡号表] WHERE [卡号] = \"%1\")").arg(CardIDList.at(0)));
    QStringList CardOrderList;
    while(query.next()){
        CardOrderList = query.value(0).toString().split(",");//需要满足的刷卡序列
    }
    quint8 CardOrderArray[CardOrderList.count()];//将刷卡序列保存到数组中
    for(int i = 0; i < CardOrderList.count(); i++){
        CardOrderArray[i] = CardOrderList.at(i).toUInt();
    }
    //解析用户刷卡序列是否满足要求
    for(int i = 0; i < CardTypeList.count() - 1; i++){
        for(int j = 0; j < CardOrderList.count(); j++){
            if(CardTypeList.at(i).toUInt() == CardOrderArray[j]){
                CardOrderArray[j] = 0;
                break;
            }
        }
    }
    quint8 TotalSum = 0;
    for(int i = 0; i < CardOrderList.count(); i++){
        TotalSum += CardOrderArray[i];
    }

    if(TotalSum > 0){
        qDebug() << "解析失败\n" << "TotalSum = " << TotalSum;
    }else if(TotalSum == 0){
        CommonLinkOperateCode();
    }
    //不管刷卡组合是否解析成功,都要将所有刷卡记录删除
    slotClearAllRecord();
}

void ReadCardID::CommonLinkOperateCode()
{
    qDebug() << "解析成功";

    //刷卡成功，蜂鸣器长叫5s,继电器上电
    link_operate->BuzzerOn();
    link_operate->BuzzerTimer->start();

    link_operate->RelayPowerOn();
    link_operate->RelayTimer->start();

    //移动/opt目录下的图片到/mnt目录下
    QString StrId = QUuid::createUuid().toString();
    QString DirName = StrId.mid(1,StrId.length() - 2);
    CommonSetting::CreateFolder("/opt",DirName);


    system(tr("mv /opt/*.txt /opt/%1").arg(DirName).toAscii().data());
    system(tr("mv /opt/%1 /mnt").arg(DirName).toAscii().data());
}

void ReadCardID::slotClearAllRecord()
{
    TimeOutClearTimer->stop();
    //切换工作模式为待机工作模式
    link_operate->PowerLedTimer->start();
    WorkModeState = ReadCardID::StandbyWorkMode;
    TotalSwipCardCount = 0;
    CardIDList.clear();
    CardTypeList.clear();
    TriggerTimeList.clear();

    //添加删除刷卡记录代码
    system("rm -rf /opt/*.txt");
}
