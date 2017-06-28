#ifndef LISTENSERIALTHREAD_H
#define LISTENSERIALTHREAD_H

#include "qextserialport.h"
#include "CommonSetting.h"
#include <QMutex>

#define TIMES QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")

#define	FRAME_STX           0x16          // Frame header
#define	MAX_RecvFrame       42            // 接收缓存区大小
#define	MAX_TransFrame      42            // 发送缓存区大小
#define RECV_TIMEOUT        4            // 字节间的最大时间间隔, 单位为tick
                                            // 最小值可以为1, 如果为0则表示不进行超时判定

/* state constant(仅用于接收) */
#define FSA_INIT            0            //等待帧头
#define FSA_ADDR_D          1            //等待目的地址
#define FSA_ADDR_S          2            //等待源地址
#define FSA_LENGTH          3            //等待长度字节
#define FSA_DATA            4            //等待命令串(包括 命令ID 及 参数)
#define FSA_CHKSUM          5            //等待校验和

class ListenSerialThread : public QObject
{
    Q_OBJECT
public:
    explicit ListenSerialThread(QObject *parent = 0);

    static ListenSerialThread *newInstance() {
        static QMutex mutex;

        if (!instance) {
            QMutexLocker locker(&mutex);

            if (!instance) {
                instance = new ListenSerialThread();
            }
        }

        return instance;
    }

    void init();

    void SendDataPackage(QString CardID, QString TriggerTime);
    
    void start();
    void stop();

public slots:
    void slotSendSmartUSBCmd();//轮循SmartUSB设备
    void slotReadSmartUSBMsg();
    void slotParseSmartUSBState();

    void slotSendRS485Cmd();
    void slotPollingReadRS485DeviceState();
    void slotProcessRS485Msg();
    void slotProcessRS485ValidPacket();

    void slotCloseConnection();
    void slotEstablishConnection();
    void slotDisplayError(QAbstractSocket::SocketError socketError);

    void slotSendCommonCode();

private:
    static ListenSerialThread *instance;

    enum SendMsgType{
        SmartUSBAlarmState,//SmartUSB设备报警
        SmartUSBDeassertState,//SmartUSB设备报警解除
        SmartUSBNewInsert,//SmartUSB有新设备插入
        SmartUSBOffline, //SmartUSB设备离线

        RS485DeviceAlarmState,//RS485开关量模块报警
        RS485DeviceDeassertState//RS485开关量模块解除报警
    };
    volatile enum SendMsgType SendMsgTypeFlag;

    enum ConnectState{
        ConnectedState,
        DisConnectedState
    };
    volatile enum ConnectState ConnectStateFlag;

    QextSerialPort *mySerial;

    QTcpSocket *tcpSocket;

    QString ServerIpAddress;
    QString ServerListenPort;

    quint8 SmartUSBNumber;
    QTimer *SendSmartUSBCmdTimer;
    QTimer *ReadSmartUSBMsgTimer;
    QTimer *ParseSmartUSBStateTimer;
    QByteArray SmartUSBMsgBuffer;

    QTimer *SendAlarmMsgTimer;//专门用来发送报警信息
    QList<QString> AlarmMsgBuffer;


    quint8 RS485DeviceNumber;
    QTimer *SendRS485CmdTimer;
    QTimer *ReadRS485DeviceStateTimer;
    QTimer *ProcessRS485MsgTimer;
    QTimer *ProcessRS485ValidPacketTimer;

    QByteArray RS485RecvMsgBuffer;//串口接收到的数据全部都保存在这里
    QByteArray RS485RecvBuffer;
    QList<QByteArray> RS485ValidPacketBuffer;//保存有效的完整数据包

    quint8 RecvState;
    quint8 RecvChksum;
    quint8 RecvCtr;

    QByteArray RS485DevicePreState;//用来保存RS485开关量模块上一次的状态:0-不报警；1-报警
};

#endif // LISTENSERIALTHREAD_H
