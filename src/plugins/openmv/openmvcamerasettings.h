#ifndef OPENMVCAMERASETTINGS_H
#define OPENMVCAMERASETTINGS_H

#include <QDialog>

namespace Ui {
class OpenMVCameraSettings;
}

class OpenMVCameraSettings : public QDialog
{
    Q_OBJECT

public:
    explicit OpenMVCameraSettings(QWidget *parent = 0);
    ~OpenMVCameraSettings();

private:
    Ui::OpenMVCameraSettings *ui;
};

#endif // OPENMVCAMERASETTINGS_H
