#include "ReadCardID/readcardid.h"
#include "QextSerialPort/listenserialthread.h"
#include "TcpThread/tcpcommunicate.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    CommonSetting::SetUTF8Code();
    CommonSetting::OpenDataBase();

    system("/bin/UdpMulticastClient -qws &");
    CommonSetting::Sleep(1000);
    system("/bin/CheckMainProgramState -qws &");
    CommonSetting::Sleep(1000);

    //创建子线程1,用来与服务器进行tcp通信
    TcpCommunicate *tcp_communicate = new TcpCommunicate;
    QThread tcp_work_thread;
    tcp_communicate->moveToThread(&tcp_work_thread);
    tcp_work_thread.start();

    //创建子线程2,用来监听串口
    ListenSerialThread *listen_serial = new ListenSerialThread;
    QThread serial_work_thread;
    listen_serial->moveToThread(&serial_work_thread);
    serial_work_thread.start();

    //主线程：读卡号、拍照、解析刷卡序列、联动操作
    ReadCardID read_cardid;

    CommonSetting::WriteCommonFileTruncate("/bin/MainProgramState",QString("OK"));

    return app.exec();
}

//    system("/bin/ListenSerialThread -qws &");
//    CommonSetting::Sleep(1000);
//    system("/bin/TcpCommunicate -qws &");
//    CommonSetting::Sleep(1000);
