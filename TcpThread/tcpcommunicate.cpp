#include "tcpcommunicate.h"
#define WITH_DEBUG1
#define WITH_WRITE_FILE1

TcpCommunicate::TcpCommunicate(QObject *parent) :
    QObject(parent)
{
    isGetCardIDList = false;
    isGetCardOrderList = false;
    isGetCardDetailInfo = false;

    tcpSocket = new QTcpSocket(this);
    connect(tcpSocket,SIGNAL(readyRead()),
            this,SLOT(slotParseServerMessage()));
    connect(tcpSocket,SIGNAL(disconnected()),
            this,SLOT(slotCloseConnection()));
    connect(tcpSocket,SIGNAL(connected()),
            this,SLOT(slotEstablishConnection()));
    connect(tcpSocket,SIGNAL(error(QAbstractSocket::SocketError)),this,SLOT(slotDisplayError(QAbstractSocket::SocketError)));

    ConnectStateFlag = TcpCommunicate::DisConnectedState;

    //改成使用tcp短连接
    ServerIpAddress = CommonSetting::ReadSettings("/bin/config.ini","ServerNetwork/IP");
    ServerListenPort = CommonSetting::ReadSettings("/bin/config.ini","ServerNetwork/PORT");

    HeartTimer = new QTimer(this);
    quint32 HeartIntervalTime = CommonSetting::ReadSettings("/bin/config.ini","time/HeartIntervalTime").toUInt() * 60 * 1000;
    HeartTimer->setInterval(HeartIntervalTime);
    connect(HeartTimer,SIGNAL(timeout()),this,SLOT(slotSendHeartData()));

    GetCardIDListTimer = new QTimer(this);
    GetCardIDListTimer->setInterval(6000);
    connect(GetCardIDListTimer,SIGNAL(timeout()),this,SLOT(slotGetIDList()));

    CheckNetWorkTimer = new QTimer(this);
    CheckNetWorkTimer->setInterval(1000);
    connect(CheckNetWorkTimer,SIGNAL(timeout()),this,SLOT(slotCheckNetWorkState()));

    slotSendHeartData();//开机发送心跳和slotGetCardTypeName

    HeartTimer->start();
    GetCardIDListTimer->start();
    //    CheckNetWorkTimer->start();

    //创建一个监视器，用于监视目录/sdcard/log的变化
    dirWatcher = new QFileSystemWatcher(this);
    dirWatcher->addPath("/sdcard/log");
    connect(dirWatcher,SIGNAL(directoryChanged(QString)), this,SLOT(slotSendLogInfo(QString)));
}

void TcpCommunicate::slotSendHeartData()
{
    SendMsgTypeFlag = TcpCommunicate::DeviceHeart;
    SendDataPackage("","","");

    CommonSetting::Sleep(1500);
    slotGetCardTypeName();

    CommonSetting::Sleep(1500);
    slotGetCardInfo();

    CommonSetting::Sleep(3000);
    tcpSocket->disconnectFromHost();
    tcpSocket->abort();
}

void TcpCommunicate::slotGetIDList()
{
    static char count = 0;

    tcpSocket->disconnectFromHost();
    tcpSocket->abort();

    if(!isGetCardIDList){
        SendMsgTypeFlag = TcpCommunicate::DeviceHeart;
        SendDataPackage("","","");
        qDebug() << "isGetCardIDList";
    }else if(!isGetCardOrderList){
        SendMsgTypeFlag = TcpCommunicate::GetCardTypeName;
        SendDataPackage("","","");
        qDebug() << "isGetCardOrderList";
    }else if(!isGetCardDetailInfo){
        SendMsgTypeFlag = TcpCommunicate::GetCardInfo;
        SendDataPackage("","","");
        qDebug() << "isGetCardDetailInfo";
    }else{
        GetCardIDListTimer->stop();
        CommonSetting::Sleep(1000);
        tcpSocket->disconnectFromHost();
        tcpSocket->abort();
        qDebug() << "stop GetCardIDListTimer";
    }

    count++;
    if(count == 10){
        count = 0;
        GetCardIDListTimer->stop();
        tcpSocket->disconnectFromHost();
        tcpSocket->abort();
        qDebug() << "stop GetCardIDListTimer";
    }
}

