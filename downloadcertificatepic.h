#ifndef DOWNLOADCERTIFICATEPIC_H
#define DOWNLOADCERTIFICATEPIC_H

#include <QObject>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QSqlQuery>
#include <QImage>
#include <QStringList>

class DownloadCertificatePic : public QObject
{
    Q_OBJECT
public:
    explicit DownloadCertificatePic(QObject *parent = 0);
    void Download(void);

public slots:
    void slotGetCardDetailInfo();
    void slotGetCardInfo();
    void slotReplyFinished(QNetworkReply *);

public:
    QNetworkAccessManager *manager;
    QNetworkReply *reply;
    QStringList CertificatePicUrlList;//保存身份证图片网址列表
    QStringList CardIDList;
    QSqlQuery query;
};

#endif // DOWNLOADCERTIFICATEPIC_H
