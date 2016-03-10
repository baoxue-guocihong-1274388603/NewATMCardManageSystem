#include "persioninfocontrol.h"
#include "ui_persioninfocontrol.h"

PersionInfoControl::PersionInfoControl(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::PersionInfoControl)
{
    ui->setupUi(this);

    layout()->setSizeConstraint(QLayout::SetFixedSize);
}

PersionInfoControl::~PersionInfoControl()
{
    delete ui;
}

void PersionInfoControl::SetText(QString text)
{
    ui->labPersionInfo->setText(text);
    ui->labPersionInfo->setFont(QFont("WenQuanYi Micro Hei",18));
}

void PersionInfoControl::ShowCertificatePic(QString url)//显示证件图片
{
    QImage image(url);
    ui->labCertificatePic->setPixmap(QPixmap::fromImage(image.scaled(ui->labCertificatePic->width(),ui->labCertificatePic->height())));
}

void PersionInfoControl::ShowLocalSnapPic(QString url)//显示本地现场抓拍图片
{
    QImage image(url);
    ui->labLocalSnapPic->setPixmap(QPixmap::fromImage(image.scaled(ui->labLocalSnapPic->width(),ui->labLocalSnapPic->height())));
}

void PersionInfoControl::ClearAll()
{
    ui->labPersionInfo->clear();
    ui->labLocalSnapPic->clear();
    ui->labCertificatePic->clear();
}