//这样命令仅仅只是为了方便
void TcpCommunicate::slotGetCardTypeName()
{
    SendMsgTypeFlag = TcpCommunicate::GetCardTypeName;
    SendDataPackage("","","");
}

//这样命令仅仅只是为了方便
void TcpCommunicate::slotGetCardInfo()
{
    SendMsgTypeFlag = TcpCommunicate::GetCardInfo;
    SendDataPackage("","","");
}

void TcpCommunicate::slotCheckNetWorkState()
{
    system("ifconfig eth0 > /bin/network_info.txt");
    QString network_info = CommonSetting::ReadFile("/bin/network_info.txt");
    if(!network_info.contains("RUNNING")){
        ConnectStateFlag = TcpCommunicate::DisConnectedState;
    }
}

void TcpCommunicate::slotParseServerMessage()
{
    CommonSetting::Sleep(600);
    QByteArray data = tcpSocket->readAll();
    QString XmlData = QString(data).mid(20);
#if defined(WITH_WRITE_FILE1)
//    CommonSetting::WriteXmlFile("/bin/TcpReceiveMessage.txt",XmlData);
#endif
    ParseServerMessage(XmlData);//解析服务返回信息
}

void TcpCommunicate::ParseServerMessage(QString XmlData)
{
    QString NowTime,CardListUpTime;
    QDomDocument dom;
    QString errorMsg;
    QStringList LatestCardIDList,LatestCardTypeList
            ,LatestCardStatusList,LatestCardValidTimeList;//下载最新的卡号信息
    QStringList CurrentCardIDList,CurrentCardTypeList
            ,CurrentCardStatusList,CurrentCardValidTimeList;//发送日志信息，服务器返回卡号的当前信息
    QStringList CardNumberList,PersionNameList,PersionPicUrlList,PersionTypeIDList;
    static uint index = 0;

    int errorLine,errorColumn;
    bool CaptionFlag = false,CardStateFlag = false,ICard = false;

    if(!dom.setContent(XmlData,&errorMsg,&errorLine,&errorColumn)){
#if  defined(WITH_DEBUG1)
        qDebug() << "Parse error at line " << errorLine <<","<< "column " << errorColumn << ","<< errorMsg;
#endif
        return;
    }

    QDomElement RootElement = dom.documentElement();//获取根元素
    if(RootElement.tagName() == "Server"){//根元素名称
        //判断根元素是否有这个属性
        if(RootElement.hasAttribute("NowTime")){
            //获得这个属性对应的值
            NowTime = CardListUpTime = RootElement.attributeNode("NowTime").value();
            CommonSetting::SettingSystemDateTime(NowTime);
        }

        QDomNode firstChildNode = RootElement.firstChild();//第一个子节点
        while(!firstChildNode.isNull()){
            if(firstChildNode.nodeName() == "Caption"){
                qDebug() << "Caption";
                CaptionFlag = true;
                QDomElement firstChildElement = firstChildNode.toElement();
                QString firstChildElementText = firstChildElement.text();

                if(firstChildElementText.split(",").size() == 4){
                    LatestCardIDList << firstChildElementText.split(",").at(0);
                    LatestCardTypeList << firstChildElementText.split(",").at(1);
                    LatestCardStatusList << firstChildElementText.split(",").at(2);
                    LatestCardValidTimeList << firstChildElementText.split(",").at(3);
                }
            }else if(firstChildNode.nodeName() == "CardState"){
                CardStateFlag = true;
                QDomElement firstChildElement = firstChildNode.toElement();
                QString firstChildElementText = firstChildElement.text();

                if(firstChildElementText.split(",").size() == 4){
                    CurrentCardIDList << firstChildElementText.split(",").at(0);
                    CurrentCardTypeList << firstChildElementText.split(",").at(1);
                    CurrentCardStatusList << firstChildElementText.split(",").at(2);
                    CurrentCardValidTimeList << firstChildElementText.split(",").at(3);
                }
                qDebug() << "Update CardState";
            }else if(firstChildNode.nodeName() == "FunctionTypeName"){
                qDebug() << "FunctionTypeName";
                isGetCardOrderList = true;
                QDomElement firstChildElement = firstChildNode.toElement();
                QString firstChildElementText = firstChildElement.text();
                QStringList FunctionList = firstChildElementText.split(";");
                // 开始启动事务
                QSqlDatabase::database().transaction();

                query.exec("delete from [功能表]");
                for(int i = 0; i < FunctionList.count();  i++){
                    QString FunctionNum = FunctionList.at(i).split(",").at(0);//功能号
                    QString ActionNum = FunctionList.at(i).split(",").at(2).split("#").at(0);//动作号
                    QString CardOrder = FunctionList.at(i).split("#").at(1);//刷卡序列
                    query.exec(tr("INSERT INTO [功能表] ([功能号], [动作号],[刷卡序列]) VALUES (\"%1\",\"%2\",\"%3\");").arg(FunctionNum).arg(ActionNum).arg(CardOrder));
                    //                    qDebug() << query.lastError().text();
                }
                // 提交事务，这个时候才是真正打开文件执行SQL语句的时候
                QSqlDatabase::database().commit();
            }else if(firstChildNode.nodeName() == "PersonTypeName"){
                qDebug() << "PersonTypeName";
                isGetCardOrderList = true;
                QDomElement firstChildElement = firstChildNode.toElement();
                QString firstChildElementText = firstChildElement.text();
                QStringList PersonTypeNameList = firstChildElementText.split(";");
                // 开始启动事务
                QSqlDatabase::database().transaction();

                query.exec("delete from [人员类型表]");
                for(int i = 0; i < PersonTypeNameList.count();  i++){
                    QString PersionTypeID = PersonTypeNameList.at(i).split(",").at(1);//人员类型ID
                    QString PersionTypeName = PersonTypeNameList.at(i).split(",").at(2);//人员类型名称
                    query.exec(tr("INSERT INTO [人员类型表] ([人员类型ID], [人员类型名称]) VALUES (\"%1\",\"%2\");").arg(PersionTypeID).arg(PersionTypeName));
                    //                    qDebug() << query.lastError().text();
                }
                // 提交事务，这个时候才是真正打开文件执行SQL语句的时候
                QSqlDatabase::database().commit();
            }else if(firstChildNode.nodeName() == "ICard"){
                qDebug() << "ICard";
                isGetCardDetailInfo = true;
                ICard = true;
                QDomElement firstChildElement = firstChildNode.toElement();

                CardNumberList << firstChildElement.attributeNode("Number").value();
                PersionNameList << QString("");
                PersionPicUrlList << QString("");
                PersionTypeIDList << QString("");//主要用来保证他们的长度是一样

                QDomNodeList SubChildNodeList = firstChildElement.childNodes(); //获得元素的所有子节点的列表
                for(int i = 0; i < SubChildNodeList.size(); i++){
                    QDomElement e = SubChildNodeList.at(i).toElement();
                    if(e.attributeNode("Value").value() == QString("姓名")){
                        PersionNameList.replace(index,e.text());
                    }

                    if(e.attributeNode("Value").value() == QString("_PicUrl")){
                        PersionPicUrlList.replace(index,e.text());
                    }

                    if(e.nodeName() == QString("Function")){
                        PersionTypeIDList.replace(index,e.text());
                    }
                }
                index++;
            }

            firstChildNode = firstChildNode.nextSibling();//下一个节点
        }
        if(CaptionFlag){
            isGetCardIDList = true;
            CommonSetting::WriteSettings("/bin/config.ini","time/CardListUpTime",CardListUpTime);
            // 开始启动事务
            QSqlDatabase::database().transaction();
            query.exec("delete from [卡号表]");//将原有卡号列表删除，重新下载最新卡号列表
            for(int i = 0; i < LatestCardIDList.count(); i++){
                query.exec(tr("INSERT INTO [卡号表] ([卡号], [功能号], [状态], [有效期限]) VALUES (\"%1\",\"%2\",\"%3\",\"%4\");")
                           .arg(LatestCardIDList.at(i)).arg(LatestCardTypeList.at(i)).arg(LatestCardStatusList.at(i)).arg(LatestCardValidTimeList.at(i)));
                //                qDebug() << query.lastError().text();
            }
            // 提交事务，这个时候才是真正打开文件执行SQL语句的时候
            QSqlDatabase::database().commit();
        }

        if(CardStateFlag){
            // 开始启动事务
            QSqlDatabase::database().transaction();
            for(int i = 0; i < CurrentCardIDList.count(); i++){
                query.exec(tr("UPDATE [卡号表] SET [功能号] = \"%1\", [状态] = \"%2\", [有效期限] = \"%3\" WHERE [卡号] = \"%4\"")
                           .arg(CurrentCardTypeList.at(i)).arg(CurrentCardStatusList.at(i)).arg(CurrentCardValidTimeList.at(i)).arg(CurrentCardIDList.at(i)));
                //                qDebug() << query.lastError().text();
            }
            // 提交事务，这个时候才是真正打开文件执行SQL语句的时候
            QSqlDatabase::database().commit();
        }


        if(ICard){
            qDebug() << "size = " << CardNumberList.size() << PersionNameList.size() << PersionTypeIDList.size() << PersionPicUrlList.size();
            index = 0;
            // 开始启动事务
            QSqlDatabase::database().transaction();
            query.exec("delete from [卡号信息表]");
            for(int i = 0; i < CardNumberList.count(); i++){
                query.exec(tr("INSERT INTO [卡号信息表] ([卡号], [姓名], [人员类型ID], [身份证图片网址]) VALUES (\"%1\",\"%2\",\"%3\",\"%4\");")
                           .arg(CardNumberList.at(i)).arg(PersionNameList.at(i)).arg(PersionTypeIDList.at(i)).arg(PersionPicUrlList.at(i)));
                //                qDebug() << query.lastError().text();
            }
            // 提交事务，这个时候才是真正打开文件执行SQL语句的时候
            QSqlDatabase::database().commit();

            CommonSetting::WriteCommonFileTruncate("/bin/GetCardDetialInfo.txt","OK");
            emit signalGetCardInfo();
        }
    }
}

