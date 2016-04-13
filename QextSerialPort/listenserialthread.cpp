#include "listenserialthread.h"

static char SmartUSBPreState[] = {0,0,0,0,0,0,0,0};//用来保存SmartUSB设备上一次的状态:0不报警,1报警
static qint8 PreSmartUSBNum = 0;//用来保存上一次SmartUSB设备的个数
static qint8 SmartUSBNum = CommonSetting::ReadSettings("/bin/config.ini","SmartUSB/Num").toInt();//SmartUSB设备的个数

//0xFA 0xFD {id} 0xFF 发送
//0x0D 0x0A {id} {USB State}{Alarm State} 0x0F 返回

ListenSerialThread::ListenSerialThread(QObject *parent) :
    QObject(parent)
{
    mySerial = new QextSerialPort("/dev/ttySAC1");
    if(mySerial->open(QIODevice::ReadWrite)){
        mySerial->setBaudRate(BAUD9600);
        mySerial->setDataBits(DATA_8);
        mySerial->setParity(PAR_NONE);
        mySerial->setStopBits(STOP_1);
        mySerial->setFlowControl(FLOW_OFF);
        mySerial->setTimeout(10);
        qDebug() << "/dev/ttySAC1 Open Succeed!";
    }

    ServerIpAddress = CommonSetting::ReadSettings("/bin/config.ini","ServerNetwork/IP");
    ServerListenPort = CommonSetting::ReadSettings("/bin/config.ini","ServerNetwork/PORT");
    ConnectStateFlag = ListenSerialThread::DisConnectedState;

    tcpSocket = new QTcpSocket(this);
    connect(tcpSocket,SIGNAL(disconnected()),
            this,SLOT(slotCloseConnection()));
    connect(tcpSocket,SIGNAL(connected()),
            this,SLOT(slotEstablishConnection()));
    connect(tcpSocket,SIGNAL(error(QAbstractSocket::SocketError)),this,SLOT(slotDisplayError(QAbstractSocket::SocketError)));

    if(SmartUSBNum > 0){
        ReadSmartUSBStateTimer = new QTimer(this);
        ReadSmartUSBStateTimer->setInterval(3000);
        connect(ReadSmartUSBStateTimer,SIGNAL(timeout()),this,SLOT(slotPollingReadSmartUSBState()));
        ReadSmartUSBStateTimer->start();

        SendAlarmMsgTimer = new QTimer(this);
        SendAlarmMsgTimer->setInterval(3000);
        connect(SendAlarmMsgTimer,SIGNAL(timeout()),this,SLOT(slotSendCommonCode()));
        SendAlarmMsgTimer->start();
    }
}

