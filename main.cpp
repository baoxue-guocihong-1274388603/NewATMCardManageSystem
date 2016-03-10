#include "ReadCardID/readcardid.h"
#include "QextSerialPort/listenserialthread.h"
#include "TcpThread/tcpcommunicate.h"
#include "downloadcertificatepic.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    CommonSetting::SetUTF8Code();
    CommonSetting::OpenDataBase();

    system("/bin/UdpMulticastClient -qws &");
    CommonSetting::Sleep(1000);
    system("/bin/CheckMainProgramState -qws &");
    CommonSetting::Sleep(1000);

    system("mkdir -p /sdcard/log");
    system("mkdir -p /sdcard/pic");//必须手动创建pic目录,不然图片保存不成功
    system("rm -rf /bin/GetCardDetialInfo.txt");

    //创建子线程1,用来与服务器进行tcp通信
    TcpCommunicate *tcp_communicate = new TcpCommunicate;
    QThread tcp_work_thread;
    tcp_communicate->moveToThread(&tcp_work_thread);
    tcp_work_thread.start();

#ifdef USES_SHUAKAJI_WAIKE//如果用的是加油站外壳
    //创建子线程2,用来监听串口
    ListenSerialThread *listen_serial = new ListenSerialThread;
    QThread serial_work_thread;
    listen_serial->moveToThread(&serial_work_thread);
    serial_work_thread.start();
#endif

    //主线程：读卡号、拍照、解析刷卡序列、联动操作
    ReadCardID read_cardid;
    read_cardid.showFullScreen();

    CommonSetting::WriteCommonFileTruncate("/bin/MainProgramState",QString("OK"));

    //下载身份证图片,必须放到最后,不然如果图片很多,会影响程序正常启动
    DownloadCertificatePic download;
    QObject::connect(tcp_communicate,SIGNAL(signalGetCardInfo()),&download,SLOT(slotGetCardInfo()));

    return app.exec();
}
