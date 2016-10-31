#include "openmvpluginserialport.h"

OpenMVPluginSerialPort_private::OpenMVPluginSerialPort_private(QObject *parent) : QObject(parent)
{
    m_port = Q_NULLPTR;
    // Bootloader Stuff //
    m_bootloaderStop = false;
}

void OpenMVPluginSerialPort_private::open(const QString &portName)
{
    if(m_port)
    {
        delete m_port;
    }

    m_port = new QSerialPort(portName, this);

    if((!m_port->setBaudRate(OPENMVCAM_BAUD_RATE))
    || (!m_port->open(QIODevice::ReadWrite)))
    {
        delete m_port;
        m_port = new QSerialPort(portName, this);

        if((!m_port->setBaudRate(OPENMVCAM_BAUD_RATE_2))
        || (!m_port->open(QIODevice::ReadWrite)))
        {
            emit openResult(m_port->errorString());
            delete m_port;
            m_port = Q_NULLPTR;
        }
    }

    if(m_port)
    {
        QTimer *timer = new QTimer(m_port);

        connect(timer, &QTimer::timeout,
                this, &OpenMVPluginSerialPort_private::processEvents);

        timer->start(1);

        emit openResult(QString());
    }
}

void OpenMVPluginSerialPort_private::write(const OpenMVPluginSerialPortData &data)
{
    if(data.first.isEmpty())
    {
        if(m_port)
        {
            delete m_port;
            m_port = Q_NULLPTR;
        }

        emit readAll(QByteArray());
    }
    else if(m_port)
    {
        if(GET_START_DELAY(data.second))
        {
            QThread::msleep(GET_START_DELAY(data.second));
        }

        m_port->clearError();

        if((m_port->write(data.first) != data.first.size()) || (!m_port->flush()))
        {
            delete m_port;
            m_port = Q_NULLPTR;
        }
        else
        {
            QElapsedTimer elaspedTimer;
            elaspedTimer.start();

            while(m_port->bytesToWrite())
            {
                m_port->waitForBytesWritten(1);

                if(elaspedTimer.hasExpired(WRITE_TIMEOUT))
                {
                    break;
                }
            }

            if(m_port->bytesToWrite())
            {
                delete m_port;
                m_port = Q_NULLPTR;
            }
            else if(GET_END_DELAY(data.second))
            {
                QThread::msleep(GET_END_DELAY(data.second));
            }
        }
    }
}

void OpenMVPluginSerialPort_private::processEvents()
{
    m_port->waitForReadyRead(1);
    QByteArray data = m_port->readAll();

    if(!data.isEmpty())
    {
        emit readAll(data);
    }
}

void OpenMVPluginSerialPort_private::isOpen()
{
    QCoreApplication::processEvents();
    emit isOpenResult(m_port);
}

// Bootloader Stuff Start /////////////////////////////////////////////////////

#define __USBDBG_CMD                    0x30
#define __USBDBG_SYS_RESET              0x0C

#define __BOOTLDR_START                 static_cast<int>(0xABCD0001)

#define BOOTLDR_START_RESPONSE_LEN      4

#define SYS_RESET_START_DELAY           0
#define SYS_RESET_END_DELAY             0

#define BOOTLDR_START_START_DELAY       0
#define BOOTLDR_START_END_DELAY         0

#define OPENMVCAM_VENDOR_ID             0x1209
#define OPENMVCAM_PRODUCT_ID            0xABD1

static void serializeByte(QByteArray &buffer, int value) // LittleEndian
{
    buffer.append(reinterpret_cast<const char *>(&value), 1);
}

static void serializeLong(QByteArray &buffer, int value) // LittleEndian
{
    buffer.append(reinterpret_cast<const char *>(&value), 4);
}

static int deserializeLong(QByteArray &buffer) // LittleEndian
{
    int r = int();
    memcpy(&r, buffer.data(), 4);
    buffer = buffer.mid(4);
    return r;
}

