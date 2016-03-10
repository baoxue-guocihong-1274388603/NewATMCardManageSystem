#include "downloadcertificatepic.h"
#include "CommonSetting.h"

DownloadCertificatePic::DownloadCertificatePic(QObject *parent) :
    QObject(parent)
{
    manager = new QNetworkAccessManager(this);
    connect(manager,SIGNAL(finished(QNetworkReply*)),
            this,SLOT(slotReplyFinished(QNetworkReply*)));

    QTimer::singleShot(3000,this,SLOT(slotGetCardDetailInfo()));
}

void DownloadCertificatePic::slotGetCardDetailInfo()
{
    QString data = CommonSetting::ReadFile("/bin/GetCardDetialInfo.txt").trimmed();
    if (data == QString("OK")) {
        this->Download();
        qDebug() << "slotGetCardDetailInfo";
        return;
    }
}

void DownloadCertificatePic::slotGetCardInfo()
{
    this->Download();
    qDebug() << "slotGetCardInfo";
}

void DownloadCertificatePic::slotReplyFinished(QNetworkReply *reply)
{
    static int index = 0;

    if(reply->error() == QNetworkReply::NoError){
        QByteArray pic_data = reply->readAll();
        if(!pic_data.isEmpty()){
            QImage image;
            image.loadFromData(pic_data);
            QString fileName = "/sdcard/pic/" + reply->url().toString().split("/").last();
            if(fileName.size() > QString("/sdcard/pic/").size()){
                image.save(fileName,"JPG");
                query.exec(tr("UPDATE [卡号信息表] SET [身份证图片本地路径] = \"%1\" WHERE [卡号] = \"%2\"").arg(fileName).arg(CardIDList[index]));
                qDebug() << fileName;
                qDebug() << query.lastError();
            }
        }
    }

    reply->deleteLater();

    index++;
    if(index < CardIDList.size()){
        reply = manager->get(QNetworkRequest(CertificatePicUrlList.at(index)));
    }else{
        index = 0;
    }
}

//下载身份证图片到sdcard上
void DownloadCertificatePic::Download(void)
{
    CardIDList.clear();
    CertificatePicUrlList.clear();

    query.exec(tr("SELECT [卡号],[身份证图片网址] FROM [卡号信息表]"));
    while(query.next()){
        CardIDList << query.value(0).toString();
        CertificatePicUrlList << query.value(1).toString();
    }

    if(!CertificatePicUrlList.isEmpty()){
        reply = manager->get(QNetworkRequest(CertificatePicUrlList.at(0)));
        qDebug() << CertificatePicUrlList;
    }
}
