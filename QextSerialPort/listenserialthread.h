#ifndef LISTENSERIALTHREAD_H
#define LISTENSERIALTHREAD_H

#include "qextserialport.h"
#include "CommonSetting.h"

#define TIMES QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss")

class ListenSerialThread : public QObject
{
    Q_OBJECT
public:
    explicit ListenSerialThread(QObject *parent = 0);
    QString ReadSerial();
    void SendDataPackage(QString CardID, QString TriggerTime);
    
public slots:
    void slotCloseConnection();
    void slotEstablishConnection();
    void slotDisplayError(QAbstractSocket::SocketError socketError);

    void slotPollingReadSmartUSBState();

    void slotSendCommonCode();

private:
    enum SendMsgType{
        SmartUSBAlarmState,//SmartUSB设备报警
        SmartUSBDeassertState,//SmartUSB设备报警解除
        SmartUSBNewInsert//SmartUSB有新设备插入
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

    QTimer *ReadSmartUSBStateTimer;

    QTimer *SendAlarmMsgTimer;//专门用来发送报警信息
    QList<QString> AlarmMsgBuffer;
};

#endif // LISTENSERIALTHREAD_H
