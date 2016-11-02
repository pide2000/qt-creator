#ifndef OPENMVPLUGINSERIALPORT_H
#define OPENMVPLUGINSERIALPORT_H

#include <QtCore>
#include <QtSerialPort>

#include <utils/hostosinfo.h>

#define OPENMVCAM_BAUD_RATE 12000000
#define OPENMVCAM_BAUD_RATE_2 921600

#define TABOO_PACKET_SIZE 64

#define SET_START_END_DELAY(start_delay, end_delay) ((((start_delay) & 0xFFFF) << 0) | (((end_delay) & 0xFFFF) << 16))
#define GET_START_DELAY(start_end_delay) (((start_end_delay) >> 0) & 0xFFFF)
#define GET_END_DELAY(start_end_delay) (((start_end_delay) >> 16) & 0xFFFF)

#define WRITE_TIMEOUT 3000

typedef QPair<QByteArray, int> OpenMVPluginSerialPortData;

class OpenMVPluginSerialPort_private : public QObject
{
    Q_OBJECT

public:

    explicit OpenMVPluginSerialPort_private(QObject *parent = Q_NULLPTR);

public slots:

    void open(const QString &portName);
    void write(const OpenMVPluginSerialPortData &data);
    void isOpen();

    // Bootloader Stuff //

    void bootloaderStart(bool closeFirst, const QString &selectedPort);
    void bootloaderStop();
    void bootloaderReset();

public slots: // private

    void processEvents();

signals:

    void openResult(const QString &errorMessage);
    void readAll(const QByteArray &data);
    void isOpenResult(bool result);

    // Bootloader Stuff //

    void bootloaderStartResponse(bool);
    void bootloaderStopResponse();
    void bootloaderResetResponse();

private:

    QSerialPort *m_port;

    // Bootloader Stuff //

    bool m_bootloaderStop;
};

class OpenMVPluginSerialPort : public QObject
{
    Q_OBJECT

public:

    explicit OpenMVPluginSerialPort(QObject *parent = Q_NULLPTR);

signals:

    void open(const QString &portName);
    void openResult(const QString &errorMessage);

    void write(const OpenMVPluginSerialPortData &data);
    void readAll(const QByteArray &data);

    void isOpen();
    void isOpenResult(bool result);

    // Bootloader Stuff //

    void bootloaderStart(bool closeFirst, const QString &selectedPort);
    void bootloaderStop();
    void bootloaderReset();

    void bootloaderStartResponse(bool);
    void bootloaderStopResponse();
    void bootloaderResetResponse();
};

#endif // OPENMVPLUGINSERIALPORT_H
