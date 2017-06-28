#include "readcardid.h"
#include "ui_readcardid.h"

ReadCardID::ReadCardID(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ReadCardID)
{
    ui->setupUi(this);

    InitForm();
    OpenDevice();

    WorkModeState = ReadCardID::StandbyWorkMode;
    TotalSwipCardCount = 0;
    index = 1;

    operate_camera = new OperateCamera(this);

    link_operate = new LinkOperate(this);

#ifdef USES_SHUAKAJI_WAIKE//如果用的是ATM刷卡机外壳
    //用来读取SmartUSB设备或者地址模块的报警信息
//    listen_serial = new ListenSerialThread();
    ListenSerialThread::newInstance()->init();

    isStop485Bus = 0;

    ReadStateTimer = new QTimer(this);
    ReadStateTimer->setInterval(1);
    connect(ReadStateTimer,SIGNAL(timeout()),this,SLOT(slotReadState()));
    ReadStateTimer->start();
#endif



    quint32 MaxTime = CommonSetting::ReadSettings("/bin/config.ini","time/MaxTime").toUInt() * 1000;
    TimeOutClearTimer = new QTimer(this);
    TimeOutClearTimer->setInterval(MaxTime);
    connect(TimeOutClearTimer,SIGNAL(timeout()),this,SLOT(slotClearAllRecord()));

    ReadCardIDTimer = new QTimer(this);
    ReadCardIDTimer->setInterval(300);
    connect(ReadCardIDTimer,SIGNAL(timeout()),this,SLOT(slotReadCardID()));
    ReadCardIDTimer->start();

    //表示启动完成
    link_operate->BuzzerOn();
    link_operate->BuzzerTimer->start();
}

ReadCardID::~ReadCardID()
{
    ::close(WiegFd);
    delete ui;
}

void ReadCardID::InitForm()
{
    this->setWindowFlags(Qt::FramelessWindowHint);
    this->setAutoFillBackground(true);
    QPalette palette;
    palette.setBrush(QPalette::Background,QBrush(QPixmap("/bin/background.jpg")));
    this->setPalette(palette);

    first = new PersionInfoControl(this);
    second = new PersionInfoControl(this);
    third = new PersionInfoControl(this);
    four = new PersionInfoControl(this);
    five = new PersionInfoControl(this);
    sex = new PersionInfoControl(this);

    ui->gridLayout->addWidget(first,0,0);
    ui->gridLayout->addWidget(second,0,1);
    ui->gridLayout->addWidget(third,0,2);
    ui->gridLayout->addWidget(four,1,0);
    ui->gridLayout->addWidget(five,1,1);
    ui->gridLayout->addWidget(sex,1,2);

    list.append(first);
    list.append(second);
    list.append(third);
    list.append(four);
    list.append(five);
    list.append(sex);
}

