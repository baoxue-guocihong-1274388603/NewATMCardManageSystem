#include "listenserialthread.h"

ListenSerialThread *ListenSerialThread::instance = NULL;

static quint8 SmartUSBPreState[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};//用来保存SmartUSB设备上一次的状态:0不报警,1报警
static quint8 SmartUSBPreNum[16]= {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};//用来保存上一次SmartUSB设备插入USB的个数
static quint8 SmartUSBOnlineState[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};//用来保存是否有这个SmartUSB设备:0-设备不存在，1-设备存在
static quint8 SmartUSBLossPacketNum[16] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};//连续检测3次，保存SmartUSB丢包次数：0-代表设备一直没有返回，大于0：代表有返回过数据包


ListenSerialThread::ListenSerialThread(QObject *parent) :
    QObject(parent)
{

}

void ListenSerialThread::init()
{
    mySerial = new QextSerialPort("/dev/ttySAC1");
    if(mySerial->open(QIODevice::ReadWrite)){
        mySerial->setBaudRate(BAUD9600);
        mySerial->setDataBits(DATA_8);
        mySerial->setParity(PAR_NONE);
        mySerial->setStopBits(STOP_1);
        mySerial->setFlowControl(FLOW_OFF);
        mySerial->setTimeout(10);
        qDebug() << "open /dev/ttySAC1  succeed";
    } else {
        qDebug() << "open /dev/ttySAC1  failed";
    }

    SmartUSBNumber = CommonSetting::ReadSettings("/bin/config.ini","SmartUSB/Num").toUInt();
    RS485DeviceNumber = CommonSetting::ReadSettings("/bin/config.ini","RS485Device/Num").toUInt();
    ServerIpAddress = CommonSetting::ReadSettings("/bin/config.ini","ServerNetwork/IP");
    ServerListenPort = CommonSetting::ReadSettings("/bin/config.ini","ServerNetwork/PORT");
    ConnectStateFlag = ListenSerialThread::DisConnectedState;

    tcpSocket = new QTcpSocket(this);
    connect(tcpSocket,SIGNAL(disconnected()),this,SLOT(slotCloseConnection()));
    connect(tcpSocket,SIGNAL(connected()),this,SLOT(slotEstablishConnection()));
    connect(tcpSocket,SIGNAL(error(QAbstractSocket::SocketError)),this,SLOT(slotDisplayError(QAbstractSocket::SocketError)));

    if(SmartUSBNumber > 0){
        SmartUSBMsgBuffer.clear();

        SendSmartUSBCmdTimer = new QTimer(this);
        SendSmartUSBCmdTimer->setInterval(200);
        connect(SendSmartUSBCmdTimer,SIGNAL(timeout()),this,SLOT(slotSendSmartUSBCmd()));
        SendSmartUSBCmdTimer->start();

        ReadSmartUSBMsgTimer = new QTimer(this);
        ReadSmartUSBMsgTimer->setInterval(200);
        connect(ReadSmartUSBMsgTimer,SIGNAL(timeout()),this,SLOT(slotReadSmartUSBMsg()));
        ReadSmartUSBMsgTimer->start();

        ParseSmartUSBStateTimer = new QTimer(this);
        ParseSmartUSBStateTimer->setInterval(1000);
        connect(ParseSmartUSBStateTimer,SIGNAL(timeout()),this,SLOT(slotParseSmartUSBState()));
        ParseSmartUSBStateTimer->start();

        SendAlarmMsgTimer = new QTimer(this);
        SendAlarmMsgTimer->setInterval(3000);
        connect(SendAlarmMsgTimer,SIGNAL(timeout()),this,SLOT(slotSendCommonCode()));
        SendAlarmMsgTimer->start();
    }else if(RS485DeviceNumber > 0){
        RS485RecvMsgBuffer.clear();
        RS485RecvBuffer.clear();
        RS485ValidPacketBuffer.clear();

        RecvState = FSA_INIT;
        RecvChksum = 0;
        RecvCtr = 0;

        RS485DevicePreState.clear();
        for(quint8 i = 16; i < 128; i++){
            RS485DevicePreState[i] = 0;
        }

        SendRS485CmdTimer = new QTimer(this);
        SendRS485CmdTimer->setInterval(1000);
        connect(SendRS485CmdTimer,SIGNAL(timeout()),this,SLOT(slotSendRS485Cmd()));
        SendRS485CmdTimer->start();

        ReadRS485DeviceStateTimer = new QTimer(this);
        ReadRS485DeviceStateTimer->setInterval(100);
        connect(ReadRS485DeviceStateTimer,SIGNAL(timeout()),this,SLOT(slotPollingReadRS485DeviceState()));
        ReadRS485DeviceStateTimer->start();

        ProcessRS485MsgTimer = new QTimer(this);
        ProcessRS485MsgTimer->setInterval(5);
        connect(ProcessRS485MsgTimer,SIGNAL(timeout()),this,SLOT(slotProcessRS485Msg()));
        ProcessRS485MsgTimer->start();

        ProcessRS485ValidPacketTimer = new QTimer(this);
        ProcessRS485ValidPacketTimer->setInterval(200);
        connect(ProcessRS485ValidPacketTimer,SIGNAL(timeout()),this,SLOT(slotProcessRS485ValidPacket()));
        ProcessRS485ValidPacketTimer->start();

        SendAlarmMsgTimer = new QTimer(this);
        SendAlarmMsgTimer->setInterval(3000);
        connect(SendAlarmMsgTimer,SIGNAL(timeout()),this,SLOT(slotSendCommonCode()));
        SendAlarmMsgTimer->start();
    }
}