void TcpCommunicate::slotCloseConnection()
{
    qDebug() << "close connection";
    ConnectStateFlag = TcpCommunicate::DisConnectedState;
    tcpSocket->abort();
}

void TcpCommunicate::slotEstablishConnection()
{
    qDebug() << "connect to server succeed";
    ConnectStateFlag = TcpCommunicate::ConnectedState;
}

void TcpCommunicate::slotDisplayError(QAbstractSocket::SocketError socketError)
{
    switch(socketError){
    case QAbstractSocket::ConnectionRefusedError:
        qDebug() << "QAbstractSocket::ConnectionRefusedError";
        break;
    case QAbstractSocket::RemoteHostClosedError:
        qDebug() << "QAbstractSocket::RemoteHostClosedError";
        break;
    case QAbstractSocket::HostNotFoundError:
        qDebug() << "QAbstractSocket::HostNotFoundError";
        break;
    default:
        qDebug() << "The following error occurred:"
                    + tcpSocket->errorString();
        break;
    }

    ConnectStateFlag = TcpCommunicate::DisConnectedState;
    tcpSocket->abort();
}

void TcpCommunicate::SendDataPackage(QString PathPrefix,QString CardID,QString TriggerTime)
{
    QString MessageMerge;
    QDomDocument dom;
    //xml声明
    QString XmlHeader("version=\"1.0\" encoding=\"UTF-8\"");
    dom.appendChild(dom.createProcessingInstruction("xml", XmlHeader));

    //创建根元素
    QDomElement RootElement = dom.createElement("Device");

    QString DeviceID = CommonSetting::ReadSettings("/bin/config.ini","DeviceID/ID");
    QString MacAddress = CommonSetting::ReadMacAddress();
    QString CardListUpTime = CommonSetting::ReadSettings("/bin/config.ini","time/CardListUpTime");

    RootElement.setAttribute("ID",DeviceID);//设置属性
    RootElement.setAttribute("Mac",MacAddress);
    RootElement.setAttribute("Type","ICCard");
    RootElement.setAttribute("Ver","1.2.0.66");

    dom.appendChild(RootElement);

    if(SendMsgTypeFlag == TcpCommunicate::DeviceHeart){
        //创建第一个子元素
        QDomElement firstChildElement = dom.createElement("DeviceHeart");
        firstChildElement.setAttribute("State","1");
        //        firstChildElement.setAttribute("CardListUpTime",CardListUpTime);
        firstChildElement.setAttribute("CardListUpTime","2012-01-01 00:00:00");

        //将元素添加到根元素后面
        RootElement.appendChild(firstChildElement);
    }else if(SendMsgTypeFlag == TcpCommunicate::GetCardTypeName){
        //创建第一个子元素
        QDomElement firstChildElement = dom.createElement("GetCardTypeName");
        //将元素添加到根元素后面
        RootElement.appendChild(firstChildElement);
    }else if(SendMsgTypeFlag == TcpCommunicate::GetCardInfo){
        //创建第一个子元素
        QDomElement firstChildElement = dom.createElement("GetCardInfo");

        QStringList id_list;
        query.exec(tr("SELECT [卡号] FROM [卡号表]"));
        while(query.next()){
            id_list << query.value(0).toString();
        }

        QDomText firstChildElementText = dom.createTextNode(id_list.join(","));
        firstChildElement.appendChild(firstChildElementText);

        //将元素添加到根元素后面
        RootElement.appendChild(firstChildElement);
    }else if(SendMsgTypeFlag == TcpCommunicate::OperationCmd){
        QStringList CardIDList = CardID.split(",");
        QStringList TriggerTimeList = TriggerTime.split(",");
        QString CardType;

        query.exec(tr("SELECT [功能号] from [卡号表] where [卡号] = \"%1\"").arg(CardIDList.at(0)));
        while(query.next()){
            CardType = query.value(0).toString();
        }

        for(int i = 0; i < CardIDList.count(); i++){
            QDomElement firstChildElement = dom.createElement("OperationCmd");
            firstChildElement.setAttribute("Type",CardType);
            firstChildElement.setAttribute("CardID",CardIDList.at(i));
            firstChildElement.setAttribute("TriggerTime",TriggerTimeList.at(i));
            QString fileName = PathPrefix + "Base64_" + CardIDList.at(i) + "_" + TriggerTimeList.at(i).split(" ").at(0) + "_" + TriggerTimeList.at(i).split(" ").at(1).split(":").join("-") + ".txt";
            qDebug() << fileName;
            QString imgBase64 = CommonSetting::ReadFile(fileName);
            if(!imgBase64.isEmpty()){
                QDomText firstChildElementText = dom.createTextNode(imgBase64);//base64图片数据
                firstChildElement.appendChild(firstChildElementText);
            }
            //将元素添加到根元素后面
            RootElement.appendChild(firstChildElement);
        }
    }

    QTextStream Out(&MessageMerge);
    dom.save(Out,4);

    MessageMerge = CommonSetting::AddHeaderByte(MessageMerge);
//    CommonSetting::WriteCommonFile("/bin/TcpSendMessage.txt",MessageMerge);
    SendCommonCode(MessageMerge);
}

