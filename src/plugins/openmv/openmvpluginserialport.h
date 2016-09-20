#ifndef OPENMVPLUGINSERIALPORT_H
#define OPENMVPLUGINSERIALPORT_H

#include <QtCore>
#include <QtSerialPort>

#define OPENMVCAM_BAUD_RATE 12000000
#define OPENMVCAM_BAUD_RATE_2 921600

// Originally, the OpenMV Cam's firmware was written for libusb. However, libusb
// was not portable to Windows/Mac from Linux. The "hack" fix for this was to move
// the libsub serial functions into USB CDC calls. This only works as long as
// serial data is delivered in seperate USB packets to the camera. All the serial
// code in the IDE has been written to achieve this packetizing goal...

#define PACKET_LEN 60 // 64 byte packets don't work on Mac (must be mult of 4).

class OpenMVPluginSerialPort_private : public QObject
{
    Q_OBJECT

public:

    explicit OpenMVPluginSerialPort_private(QObject *parent = Q_NULLPTR);

public slots:

    void open(const QString &portName);
    void write(const QByteArray &data);

public slots: // private

    void processEvents();

signals:

    void openResult(const QString &errorMessage);
    void readAll(const QByteArray &data);

private:

    QSerialPort *m_port;
};

class OpenMVPluginSerialPort : public QObject
{
    Q_OBJECT

public:

    explicit OpenMVPluginSerialPort(QObject *parent = Q_NULLPTR);

signals:

    // Usage:
    //
    // 1: Create object on startup and destroy object on shutdown.
    // 2: Call open and receive the result from open result.
    // 3: Send with write and receive with read.
    // 4: Call write with null and receive read with null to close.

    void open(const QString &portName);
    void openResult(const QString &errorMessage);

    void write(const QByteArray &data);
    void readAll(const QByteArray &data);
};

#endif // OPENMVPLUGINSERIALPORT_H
