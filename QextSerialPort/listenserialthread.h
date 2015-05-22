#ifndef LISTENSERIALTHREAD_H
#define LISTENSERIALTHREAD_H

#include "qextserialport.h"
#include "CommonSetting.h"

class ListenSerialThread : public QObject
{
    Q_OBJECT
public:
    explicit ListenSerialThread(QObject *parent = 0);
    QString ReadSerial();
    void SendDataPackage(QString CardID, QString TriggerTime);
    void SendCommonCode(QString MessageMerge);
    
public slots:
    void slotCloseConnection();
    void slotEstablishConnection();
    void slotDisplayError(QAbstractSocket::SocketError socketError);

    void PollingReadSmartUSBState();

private:
    enum SendMsgType{
        SmartUSBAlarmState,//SmartUSB设备报警
        SmartUSBDeassertState,//SmartUSB设备报警解除
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
};

#endif // LISTENSERIALTHREAD_H