void OpenMVPluginSerialPort_private::bootloaderStart(bool closeFirst, const QString &selectedPort)
{
    if(closeFirst)
    {
        // Send System Reset

        QByteArray buffer;
        serializeByte(buffer, __USBDBG_CMD);
        serializeByte(buffer, __USBDBG_SYS_RESET);
        serializeLong(buffer, int());
        write(OpenMVPluginSerialPortData(buffer, SET_START_END_DELAY(SYS_RESET_START_DELAY, SYS_RESET_END_DELAY)));

        // Send Close

        if(m_port)
        {
            delete m_port;
            m_port = Q_NULLPTR;
        }
    }

    forever
    {
        QStringList stringList;

        foreach(QSerialPortInfo port, QSerialPortInfo::availablePorts())
        {
            if(port.hasVendorIdentifier() && (port.vendorIdentifier() == OPENMVCAM_VENDOR_ID)
            && port.hasProductIdentifier() && (port.productIdentifier() == OPENMVCAM_PRODUCT_ID))
            {
                stringList.append(port.portName());
            }
        }

        if(Utils::HostOsInfo::isMacHost())
        {
            stringList = stringList.filter(QStringLiteral("cu"), Qt::CaseInsensitive);
        }

        if(!stringList.isEmpty())
        {
            const QString portName = ((!selectedPort.isEmpty()) && stringList.contains(selectedPort)) ? selectedPort : stringList.first();

            m_port = new QSerialPort(portName, this);

            if((!m_port->setBaudRate(OPENMVCAM_BAUD_RATE))
            || (!m_port->open(QIODevice::ReadWrite)))
            {
                delete m_port;
                m_port = new QSerialPort(portName, this);

                if((!m_port->setBaudRate(OPENMVCAM_BAUD_RATE_2))
                || (!m_port->open(QIODevice::ReadWrite)))
                {
                    delete m_port;
                    m_port = Q_NULLPTR;
                }
            }

            if(m_port)
            {
                QByteArray response;

                for(int i = 0; i < 5; i++)
                {
                    QByteArray buffer;
                    serializeLong(buffer, __BOOTLDR_START);
                    write(OpenMVPluginSerialPortData(buffer, SET_START_END_DELAY(BOOTLDR_START_START_DELAY, BOOTLDR_START_END_DELAY)));

                    if(m_port)
                    {
                        QElapsedTimer elaspedTimer;
                        elaspedTimer.start();

                        do
                        {
                            m_port->waitForReadyRead(1);
                            response.append(m_port->readAll());
                        }
                        while((response.size() < BOOTLDR_START_RESPONSE_LEN) && (!elaspedTimer.hasExpired(2)));

                        if(response.size() >= BOOTLDR_START_RESPONSE_LEN)
                        {
                            if(deserializeLong(response) == __BOOTLDR_START)
                            {
                                elaspedTimer.start();

                                do
                                {
                                    m_port->waitForReadyRead(1);
                                    m_port->readAll();
                                }
                                while(!elaspedTimer.hasExpired(100));

                                QTimer *timer = new QTimer(m_port);

                                connect(timer, &QTimer::timeout,
                                        this, &OpenMVPluginSerialPort_private::processEvents);

                                timer->start(1);

                                emit bootloaderStartResponse(true);
                                return; // success
                            }

                            break; // try again
                        }
                    }
                    else
                    {
                        break; // try again
                    }
                }

                if(m_port)
                {
                    delete m_port;
                    m_port = Q_NULLPTR;
                }
            }
        }

        QCoreApplication::processEvents();

        if(m_bootloaderStop)
        {
            emit bootloaderStartResponse(false);
            return; // failure
        }
    }
}

void OpenMVPluginSerialPort_private::bootloaderStop()
{
    m_bootloaderStop = true;
    emit bootloaderStopResponse();
}

void OpenMVPluginSerialPort_private::bootloaderReset()
{
    m_bootloaderStop = false;
    emit bootloaderResetResponse();
}

// Bootloader Stuff End ///////////////////////////////////////////////////////

OpenMVPluginSerialPort::OpenMVPluginSerialPort(QObject *parent) : QObject(parent)
{
    QThread *thread = new QThread;
    OpenMVPluginSerialPort_private* port = new OpenMVPluginSerialPort_private;

    port->moveToThread(thread);

    connect(this, &OpenMVPluginSerialPort::open,
            port, &OpenMVPluginSerialPort_private::open);

    connect(port, &OpenMVPluginSerialPort_private::openResult,
            this, &OpenMVPluginSerialPort::openResult);

    connect(this, &OpenMVPluginSerialPort::write,
            port, &OpenMVPluginSerialPort_private::write);

    connect(port, &OpenMVPluginSerialPort_private::readAll,
            this, &OpenMVPluginSerialPort::readAll);

    connect(this, &OpenMVPluginSerialPort::isOpen,
            port, &OpenMVPluginSerialPort_private::isOpen);

    connect(port, &OpenMVPluginSerialPort_private::isOpenResult,
            this, &OpenMVPluginSerialPort::isOpenResult);

    // Bootloader Stuff Start //

    connect(this, &OpenMVPluginSerialPort::bootloaderStart,
            port, &OpenMVPluginSerialPort_private::bootloaderStart);

    connect(this, &OpenMVPluginSerialPort::bootloaderStop,
            port, &OpenMVPluginSerialPort_private::bootloaderStop);

    connect(this, &OpenMVPluginSerialPort::bootloaderReset,
            port, &OpenMVPluginSerialPort_private::bootloaderReset);

    connect(port, &OpenMVPluginSerialPort_private::bootloaderStartResponse,
            this, &OpenMVPluginSerialPort::bootloaderStartResponse);

    connect(port, &OpenMVPluginSerialPort_private::bootloaderStopResponse,
            this, &OpenMVPluginSerialPort::bootloaderStopResponse);

    connect(port, &OpenMVPluginSerialPort_private::bootloaderResetResponse,
            this, &OpenMVPluginSerialPort::bootloaderResetResponse);

    // Bootloader Stuff End //

    connect(this, &OpenMVPluginSerialPort::destroyed,
            port, &OpenMVPluginSerialPort_private::deleteLater);

    connect(port, &OpenMVPluginSerialPort_private::destroyed,
            thread, &QThread::quit);

    connect(thread, &QThread::finished,
            thread, &QThread::deleteLater);

    thread->start();
}
