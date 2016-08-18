#ifndef OPENMVPLUGINSERIALPORT_H
#define OPENMVPLUGINSERIALPORT_H

#include <QtCore>
#include <QtSerialPort>

#define OPENMVCAM_BAUD_RATE 12000000
#define OPENMVCAM_BAUD_RATE_2 921600

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
    void shutdown(const QString &errorMessage);

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
    // 4: Shutdown emitted on error when the port needs to close.
    // 5: Call write with null and receive read with null to close.

    void open(const QString &portName);
    void openResult(const QString &errorMessage);

    void write(const QByteArray &data);
    void readAll(const QByteArray &data);

    void shutdown(const QString &errorMessage);
};

#endif // OPENMVPLUGINSERIALPORT_H