void ListenSerialThread::SendDataPackage(QString CardID,QString TriggerTime)
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

    RootElement.setAttribute("ID",DeviceID);//设置属性
    RootElement.setAttribute("Mac",MacAddress);
    RootElement.setAttribute("Type","ICCard");
    RootElement.setAttribute("Ver","1.2.0.66");

    dom.appendChild(RootElement);

    if(SendMsgTypeFlag == ListenSerialThread::SmartUSBAlarmState){
        //创建第一个子元素
        QDomElement firstChildElement = dom.createElement("OperationCmd");
        firstChildElement.setAttribute("Type","80001");
        firstChildElement.setAttribute("CardID",CardID);
        firstChildElement.setAttribute("TriggerTime",TriggerTime);

        //将元素添加到根元素后面
        RootElement.appendChild(firstChildElement);
    }else if(SendMsgTypeFlag == ListenSerialThread::SmartUSBDeassertState){
        //创建第一个子元素
        QDomElement firstChildElement = dom.createElement("OperationCmd");
        firstChildElement.setAttribute("Type","80002");
        firstChildElement.setAttribute("CardID",CardID);
        firstChildElement.setAttribute("TriggerTime",TriggerTime);

        //将元素添加到根元素后面
        RootElement.appendChild(firstChildElement);
    }else if(SendMsgTypeFlag == ListenSerialThread::SmartUSBNewInsert){
        //创建第一个子元素
        QDomElement firstChildElement = dom.createElement("OperationCmd");
        firstChildElement.setAttribute("Type","80003");
        firstChildElement.setAttribute("CardID",CardID);
        firstChildElement.setAttribute("TriggerTime",TriggerTime);

        //将元素添加到根元素后面
        RootElement.appendChild(firstChildElement);
    }else if(SendMsgTypeFlag == ListenSerialThread::SmartUSBOffline){
        //创建第一个子元素
        QDomElement firstChildElement = dom.createElement("OperationCmd");
        firstChildElement.setAttribute("Type","80004");
        firstChildElement.setAttribute("CardID",CardID);
        firstChildElement.setAttribute("TriggerTime",TriggerTime);

        //将元素添加到根元素后面
        RootElement.appendChild(firstChildElement);
    }else if(SendMsgTypeFlag == ListenSerialThread::RS485DeviceAlarmState){
        //创建第一个子元素
        QDomElement firstChildElement = dom.createElement("OperationCmd");
        firstChildElement.setAttribute("Type","80001");
        firstChildElement.setAttribute("CardID",CardID);
        firstChildElement.setAttribute("TriggerTime",TriggerTime);

        //将元素添加到根元素后面
        RootElement.appendChild(firstChildElement);
    }else if(SendMsgTypeFlag == ListenSerialThread::RS485DeviceDeassertState){
        //创建第一个子元素
        QDomElement firstChildElement = dom.createElement("OperationCmd");
        firstChildElement.setAttribute("Type","80002");
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

void ListenSerialThread::start()
{
    if (SmartUSBNumber > 0) {
        SendSmartUSBCmdTimer->start();
        ReadSmartUSBMsgTimer->start();
        ParseSmartUSBStateTimer->start();
        SendAlarmMsgTimer->start();
    }

    if (RS485DeviceNumber > 0) {
        SendRS485CmdTimer->start();
        ReadRS485DeviceStateTimer->start();
        ProcessRS485MsgTimer->start();
        ProcessRS485ValidPacketTimer->start();
        SendAlarmMsgTimer->start();
    }
}

void ListenSerialThread::stop()
{
    if (SmartUSBNumber > 0) {
        SendSmartUSBCmdTimer->stop();
        ReadSmartUSBMsgTimer->stop();
        ParseSmartUSBStateTimer->stop();
        SendAlarmMsgTimer->stop();
    }

    if (RS485DeviceNumber > 0) {
        SendRS485CmdTimer->stop();
        ReadRS485DeviceStateTimer->stop();
        ProcessRS485MsgTimer->stop();
        ProcessRS485ValidPacketTimer->stop();
        SendAlarmMsgTimer->stop();
    }
}

void ListenSerialThread::slotSendSmartUSBCmd()
{
    //发送:0xFA 0xFD {id} 0xFF
    //返回：0x0D 0x0A {id} {USB State}{Alarm State} 0x0F

    static quint8 index = 0;//设备索引
    static quint8 loop = 0;//轮循次数

    QByteArray ControlCode;
    ControlCode.append(0xFA);
    ControlCode.append(0xFD);
    ControlCode.append(index);
    ControlCode.append(0xFF);

    mySerial->write(ControlCode);

    index++;

    if (index == SmartUSBNumber) {
        index = 0;
        loop++;

        if (loop == 5) {//轮循5次，就需要检测是否有设备离线
            loop = 0;

            for (quint8 i = 0; i < SmartUSBNumber; i++) {
                if ((SmartUSBOnlineState[i] == 1) && (SmartUSBLossPacketNum[i] == 0)) {//设备离线
                    SmartUSBOnlineState[i] = 0;//设备离线只需要上报一次，不要一直上报

                    QString CardID = QString::number(i) + QString(",") + QString::number(0) + QString(",") + QString::number(0);
                    QString TriggerTime = TIMES;
                    SendMsgTypeFlag = ListenSerialThread::SmartUSBOffline;
                    SendDataPackage(CardID,TriggerTime);
                    qDebug() << "Offline Device ID:" << i << "\n";
                }

                SmartUSBLossPacketNum[i] = 0;//清零，重新计数
            }
        }
    }
}

void ListenSerialThread::slotReadSmartUSBMsg()
{
    if (mySerial->bytesAvailable() <= 0) {
        return;
    }

    QByteArray msg = mySerial->readAll();
    SmartUSBMsgBuffer.append(msg);
//    qDebug() << "Recv:" << msg.toHex();
}

void ListenSerialThread::slotParseSmartUSBState()
{
    if (SmartUSBMsgBuffer.size() == 0) {
        return;
    }

    /*
    通信协议:
    上位机发送4字节：
    0xFA 0xFD {id} 0xFF
    SmartUSB返回6字节：
            0x0D 0x0A {id} {USB State} {Alarm State} 0x0F

    说明：
    上位机发送指令后，SmartUSB最大100ms内返回状态数据。
    0x0D 0x0A为指令起始位，占用2Byte
    ｛USB State｝为每个USB占用1bit，0为未插入USB，1为插入USB的端口共8bit，低6bit有效。
    ｛Alarm State｝为0正常，1 报警，当有USB口被拔出后，启动报警。
    0x0F为指令结尾，占用1Byte
    */

//    qDebug() << SmartUSBMsgBuffer.toHex();
    while(SmartUSBMsgBuffer.size() > 0) {
        //寻找帧头0x0D的索引
        int index = SmartUSBMsgBuffer.indexOf(0x0D);
        if (index < 0) {//没有找到帧头
            return;
        }

        //判断是否收到一个完整的数据包
        if (SmartUSBMsgBuffer.size() < (index + 5)) {//没有收到一个完整的数据包
            return;
        }

        //判断接收的一帧数据是否是正确的
        if ((SmartUSBMsgBuffer.at(index + 1) == 0x0A) &&
                (SmartUSBMsgBuffer.at(index + 5) == 0xF)) {//数据校验正确，收到一个正确的完整的数据包
            qint8 SmartUSBId = SmartUSBMsgBuffer.at(index + 2);
            qint8 SmartUSBState = SmartUSBMsgBuffer.at(index + 3);
            qint8 SmartUSBAlarmState = SmartUSBMsgBuffer.at(index + 4);

            SmartUSBOnlineState[SmartUSBId] = 1;//设备存在
            SmartUSBLossPacketNum[SmartUSBId]++;//设备返回次数累加

            if(SmartUSBPreNum[SmartUSBId] < SmartUSBState){//只要有设备插入,就上传记录,不管是否报警和报警解除
                QString CardID = QString::number(SmartUSBId) + QString(",") + QString::number(SmartUSBState) + QString(",") + QString::number(SmartUSBAlarmState);
                QString TriggerTime = TIMES;
                SendMsgTypeFlag = ListenSerialThread::SmartUSBNewInsert;
                SendDataPackage(CardID,TriggerTime);
                qDebug() << "Insert Device ID:" << SmartUSBId <<"\n";
            }

            if((SmartUSBPreState[SmartUSBId] == 0) && (SmartUSBAlarmState == 1)){//报警
                SmartUSBPreState[SmartUSBId] = 1;
                QString CardID = QString::number(SmartUSBId) + QString(",") + QString::number(SmartUSBState) + QString(",") + QString::number(SmartUSBAlarmState);
                QString TriggerTime = TIMES;
                SendMsgTypeFlag = ListenSerialThread::SmartUSBAlarmState;
                SendDataPackage(CardID,TriggerTime);
                qDebug() << "Alarm Device ID:" << SmartUSBId << "\n";
            }else if((SmartUSBPreState[SmartUSBId] == 1) && (SmartUSBAlarmState == 0)){//报警解除
                SmartUSBPreState[SmartUSBId] = 0;
                QString CardID = QString::number(SmartUSBId) + QString(",") + QString::number(SmartUSBState) + QString(",") + QString::number(SmartUSBAlarmState);
                QString TriggerTime = TIMES;
                SendMsgTypeFlag = ListenSerialThread::SmartUSBDeassertState;
                SendDataPackage(CardID,TriggerTime);
                qDebug() << "Alarm Reset Device ID:" << SmartUSBId << "\n";
            }

            SmartUSBPreNum[SmartUSBId] = SmartUSBState;
        }

        SmartUSBMsgBuffer = SmartUSBMsgBuffer.mid(index + 6);
    }
}

void ListenSerialThread::slotSendRS485Cmd()
{
    char ControlCode[] = {0x16,0xFF,0x01,0x01,0xE0,0xE1};
    mySerial->write(ControlCode,6);
}

void ListenSerialThread::slotPollingReadRS485DeviceState()
{
    if(mySerial->bytesAvailable() <= 0){
        return;
    }

    QByteArray Buffer = mySerial->readAll().toHex();
    RS485RecvMsgBuffer.append(Buffer);
}

void ListenSerialThread::slotProcessRS485Msg()
{
    if(RS485RecvMsgBuffer.size() == 0) {
        return;
    }

    bool ok = true;
    quint8 c = RS485RecvMsgBuffer.mid(0,2).toUInt(&ok,16);
    RS485RecvMsgBuffer.remove(0,2);

    switch (RecvState)
    {
    case FSA_INIT://是否为帧头
        if (c == FRAME_STX) { //为帧头, 开始新的一帧
            RecvCtr = 0;
            RecvChksum = 0;
            RecvState = FSA_ADDR_D;
        }
        break;

    case FSA_ADDR_D://为目的地址, 开始保存并计算效验和
        RS485RecvBuffer[RecvCtr++] = c;
        RecvChksum += c;
        RecvState = FSA_ADDR_S;
        break;

    case FSA_ADDR_S://为源地址
        RS485RecvBuffer[RecvCtr++] = c;
        RecvChksum += c;
        RecvState = FSA_LENGTH;
        break;

    case FSA_LENGTH://为长度字节
        if ((c > 0) && (c < (MAX_RecvFrame - 3))) { //有效串
            RS485RecvBuffer[RecvCtr++] = c;    //第三个字节保存长度
            RecvChksum += c;
            RecvState = FSA_DATA;
        } else {	//非有效串
            RecvState = FSA_INIT;
        }
        break;

    case FSA_DATA://读取命令串
        RS485RecvBuffer[RecvCtr] = c;
        RecvChksum += c;   //更新校验和
        if (RecvCtr == (RS485RecvBuffer[2] + 2)){ //已经收到指定长度的命令数据
            RecvState = FSA_CHKSUM;
        }else{//还未结束
            RecvCtr ++;
        }
        break;

    case FSA_CHKSUM://检查校验字节
        if (RecvChksum == c){
            //已经收到完整一帧
            RS485ValidPacketBuffer.append(RS485RecvBuffer);
            RS485RecvBuffer.clear();
        }
    default:
        //复位
        RecvState = FSA_INIT;
        break;
    }
}

void ListenSerialThread::slotProcessRS485ValidPacket()
{
    if(RS485ValidPacketBuffer.size() == 0) {
        return;
    }

    bool ok = true;
    QByteArray packet = RS485ValidPacketBuffer.takeFirst().toHex();

    //    quint8 HostRS485Address = packet.mid(0,2).toUInt(&ok,16);
    quint8 SlaveRS485Address = packet.mid(2,2).toUInt(&ok,16);
    //    quint8 Lenght = packet.mid(4,2).toUInt(&ok,16);
    quint8 Cmd = packet.mid(6,2).toUInt(&ok,16);

    QString CardID;
    QString TriggerTime;

    switch(Cmd){
    case 0x00:
        //设备正常
        if(RS485DevicePreState[SlaveRS485Address].operator ==(1)){
            qDebug() << "DeviceAddr = " << SlaveRS485Address << ",DeviceState = Alarm Reset";

            CardID = QString::number(SlaveRS485Address) + QString(",") + QString::number(0) + QString(",") + QString::number(0);
            TriggerTime = TIMES;
            SendMsgTypeFlag = ListenSerialThread::RS485DeviceDeassertState;
            SendDataPackage(CardID,TriggerTime);
            RS485DevicePreState[SlaveRS485Address] = 0;
        }
        break;

    case 0xF0:
        //设备报警
        if(RS485DevicePreState[SlaveRS485Address].operator ==(0)){
            qDebug() << "DeviceAddr = " << SlaveRS485Address << ",DeviceState = Alarm";

            CardID = QString::number(SlaveRS485Address) + QString(",") + QString::number(0) + QString(",") + QString::number(1);
            TriggerTime = TIMES;
            SendMsgTypeFlag = ListenSerialThread::RS485DeviceAlarmState;
            SendDataPackage(CardID,TriggerTime);
            RS485DevicePreState[SlaveRS485Address] = 1;
        }

        break;
    }
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

//tcp短连接
void ListenSerialThread::slotSendCommonCode()
{
    if(AlarmMsgBuffer.size() > 0){
        tcpSocket->connectToHost(ServerIpAddress,ServerListenPort.toUInt());
        CommonSetting::Sleep(1000);

        if(ConnectStateFlag == ListenSerialThread::ConnectedState){
            QByteArray data =  AlarmMsgBuffer.takeFirst().toAscii();

            tcpSocket->write(data);
            CommonSetting::Sleep(1000);

            if(SendMsgTypeFlag == ListenSerialThread::SmartUSBAlarmState){
                qDebug() << "SmartUSB设备报警记录上传成功";
            }else if(SendMsgTypeFlag ==
                     ListenSerialThread::SmartUSBDeassertState){
                qDebug() << "SmartUSB设备报警解除记录上传成功";
            }else if(SendMsgTypeFlag == ListenSerialThread::SmartUSBNewInsert){
                qDebug() << "SmartUSB有新设备插入记录上传成功";
            }else if(SendMsgTypeFlag == ListenSerialThread::SmartUSBOffline){
                qDebug() << "SmartUSB设备离线记录上传成功";
            }else if(SendMsgTypeFlag == ListenSerialThread::RS485DeviceAlarmState){
                qDebug() << "RS485开关量模块报警记录上传成功";
            }else if(SendMsgTypeFlag == ListenSerialThread::RS485DeviceDeassertState){
                qDebug() << "RS485开关量模块报警解除记录上传成功";
            }

            tcpSocket->disconnectFromHost();
            tcpSocket->abort();
            return;
        }else if(ConnectStateFlag == ListenSerialThread::DisConnectedState){           
            qDebug() << "上传失败:与服务器连接未成功。请检查网线是否插好,本地网络配置,服务器IP,服务器监听端口配置是否正确.\n";
            tcpSocket->disconnectFromHost();
            tcpSocket->abort();
        }
    }
}