//tcp短连接
void TcpCommunicate::SendCommonCode(QString MessageMerge)
{    
    if(ConnectStateFlag == TcpCommunicate::ConnectedState){
        tcpSocket->write(MessageMerge.toAscii());
        CommonSetting::Sleep(300);

        DataSendStateFlag = TcpCommunicate::SendSucceed;
        PareseSendMsgType();

        return;
    }else if(ConnectStateFlag == TcpCommunicate::DisConnectedState){
        tcpSocket->disconnectFromHost();
        tcpSocket->abort();
        for(int i = 0; i < 3; i++){
            tcpSocket->connectToHost(ServerIpAddress,
                                     ServerListenPort.toUInt());
            CommonSetting::Sleep(300);

            if(ConnectStateFlag == TcpCommunicate::ConnectedState){
                tcpSocket->write(MessageMerge.toAscii());
                CommonSetting::Sleep(300);

                DataSendStateFlag = TcpCommunicate::SendSucceed;
                PareseSendMsgType();
                return;
            }
            tcpSocket->disconnectFromHost();
            tcpSocket->abort();
        }
        DataSendStateFlag = TcpCommunicate::SendFailed;
        qDebug() << "上传失败:与服务器连接未成功。请检查网线是否插好,本地网络配置,服务器IP,服务器监听端口配置是否正确.\n";
    }
}

