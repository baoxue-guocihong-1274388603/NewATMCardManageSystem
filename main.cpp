#include "ReadCardID/readcardid.h"
#include "TcpThread/tcpcommunicate.h"
#include "downloadcertificatepic.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    CommonSetting::SetUTF8Code();
    CommonSetting::OpenDataBase();

//    system("/bin/UdpMulticastClient -qws &");
//    CommonSetting::Sleep(1000);
    system("/bin/CheckMainProgramState -qws &");
    CommonSetting::Sleep(1000);

    system("mkdir -p /sdcard/log");
    system("mkdir -p /sdcard/pic");//必须手动创建pic目录,不然图片保存不成功
    system("rm -rf /bin/GetCardDetialInfo.txt");

    //用来与服务器进行tcp通信
    TcpCommunicate *tcp_communicate = new TcpCommunicate();

    //读卡号、拍照、解析刷卡序列、联动操作
    ReadCardID read_cardid;
    read_cardid.showFullScreen();

    CommonSetting::WriteCommonFileTruncate("/bin/MainProgramState",QString("OK"));

    //下载身份证图片,必须放到最后,不然如果图片很多,会影响程序正常启动
    DownloadCertificatePic download;
    QObject::connect(tcp_communicate,SIGNAL(signalGetCardInfo()),&download,SLOT(slotGetCardInfo()));

    return app.exec();
}
