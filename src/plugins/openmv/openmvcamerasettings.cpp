#include "openmvcamerasettings.h"
#include "ui_openmvcamerasettings.h"

#define SETTINGS_GROUP "WiFiSettings"
#define WIFI_MODE "WiFiMode"
#define CLIENT_MODE_SSID "ClientModeSSID"
#define CLIENT_MODE_PASS "ClientModePass"
#define CLIENT_MODE_TYPE "ClientModeType"
#define ACCESS_POINT_MODE_SSID "AccessPointModeSSID"
#define ACCESS_POINT_MODE_PASS "AccessPointModePass"
#define ACCESS_POINT_MODE_TYPE "AccessPointModeType"
#define BOARD_NAME "BoardName"

OpenMVCameraSettings::OpenMVCameraSettings(const QString &fileName, QWidget *parent) : QDialog(parent), m_settings(new QSettings(fileName, QSettings::IniFormat, this)), m_ui(new Ui::OpenMVCameraSettings)
{
    m_settings->beginGroup(QStringLiteral(SETTINGS_GROUP));
    m_ui->setupUi(this);
    setWindowFlags(Qt::MSWindowsFixedSizeDialogHint | Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
                   (Utils::HostOsInfo::isMacHost() ? Qt::WindowType(0) : Qt::WindowCloseButtonHint));

    int wifiMode = m_settings->value(QStringLiteral(WIFI_MODE)).toInt();
    QString clientModeSSID = m_settings->value(QStringLiteral(CLIENT_MODE_SSID)).toString();
    QString clientModePass = m_settings->value(QStringLiteral(CLIENT_MODE_PASS)).toString();
    int clientModeType = m_settings->value(QStringLiteral(CLIENT_MODE_TYPE)).toInt();
    QString accessPointModeSSID = m_settings->value(QStringLiteral(ACCESS_POINT_MODE_SSID)).toString();
    QString accessPointModePass = m_settings->value(QStringLiteral(ACCESS_POINT_MODE_PASS)).toString();
    int accessPointModeType = m_settings->value(QStringLiteral(ACCESS_POINT_MODE_TYPE)).toInt();
    QString boardName = m_settings->value(QStringLiteral(BOARD_NAME)).toString();

    switch(wifiMode)
    {
        case 0:
        {
            m_ui->wifiSettingsBox->setChecked(false);
            m_ui->clientModeButton->setChecked(true);
            m_ui->accessPointModeButton->setChecked(false);
            break;
        }
        case 1:
        {
            m_ui->wifiSettingsBox->setChecked(true);
            m_ui->clientModeButton->setChecked(true);
            m_ui->accessPointModeButton->setChecked(false);
            break;
        }
        case 2:
        {
            m_ui->wifiSettingsBox->setChecked(true);
            m_ui->clientModeButton->setChecked(false);
            m_ui->accessPointModeButton->setChecked(true);
            break;
        }
        default:
        {
            m_ui->wifiSettingsBox->setChecked(false);
            m_ui->clientModeButton->setChecked(true);
            m_ui->accessPointModeButton->setChecked(false);
            break;
        }
    }

    QStringList list;

    QNetworkConfigurationManager *manager = new QNetworkConfigurationManager(this);

    foreach(const QNetworkConfiguration &config, manager->allConfigurations())
    {
        if((config.bearerType() == QNetworkConfiguration::BearerWLAN) && (!config.name().isEmpty()) && config.name().compare(clientModeSSID, Qt::CaseInsensitive))
        {
            list.append(config.name());
        }
    }

    list.sort();

    if(clientModeSSID.isEmpty())
    {
        m_ui->clientModeSSIDEntry->addItem(tr("Please enter or select your WiFi network here"));
    }
    else
    {
        m_ui->clientModeSSIDEntry->addItem(clientModeSSID);
    }

    m_ui->clientModeSSIDEntry->addItems(list);
    m_ui->clientModePasswordEntry->setText(clientModePass);
    m_ui->clientModeTypeEntry->setCurrentIndex(((0 <= clientModeType) && (clientModeType <= 2)) ? clientModeType : 0);

    m_ui->accessPointModeSSIDEntry->setText(accessPointModeSSID);
    m_ui->accessPointModePasswordEntry->setText(accessPointModePass);
    m_ui->accessPointModeTypeEntry->setCurrentIndex(((0 <= accessPointModeType) && (accessPointModeType <= 2)) ? accessPointModeType : 0);

    m_ui->boardNameEntry->setText(boardName);

    m_ui->clientModeWidget->setEnabled(m_ui->wifiSettingsBox->isChecked() && m_ui->clientModeButton->isChecked());
    m_ui->accessPointModeWidget->setEnabled(m_ui->wifiSettingsBox->isChecked() && m_ui->accessPointModeButton->isChecked());

    connect(m_ui->wifiSettingsBox, &QGroupBox::clicked, this, [this] {
        m_ui->clientModeWidget->setEnabled(m_ui->wifiSettingsBox->isChecked() && m_ui->clientModeButton->isChecked());
        m_ui->accessPointModeWidget->setEnabled(m_ui->wifiSettingsBox->isChecked() && m_ui->accessPointModeButton->isChecked());
    });

    connect(m_ui->clientModeButton, &QRadioButton::toggled,
            m_ui->clientModeWidget, &QWidget::setEnabled);

    connect(m_ui->accessPointModeButton, &QRadioButton::toggled,
            m_ui->accessPointModeWidget, &QWidget::setEnabled);
}

OpenMVCameraSettings::~OpenMVCameraSettings()
{
    delete m_ui;
}

void OpenMVCameraSettings::accept()
{
    m_settings->setValue(QStringLiteral(WIFI_MODE), m_ui->wifiSettingsBox->isChecked() ? ((m_ui->clientModeButton->isChecked() ? 1 : 0) | (m_ui->accessPointModeButton->isChecked() ? 2 : 0)) : 0);
    m_settings->setValue(QStringLiteral(CLIENT_MODE_SSID), m_ui->clientModeSSIDEntry->currentText());
    m_settings->setValue(QStringLiteral(CLIENT_MODE_PASS), m_ui->clientModePasswordEntry->text());
    m_settings->setValue(QStringLiteral(CLIENT_MODE_TYPE), m_ui->clientModeTypeEntry->currentIndex());
    m_settings->setValue(QStringLiteral(ACCESS_POINT_MODE_SSID), m_ui->accessPointModeSSIDEntry->text());
    m_settings->setValue(QStringLiteral(ACCESS_POINT_MODE_PASS), m_ui->accessPointModePasswordEntry->text());
    m_settings->setValue(QStringLiteral(ACCESS_POINT_MODE_TYPE), m_ui->accessPointModeTypeEntry->currentIndex());
    m_settings->setValue(QStringLiteral(BOARD_NAME), m_ui->boardNameEntry->text());

    QDialog::accept();
}
