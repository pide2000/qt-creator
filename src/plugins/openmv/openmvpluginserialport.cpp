#include "openmvpluginserialport.h"

OpenMVPluginSerialPort_private::OpenMVPluginSerialPort_private(QObject *parent) : QObject(parent)
{
    m_port = Q_NULLPTR;
}

void OpenMVPluginSerialPort_private::open(const QString &portName)
{
    if(m_port)
    {
        delete m_port;
    }

    m_port = new QSerialPort(portName, this);

    connect(m_port, &QSerialPort::readyRead,
            this, &OpenMVPluginSerialPort_private::processEvents);

    if((!m_port->setBaudRate(OPENMVCAM_BAUD_RATE))
    || (!m_port->open(QIODevice::ReadWrite)))
    {
        delete m_port;
        m_port = new QSerialPort(portName, this);

        connect(m_port, &QSerialPort::readyRead,
                this, &OpenMVPluginSerialPort_private::processEvents);

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
            while(m_port->bytesToWrite())
            {
                m_port->waitForBytesWritten(1);
            }

            if(GET_END_DELAY(data.second))
            {
                QThread::msleep(GET_END_DELAY(data.second));
            }
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