void TcpCommunicate::PareseSendMsgType()
{
    if(SendMsgTypeFlag == TcpCommunicate::DeviceHeart){
        qDebug() << "Tcp:Send DeviceHeart Succeed";
    }else if(SendMsgTypeFlag == TcpCommunicate::GetCardTypeName){
        qDebug() << "Tcp:Send GetCardTypeName Succeed";
    }else if(SendMsgTypeFlag == TcpCommunicate::GetCardInfo){
        qDebug() << "Tcp:Send GetCardInfo Succeed";
    }else if(SendMsgTypeFlag == TcpCommunicate::OperationCmd){
        qDebug() << "Tcp:Picture CardID Already Succeed Send";
    }
}

void TcpCommunicate::slotSendLogInfo(QString info)
{
    dirWatcher->removePath("/sdcard/log");
    CommonSetting::Sleep(1000);

    tcpSocket->connectToHost(ServerIpAddress,ServerListenPort.toUInt());
    CommonSetting::Sleep(300);

    QStringList tempCardIDList;
    QStringList tempTriggerTimeList;
    char txFailedCount = 0;//用来统计发送失败次数
    char txSucceedCount = 0;//用来统计发送成功的次数

    QStringList dirlist = CommonSetting::GetDirNames("/sdcard/log");
    if(!dirlist.isEmpty()){
        foreach(const QString &dirName,dirlist){
            QStringList filelist =
                    CommonSetting::GetFileNames("/sdcard/log/" + dirName,"*.txt");
            qDebug() << "filelist:" << filelist;
            if(!filelist.isEmpty()){
                foreach(const QString &fileName,filelist){
                    tempCardIDList << fileName.split("_").at(1);
                    tempTriggerTimeList << fileName.split("_").at(2) + " " + fileName.split("_").at(3).split(".").at(0).split("-").join(":");
                }
                qDebug() << "tempCardIDList:" << tempCardIDList.join(",");
                SendMsgTypeFlag = TcpCommunicate::OperationCmd;
                SendDataPackage(QString("/sdcard/log/") + dirName + QString("/"),tempCardIDList.join(","),tempTriggerTimeList.join(","));
                //日志信息发送成功:删除本地记录
                if(DataSendStateFlag == TcpCommunicate::SendSucceed){
                    system(tr("rm -rf /sdcard/log/%1").arg(dirName).toAscii().data());
                    txSucceedCount++;
                    if(txSucceedCount == 5){
                        break;
                    }
                }else{
                    txFailedCount++;
                    if(txFailedCount == 5){
                        qDebug() << "连续发送五次都没有成功，表示网络离线\n";
                        break;
                    }
                }
                tempCardIDList.clear();
                tempTriggerTimeList.clear();
                CommonSetting::Sleep(1000);
            }
        }
    }

    CommonSetting::Sleep(1000);
    tcpSocket->disconnectFromHost();
    tcpSocket->abort();

    dirWatcher->addPath("/sdcard/log");
}