void ReadCardID::OpenDevice()
{
#ifdef USES_SHUAKAJI_WAIKE
    WiegFd = open("/dev/s5pv210_wieg0",O_RDWR | O_NONBLOCK);//打开外部韦根设备文件
    if(WiegFd < 0){
        qDebug() << "open /dev/s5pv210_wieg0 failed";
    } else {
        qDebug() << "open /dev/s5pv210_wieg0 succeed";
    }
#endif

#ifdef USES_JIAYOUZHAN_WAIKE
    WiegFd = open("/dev/s5pv210_wieg1",O_RDWR | O_NONBLOCK);//打开外部韦根设备文件
    if(WiegFd < 0){
        qDebug() << "open /dev/s5pv210_wieg1 failed";
    } else {
        qDebug() << "open /dev/s5pv210_wieg1 succeed";
    }
#endif
    /*
    //将当前进程PID设置为fd文件所对应驱动程序将要发送SIGIO,SIGUSR信号进程PID
    fcntl(WiegFd, F_SETOWN, getpid());

    //获取fd的打开方式
    int Oflags = fcntl(WiegFd, F_GETFL);

    //将fd的打开方式设置为FASYNC --- 即 支持异步通知
    //该行代码执行会触发驱动程序中 file_operations->fasync 函数
    //fasync函数调用fasync_helper初始化一个fasync_struct结构体
    //该结构体描述了将要发送信号的进程PID (fasync_struct->fa_file->f_owner->pid)
    fcntl(WiegFd, F_SETFL, Oflags | FASYNC);

    //捕获SIGIO信号（由驱动程序发送）
    signal(SIGIO, signal_handler);
    */
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
    if(WorkModeState == ReadCardID::StandbyWorkMode){//待机工作模式
        bool is_exist = false;

        //如果该卡在数据库中不存在,此次操作无影响；如果数据库中存在,就要判断该卡的动作号是什么
        query.exec(tr("SELECT [动作号] FROM [功能表] WHERE [功能号] = (SELECT [功能号] FROM [卡号表] WHERE [卡号] = \"%1\")").arg(CardID));
        while(query.next()){
            is_exist = true;

            QString ActionNum = query.value(0).toString();
            if(ActionNum == "2"){//拍照直接上传,不进行工作模式切换
                QImage image = operate_camera->StartCamera(CardID,TriggerTime);

                QString StrId = QUuid::createUuid().toString();
                QString DirName = StrId.mid(1,StrId.length() - 2);
                CommonSetting::CreateFolder("/opt",DirName);

                QString Base64 = QString("/opt/Base64_") + CardID + QString("_") + TriggerTime + QString(".txt");
                system(tr("mv %1 /opt/%2").arg(Base64).arg(DirName).toAscii().data());
                system(tr("mv /opt/%1 /sdcard/log").arg(DirName).toAscii().data());

#ifdef USES_SHUAKAJI_WAIKE//如果用的是ATM刷卡机外壳
            //    listen_serial->start();
            ListenSerialThread::newInstance()->start();//开启读取SmartUSB或者地址模块的报警信息
#endif
            }else if((ActionNum == "0")){//什么事情都不做
#ifdef USES_SHUAKAJI_WAIKE//如果用的是ATM刷卡机外壳
            //    listen_serial->start();
            ListenSerialThread::newInstance()->start();//开启读取SmartUSB或者地址模块的报警信息
#endif
            }else if((ActionNum == "1")){//切换工作模式为刷卡工作模式,并拍照、保存卡号和触发时间、启动清零定时器、自增刷卡次数,清除上次刷卡图片,显示该卡对应人员的姓名和卡号和卡类型和本地抓拍图片和证件图片
#ifdef USES_SHUAKAJI_WAIKE//如果用的是ATM刷卡机外壳
                //因为读取SmartUSB设备或者地址模块的报警信息会影响韦根26，所有这里停止读取两种设备的报警信息
//                listen_serial->stop();
                ListenSerialThread::newInstance()->stop();
#endif
                //设置为刷卡工作模式
                link_operate->PowerLedTimer->stop();
                link_operate->PowerLedOn();//电源指示灯常亮
                WorkModeState = ReadCardID::SwipCardWorkMode;

                QImage image = operate_camera->StartCamera(CardID,TriggerTime);//启动摄像头拍照
                CardIDList << CardID;//保存卡号
                TriggerTimeList << TriggerTime;//保存触发时间

                //启动清零定时器、自增刷卡次数
                TimeOutClearTimer->start();
                TotalSwipCardCount++;

                //清除上次刷卡图片
                for(int i = 0; i < list.size(); i++){
                    list.at(i)->ClearAll();
                }

                //显示该卡对应人员的姓名和卡号和卡类型和本地抓拍图片和证件图片
                QString PersionName,PersionTypeID,PersionPicLocalUrl,PersionTypeName;

                query.exec(tr("SELECT [姓名],[人员类型ID],[身份证图片本地路径] FROM [卡号信息表] WHERE [卡号] = \"%1\"").arg(CardID));
                while(query.next()){
                    PersionName = query.value(0).toString();
                    PersionTypeID = query.value(1).toString().split(",").at(0);
                    PersionPicLocalUrl = query.value(2).toString();
                }

                query.exec(tr("SELECT [人员类型名称] FROM [人员类型表] WHERE [人员类型ID] = \"%1\"").arg(PersionTypeID));
                while(query.next()){
                    PersionTypeName = query.value(0).toString();
                }

                first->SetText(PersionName + "\t" + CardID + "\t" + PersionTypeName);
                first->ShowLocalSnapPic(image);
                first->ShowCertificatePic(PersionPicLocalUrl);

                qDebug() << "PersionPicLocalUrl:" << PersionPicLocalUrl;
            }
        }

        if (!is_exist) {//卡号不在数据库里面，因为刷卡之后，停止读取报警信息，所以需要重新开启读取SmartUSB设备或者地址模块的报警信息
#ifdef USES_SHUAKAJI_WAIKE//如果用的是ATM刷卡机外壳
            //    listen_serial->start();
            ListenSerialThread::newInstance()->start();//开启读取SmartUSB或者地址模块的报警信息
#endif
        }
    }else if(WorkModeState == ReadCardID::SwipCardWorkMode){//刷卡工作模式
        bool isDuplicate = false;
        if(TotalSwipCardCount >= 2){//判断人员卡是否刷多次
            if(CardIDList.join(",").mid(9).contains(CardID)){
                isDuplicate = true;
            }
        }

        //如果刷卡多次,则只保留最早一次刷卡记录
        if(!isDuplicate){
            QImage image = operate_camera->StartCamera(CardID,TriggerTime);//启动摄像头拍照
            CardIDList << CardID;//保存卡号和触发时间
            TriggerTimeList << TriggerTime;

            //显示该卡对应人员的姓名和卡号和卡类型和本地抓拍图片和证件图片,并且功能卡只显示第一张
            if(!(CardIDList.at(0) == CardID)){
                QString PersionName,PersionTypeID,PersionPicLocalUrl,PersionTypeName;

                query.exec(tr("SELECT [姓名],[人员类型ID],[身份证图片本地路径] FROM [卡号信息表] WHERE [卡号] = \"%1\"").arg(CardID));
                while(query.next()){
                    PersionName = query.value(0).toString();
                    PersionTypeID = query.value(1).toString().split(",").at(0);
                    PersionPicLocalUrl = query.value(2).toString();
                }

                query.exec(tr("SELECT [人员类型名称] FROM [人员类型表] WHERE [人员类型ID] = \"%1\"").arg(PersionTypeID));
                while(query.next()){
                    PersionTypeName = query.value(0).toString();
                }

                list.at(index)->SetText(PersionName + "\t" + CardID + "\t" + PersionTypeName);
                list.at(index)->ShowLocalSnapPic(image);
                list.at(index)->ShowCertificatePic(PersionPicLocalUrl);

                qDebug() << "PersionPicLocalUrl:" << PersionPicLocalUrl;

                index++;
                if(index == 6){
                    index = 0;
                }
            }
        }

        //自增刷卡次数
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
#if 0 //只要刷卡序列中包含本次刷卡需要满足的刷卡序列即可
    quint8 CardOrderArray[CardOrderList.count()];//将需要满足的刷卡序列保存到数组中
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
#else //本次刷卡序列必须和本次刷卡需要满足的刷卡序列完全相同
    //判断本次刷卡序列的长度是否大于等于需要满足的刷卡序列长度
    if(CardTypeList.size() - 1 < CardOrderList.size()){
        qDebug() << "解析失败\n";
        slotClearAllRecord();
        return;
    }

    quint8 CardOrderArray[CardTypeList.count() - 1];//将本次刷卡刷卡序列保存到数组中
    for(int i = 0; i < CardTypeList.count() - 1; i++){
        CardOrderArray[i] = CardTypeList.at(i).toUInt();
    }

    //解析用户刷卡序列是否满足要求
    for(int i = 0; i < CardOrderList.count(); i++){
        for(int j = 0; j < CardTypeList.count() - 1; j++){
            if(CardOrderList[i].toUInt() == CardOrderArray[j]){
                CardOrderArray[j] = 0;
                break;
            }
        }
    }

    quint8 TotalSum = 0;
    for(int i = 0; i < CardTypeList.count() - 1; i++){
        TotalSum += CardOrderArray[i];
    }

    //刷卡组合必须完全正确才能开锁，如果刷卡组合除了包含本次操作需要满足的刷卡序列，还包含其他卡号，则不能开锁，但是还是要上传记录。
    if(TotalSum > 0){
        qDebug() << "解析失败\n" << "TotalSum = " << TotalSum;
        //移动/opt目录下的图片到/sdcard/log目录下
        QString StrId = QUuid::createUuid().toString();
        QString DirName = StrId.mid(1,StrId.length() - 2);
        CommonSetting::CreateFolder("/opt",DirName);

        system(tr("mv /opt/*.txt /opt/%1").arg(DirName).toAscii().data());
        system(tr("mv /opt/%1 /sdcard/log").arg(DirName).toAscii().data());
    }else if(TotalSum == 0){
        CommonLinkOperateCode();
    }

#endif

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

    //移动/opt目录下的图片到/sdcard/log目录下
    QString StrId = QUuid::createUuid().toString();
    QString DirName = StrId.mid(1,StrId.length() - 2);
    CommonSetting::CreateFolder("/opt",DirName);

    system(tr("mv /opt/*.txt /opt/%1").arg(DirName).toAscii().data());
    system(tr("mv /opt/%1 /sdcard/log").arg(DirName).toAscii().data());
}

#ifdef USES_SHUAKAJI_WAIKE//如果用的是ATM刷卡机外壳
void ReadCardID::slotReadState()
{
    ioctl(WiegFd,GET_STATE,&isStop485Bus);

    if (isStop485Bus == 1) {
        //因为读取SmartUSB设备或者地址模块的报警信息会影响韦根26，所有这里停止读取两种设备的报警信息
        //                listen_serial->stop();
        ListenSerialThread::newInstance()->stop();
    }
}
#endif

void ReadCardID::slotClearAllRecord()
{
    TimeOutClearTimer->stop();
    //切换工作模式为待机工作模式
    link_operate->PowerLedTimer->start();
    WorkModeState = ReadCardID::StandbyWorkMode;
    TotalSwipCardCount = 0;
    index = 1;
    CardIDList.clear();
    CardTypeList.clear();
    TriggerTimeList.clear();

    //添加删除刷卡记录代码
    system("rm -rf /opt/*.txt");
    system("rm -rf /opt/*.jpg");

#ifdef USES_SHUAKAJI_WAIKE//如果用的是ATM刷卡机外壳
//    listen_serial->start();
    ListenSerialThread::newInstance()->start();//退出刷卡工作模式以后需要恢复读取SmartUSB或者地址模块的报警信息
#endif
}

/*
//信号处理函数
void signal_handler(int signum)
{
    if(signum == SIGIO){//停止读取SmartUSB设备的状态或者地址模块的状态
        ListenSerialThread::newInstance()->stop();

        qDebug() << "stop read SmartUSB or AddressModule state info";
    }
}
*/