void ListenSerialThread::slotPollingReadSmartUSBState()
{
    char ControlCode[] = {0xFA,0xFD,0,0xFF};

    for(qint8 i = 0; i < SmartUSBNum; i++){
        ControlCode[2] = i;
        mySerial->write(ControlCode,4);
        QString SmartUSB = ReadSerial();
		
        if(!SmartUSB.isEmpty()){
			//0x0D 0x0A {id} {USB State}{Alarm State} 0x0F
            qint8 len = SmartUSB.split(" ").size();
			if (len < 6) {
				continue;
			}
			
			qint8 index = 0;
            while(index < len){
                if(SmartUSB.split(" ").at(index) == "0D"){
                    break;
                }
                index++;
            }

            if(index == len){
                continue;
            }

            if((SmartUSB.split(" ").at(index) == "0D") && (SmartUSB.split(" ").at(index + 1) == "0A") && (SmartUSB.split(" ").at(index + 5) == "0F")){
                qint8 SmartUSBId = SmartUSB.split(" ").at(index + 2).toInt();
                qint8 SmartUSBState = SmartUSB.split(" ").at(index + 3).toInt();
                qint8 SmartUSBAlarmState = SmartUSB.split(" ").at(index + 4).toInt();
                if(PreSmartUSBNum < SmartUSBState){
                    //只要有设备插入,就上传记录,不管是否报警和报警解除
                    QString CardID = QString::number(SmartUSBId) + QString(",") + QString::number(SmartUSBState) + QString(",") + QString::number(SmartUSBAlarmState);
                    QString TriggerTime = TIMES;
                    SendMsgTypeFlag =
                            ListenSerialThread::SmartUSBNewInsert;
                    SendDataPackage(CardID,TriggerTime);
                    qDebug() << "设备ID:" << SmartUSBId <<"\n";
                }

                if((SmartUSBPreState[SmartUSBId] == 0) && (SmartUSBAlarmState == 1)){//报警
                    SmartUSBPreState[SmartUSBId] = 1;
                    QString CardID = QString::number(SmartUSBId) + QString(",") + QString::number(SmartUSBState) + QString(",") + QString::number(SmartUSBAlarmState);
                    QString TriggerTime = TIMES;
                    SendMsgTypeFlag =
                            ListenSerialThread::SmartUSBAlarmState;
                    SendDataPackage(CardID,TriggerTime);
                    qDebug() << "报警设备ID:" << SmartUSBId << "\n";
                }else if((SmartUSBPreState[SmartUSBId] == 1) && (SmartUSBAlarmState == 0)){//报警解除
                    SmartUSBPreState[SmartUSBId] = 0;
                    QString CardID = QString::number(SmartUSBId) + QString(",") + QString::number(SmartUSBState) + QString(",") + QString::number(SmartUSBAlarmState);
                    QString TriggerTime = TIMES;
                    SendMsgTypeFlag = ListenSerialThread::SmartUSBDeassertState;
                    SendDataPackage(CardID,TriggerTime);
                    qDebug() << "报警解除设备ID:" << SmartUSBId << "\n";
                }

                PreSmartUSBNum = SmartUSBState;
            }
        }
    }
}

QString ListenSerialThread::ReadSerial()
{
    CommonSetting::Sleep(100);

    QString strHex,RetValue;
    QByteArray Buffer = mySerial->readAll();
    QDataStream in(&Buffer,QIODevice::ReadWrite);
    if(!Buffer.isEmpty()){
        while(!in.atEnd()){
            quint8 outChar;
            in >> outChar;
            strHex = QString::number(outChar,16);
            if(strHex.length() == 1){
                strHex = "0" + strHex;
            }
            RetValue += strHex + " ";
        }
        RetValue = RetValue.simplified().trimmed().toUpper();
    }
//    qDebug() << "Recv:" << QString(RetValue);

    return QString(RetValue);
}

void ListenSerialThread::slotCloseConnection()
{
    qDebug() << "close connection";
    ConnectStateFlag = ListenSerialThread::DisConnectedState;
    tcpSocket->abort();
}

void ListenSerialThread::slotEstablishConnection()
{
    qDebug() << "connect to server succeed";
    ConnectStateFlag = ListenSerialThread::ConnectedState;
}

void ListenSerialThread::slotDisplayError(QAbstractSocket::SocketError socketError)
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

    ConnectStateFlag = ListenSerialThread::DisConnectedState;
    tcpSocket->abort();
}

