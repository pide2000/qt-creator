#include "openmvcamerasettings.h"
#include "ui_openmvcamerasettings.h"

OpenMVCameraSettings::OpenMVCameraSettings(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::OpenMVCameraSettings)
{
    ui->setupUi(this);
}

OpenMVCameraSettings::~OpenMVCameraSettings()
{
    delete ui;
}
