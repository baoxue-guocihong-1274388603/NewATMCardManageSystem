#ifndef PERSIONINFOCONTROL_H
#define PERSIONINFOCONTROL_H

#include <QWidget>
#include <QImage>

namespace Ui {
class PersionInfoControl;
}

class PersionInfoControl : public QWidget
{
    Q_OBJECT

public:
    explicit PersionInfoControl(QWidget *parent = 0);
    ~PersionInfoControl();
    void SetText(QString text);
    void ShowCertificatePic(QString url);
    void ShowLocalSnapPic(QString url);
    void ShowLocalSnapPic(QImage image);
    void ClearAll();

private:
    Ui::PersionInfoControl *ui;
};

#endif // PERSIONINFOCONTROL_H