void ListenSerialThread::SendDataPackage(QString CardID,QString TriggerTime)
{
    QString MessageMerge;
    QDomDocument dom;
    //xml声明
    QString XmlHeader("version=\"1.0\" encoding=\"UTF-8\"");
    dom.appendChild(dom.createProcessingInstruction("xml", XmlHeader));

    //创建根元素
    QDomElement RootElement =
            dom.createElement("Device");
    QString DeviceID = CommonSetting::ReadSettings("/bin/config.ini","DeviceID/ID");
    QString MacAddress = CommonSetting::ReadMacAddress();

    RootElement.setAttribute("ID",DeviceID);//设置属性
    RootElement.setAttribute("Mac",MacAddress);
    RootElement.setAttribute("Type","ICCard");
    RootElement.setAttribute("Ver","1.2.0.66");

    dom.appendChild(RootElement);

    if(SendMsgTypeFlag == ListenSerialThread::SmartUSBAlarmState){
        //创建第一个子元素
        QDomElement firstChildElement =
                dom.createElement("OperationCmd");
        firstChildElement.setAttribute("Type","80001");
        firstChildElement.setAttribute("CardID",CardID);
        firstChildElement.setAttribute("TriggerTime",TriggerTime);

        //将元素添加到根元素后面
        RootElement.appendChild(firstChildElement);
    }else if(SendMsgTypeFlag ==
             ListenSerialThread::SmartUSBDeassertState){
        //创建第一个子元素
        QDomElement firstChildElement =
                dom.createElement("OperationCmd");
        firstChildElement.setAttribute("Type","80002");
        firstChildElement.setAttribute("CardID",CardID);
        firstChildElement.setAttribute("TriggerTime",TriggerTime);

        //将元素添加到根元素后面
        RootElement.appendChild(firstChildElement);
    }else if(SendMsgTypeFlag ==
             ListenSerialThread::SmartUSBNewInsert){
        //创建第一个子元素
        QDomElement firstChildElement =
                dom.createElement("OperationCmd");
        firstChildElement.setAttribute("Type","80003");
        firstChildElement.setAttribute("CardID",CardID);
        firstChildElement.setAttribute("TriggerTime",TriggerTime);

        //将元素添加到根元素后面
        RootElement.appendChild(firstChildElement);
    }

    QTextStream Out(&MessageMerge);
    dom.save(Out,4);

    MessageMerge = CommonSetting::AddHeaderByte(MessageMerge);
    //    CommonSetting::WriteCommonFile("/bin/TcpSendMessage.txt",MessageMerge);
    AlarmMsgBuffer.append(MessageMerge);
}

//tcp短连接
void ListenSerialThread::slotSendCommonCode()
{
    if(AlarmMsgBuffer.size() > 0){
        tcpSocket->connectToHost(ServerIpAddress,ServerListenPort.toUInt());
        CommonSetting::Sleep(1000);

        if(ConnectStateFlag == ListenSerialThread::ConnectedState){
            tcpSocket->write(AlarmMsgBuffer.takeFirst().toAscii());
            if(tcpSocket->waitForBytesWritten(1000)){
                if(SendMsgTypeFlag ==
                        ListenSerialThread::SmartUSBAlarmState){
                    qDebug() << "SmartUSB设备报警记录上传成功";
                }else if(SendMsgTypeFlag ==
                         ListenSerialThread::SmartUSBDeassertState){
                    qDebug() << "SmartUSB设备报警解除记录上传成功";
                }else if(SendMsgTypeFlag == ListenSerialThread::SmartUSBNewInsert){
                    qDebug() << "SmartUSB有新设备插入记录上传成功";
                }
            }
            tcpSocket->disconnectFromHost();
            tcpSocket->abort();
            return;
        }else if(ConnectStateFlag == ListenSerialThread::DisConnectedState){
            tcpSocket->disconnectFromHost();
            tcpSocket->abort();
            for(qint8 i = 0; i < 3; i++){
                tcpSocket->connectToHost(ServerIpAddress,
                                         ServerListenPort.toUInt());
                CommonSetting::Sleep(1000);
                if(ConnectStateFlag == ListenSerialThread::ConnectedState){
                    tcpSocket->write(AlarmMsgBuffer.takeFirst().toAscii());
                    if(tcpSocket->waitForBytesWritten(1000)){
                        if(SendMsgTypeFlag ==ListenSerialThread::SmartUSBAlarmState){
                            qDebug() << "SmartUSB设备报警记录上传成功";
                        }else if(SendMsgTypeFlag == ListenSerialThread::SmartUSBDeassertState){
                            qDebug() << "SmartUSB设备报警解除记录上传成功";
                        }else if(SendMsgTypeFlag == ListenSerialThread::SmartUSBNewInsert){
                            qDebug() << "SmartUSB有新设备插入记录上传成功";
                        }
                    }

                    tcpSocket->disconnectFromHost();
                    tcpSocket->abort();
                    return;
                }
                tcpSocket->disconnectFromHost();
                tcpSocket->abort();
            }
            qDebug() << "上传失败:与服务器连接未成功。请检查网线是否插好,本地网络配置,服务器IP,服务器监听端口配置是否正确.\n";
            tcpSocket->disconnectFromHost();
            tcpSocket->abort();
        }
    }
}
