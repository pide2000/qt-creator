#include "openmvpluginserialport.h"

OpenMVPluginSerialPort_private::OpenMVPluginSerialPort_private(QObject *parent) : QObject(parent)
{
    m_port = Q_NULLPTR;
}

void OpenMVPluginSerialPort_private::open(const QString &portName)
{
    if(m_port)
    {
        QThread::msleep(2);
        delete m_port;
    }

    QThread::msleep(2);
    m_port = new QSerialPort(portName, this);
    QThread::msleep(2);

    connect(m_port, &QSerialPort::readyRead,
            this, &OpenMVPluginSerialPort_private::processEvents);

    if((!m_port->setBaudRate(OPENMVCAM_BAUD_RATE))
    || (!m_port->open(QIODevice::ReadWrite)))
    {
        // Try again with a fresh serial port.

        QThread::msleep(2);
        delete m_port;
        QThread::msleep(2);
        m_port = new QSerialPort(portName, this);
        QThread::msleep(2);

        connect(m_port, &QSerialPort::readyRead,
                this, &OpenMVPluginSerialPort_private::processEvents);

        if((!m_port->setBaudRate(OPENMVCAM_BAUD_RATE_2))
        || (!m_port->open(QIODevice::ReadWrite)))
        {
            QThread::msleep(2);
            emit openResult(m_port->errorString());
            delete m_port;
            m_port = Q_NULLPTR;
        }
    }

    if(m_port)
    {
        QThread::msleep(2);
        emit openResult(QString());

        QTimer *timer = new QTimer(m_port);

        connect(timer, &QTimer::timeout,
                this, &OpenMVPluginSerialPort_private::processEvents);

        timer->start(1);
    }
}

void OpenMVPluginSerialPort_private::write(const QByteArray &data)
{
    if(data.isEmpty())
    {
        QThread::msleep(2);
        emit readAll(QByteArray());

        if(m_port)
        {
            QThread::msleep(2);
            delete m_port;
            m_port = Q_NULLPTR;
        }
    }
    else if(m_port)
    {
        QThread::msleep(2);
        m_port->clearError();

        if((m_port->write(data) != data.size()) || (!m_port->flush()))
        {
            QThread::msleep(2);
            delete m_port;
            m_port = Q_NULLPTR;
        }
    }
}

void OpenMVPluginSerialPort_private::processEvents()
{
    QByteArray data = m_port->readAll();

    if(!data.isEmpty())
    {
        emit readAll(data);
    }
}

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

    connect(this, &OpenMVPluginSerialPort::destroyed,
            port, &OpenMVPluginSerialPort_private::deleteLater);

    connect(port, &OpenMVPluginSerialPort_private::destroyed,
            thread, &QThread::quit);

    connect(thread, &QThread::finished,
            thread, &QThread::deleteLater);

    thread->start();
}
