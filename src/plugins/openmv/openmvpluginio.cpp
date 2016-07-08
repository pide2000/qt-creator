#include "openmvpluginio.h"

#define __USBDBG_CMD                0x30
#define __USBDBG_FW_VERSION         0x80
#define __USBDBG_FRAME_SIZE         0x81
#define __USBDBG_FRAME_DUMP         0x82
#define __USBDBG_FRAME_UPDATE       0x04
#define __USBDBG_SCRIPT_EXEC        0x05
#define __USBDBG_SCRIPT_STOP        0x06
#define __USBDBG_SCRIPT_SAVE        0x07 // ???
#define __USBDBG_SCRIPT_RUNNING     0x87
#define __USBDBG_TEMPLATE_SAVE      0x08
#define __USBDBG_DESCRIPTOR_SAVE    0x09
#define __USBDBG_ATTR_READ          0x8A
#define __USBDBG_ATTR_WRITE         0x0B
#define __USBDBG_SYS_RESET          0x0C
#define __USBDBG_SYS_BOOT           0x0D // ???
#define __USBDBG_JPEG_ENABLE        0x0E
#define __USBDBG_TX_BUF_LEN         0x8E
#define __USBDBG_TX_BUF             0x8F

#define __BOOTLDR_START             0xABCD0001
#define __BOOTLDR_RESET             0xABCD0002
#define __BOOTLDR_ERASE             0xABCD0004
#define __BOOTLDR_WRITE             0xABCD0008

#define FW_VERSION_RESPONSE_LEN     12
#define FRAME_SIZE_RESPONSE_LEN     12
#define SCRIPT_RUNNING_RESPONSE_LEN 4
#define ATTR_READ_RESPONSE_LEN      1
#define TX_BUF_LEN_RESPONSE_LEN     4

#define BOOTLDR_START_RESPONSE_LEN  4

#define IS_JPG(bpp)                 ((bpp) >= 3)
#define IS_RGB(bpp)                 ((bpp) == 2)

static inline void serializeByte(QByteArray &buffer, char value) // LittleEndian
{
    buffer.append(reinterpret_cast<const char *>(&value), 1);
}

static inline void serializeWord(QByteArray &buffer, short value) // LittleEndian
{
    buffer.append(reinterpret_cast<const char *>(&value), 2);
}

static inline void serializeLong(QByteArray &buffer, long value) // LittleEndian
{
    buffer.append(reinterpret_cast<const char *>(&value), 4);
}

static inline char deserializeByte(QByteArray &buffer) // LittleEndian
{
    char r;
    memcpy(&r, buffer.data(), 1);
    buffer = buffer.mid(1);
    return r;
}

static inline short deserializeWord(QByteArray &buffer) // LittleEndian
{
    short r;
    memcpy(&r, buffer.data(), 2);
    buffer = buffer.mid(2);
    return r;
}

static inline long deserializeLong(QByteArray &buffer) // LittleEndian
{
    long r;
    memcpy(&r, buffer.data(), 4);
    buffer = buffer.mid(4);
    return r;
}

QByteArray byteSwap(QByteArray buffer, bool ok)
{
    if(ok)
    {
        char *data = buffer.data();

        for(int i = 0, j = (buffer.size() / 2) * 2; i < j; i += 2)
        {
            char tmp = data[i];
            data[i] = data[i+1];
            data[i+1] = tmp;
        }
    }

    return buffer;
}

OpenMVPluginIO::OpenMVPluginIO(OpenMVPluginSerialPort *port, QObject *parent) : QObject(parent)
{
    m_port = port;

    connect(m_port, &OpenMVPluginSerialPort::readAll,
            this, &OpenMVPluginIO::readAll);

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);

    connect(m_timer, &QTimer::timeout,
            this, &OpenMVPluginIO::timeout);

    m_commandQueue = QQueue<QByteArray>();
    m_expectedHeaderQueue = QQueue<int>();
    m_expectedDataQueue = QQueue<int>();
    m_receivedBytes = QByteArray();
    m_frameSizeW = int();
    m_frameSizeH = int();
    m_frameSizeBPP = int();

    QTimer *timer = new QTimer(this);

    connect(timer, &QTimer::timeout,
            this, &OpenMVPluginIO::processEvents);

    timer->start(1);
}

void OpenMVPluginIO::processEvents()
{
    if((!m_commandQueue.isEmpty()) && (!m_timer->isActive()))
    {
        m_port->write(m_commandQueue.dequeue());

        if(m_expectedHeaderQueue.head())
        {
            m_timer->start(1000);
        }
        else
        {
            m_expectedHeaderQueue.dequeue();
            m_expectedDataQueue.dequeue();
        }
    }
}

void OpenMVPluginIO::readAll(const QByteArray &data)
{
    if(data.isEmpty())
    {
        emit closeResponse();
    }
    else
    {
        if(!m_expectedHeaderQueue.isEmpty())
        {
            m_receivedBytes.append(data);

            if(m_receivedBytes.size() >= m_expectedDataQueue.head())
            {
                int receivedBytes = m_expectedDataQueue.dequeue();

                switch(m_expectedHeaderQueue.dequeue())
                {
                    case __USBDBG_FW_VERSION:
                    {
                        // The optimizer will mess up the order if executed in emit.
                        int major = deserializeLong(m_receivedBytes);
                        int minor = deserializeLong(m_receivedBytes);
                        int patch = deserializeLong(m_receivedBytes);
                        emit firmwareVersion(major, minor, patch);
                        break;
                    }
                    case __USBDBG_FRAME_SIZE:
                    {
                        int w = deserializeLong(m_receivedBytes);
                        int h = deserializeLong(m_receivedBytes);
                        int bpp = deserializeLong(m_receivedBytes);

                        if(w && h && bpp)
                        {
                            int size = IS_JPG(bpp) ? bpp : (w * h * bpp);

                            QByteArray buffer;
                            serializeByte(buffer, __USBDBG_CMD);
                            serializeByte(buffer, __USBDBG_FRAME_DUMP);
                            serializeLong(buffer, size);
                            m_commandQueue.push_front(buffer);

                            m_expectedHeaderQueue.push_front(__USBDBG_FRAME_DUMP);
                            m_expectedDataQueue.push_front(size);

                            m_frameSizeW = w;
                            m_frameSizeH = h;
                            m_frameSizeBPP = bpp;
                        }

                        break;
                    }
                    case __USBDBG_FRAME_DUMP:
                    {
                        emit frameBufferData(QPixmap::fromImage(IS_JPG(m_frameSizeBPP)
                        ? QImage::fromData(m_receivedBytes.left(receivedBytes), "JPG")
                        : QImage(reinterpret_cast<uchar *>(byteSwap(m_receivedBytes.left(receivedBytes),
                            IS_RGB(m_frameSizeBPP)).data()), m_frameSizeW, m_frameSizeH, m_frameSizeW * m_frameSizeBPP,
                            IS_RGB(m_frameSizeBPP) ? QImage::Format_RGB16 : QImage::Format_Grayscale8)));
                        m_receivedBytes = m_receivedBytes.mid(receivedBytes);
                        m_frameSizeW = int();
                        m_frameSizeH = int();
                        m_frameSizeBPP = int();
                        break;
                    }
                    case __USBDBG_SCRIPT_RUNNING:
                    {
                        emit scriptRunning(deserializeLong(m_receivedBytes));
                        break;
                    }
                    case __USBDBG_ATTR_READ:
                    {
                        emit attribute(deserializeByte(m_receivedBytes));
                        break;
                    }
                    case __USBDBG_TX_BUF_LEN:
                    {
                        int len = deserializeLong(m_receivedBytes);

                        if(len)
                        {
                            QByteArray buffer;
                            serializeByte(buffer, __USBDBG_CMD);
                            serializeByte(buffer, __USBDBG_TX_BUF);
                            serializeLong(buffer, len);
                            m_commandQueue.push_front(buffer);

                            m_expectedHeaderQueue.push_front(__USBDBG_TX_BUF);
                            m_expectedDataQueue.push_front(len);
                        }

                        break;
                    }
                    case __USBDBG_TX_BUF:
                    {
                        emit printData(m_receivedBytes.left(receivedBytes));
                        m_receivedBytes = m_receivedBytes.mid(receivedBytes);
                        break;
                    }
                    case __BOOTLDR_START:
                    {
                        emit gotBootloaderStart(deserializeLong(m_receivedBytes) == static_cast<long>(__BOOTLDR_START));
                        break;
                    }
                }

                m_timer->stop();

                m_receivedBytes.clear();
            }
        }
    }
}

void OpenMVPluginIO::timeout()
{
    switch(m_expectedHeaderQueue.dequeue())
    {
        case __USBDBG_FW_VERSION:
        {
            emit firmwareVersion(0, 0, 0);
            break;
        }
        case __USBDBG_FRAME_SIZE:
        {
            break;
        }
        case __USBDBG_FRAME_DUMP:
        {
            break;
        }
        case __USBDBG_SCRIPT_RUNNING:
        {
            emit scriptRunning(0);
            break;
        }
        case __USBDBG_ATTR_READ:
        {
            emit attribute(0);
            break;
        }
        case __USBDBG_TX_BUF_LEN:
        {
            break;
        }
        case __USBDBG_TX_BUF:
        {
            break;
        }
        case __BOOTLDR_START:
        {
            emit gotBootloaderStart(false);
            break;
        }
    }

    m_expectedDataQueue.dequeue();
    m_receivedBytes.clear();
    m_frameSizeW = int();
    m_frameSizeH = int();
    m_frameSizeBPP = int();
}

bool OpenMVPluginIO::frameSizeDumpQueued() const
{
    return m_expectedHeaderQueue.contains(__USBDBG_FRAME_SIZE) ||
           m_expectedHeaderQueue.contains(__USBDBG_FRAME_DUMP);
}

bool OpenMVPluginIO::getScriptRunningQueued() const
{
    return m_expectedHeaderQueue.contains(__USBDBG_SCRIPT_RUNNING);
}

bool OpenMVPluginIO::getAttributeQueued() const
{
    return m_expectedHeaderQueue.contains(__USBDBG_ATTR_READ);
}

bool OpenMVPluginIO::getTxBufferQueued() const
{
    return m_expectedHeaderQueue.contains(__USBDBG_TX_BUF_LEN) ||
           m_expectedHeaderQueue.contains(__USBDBG_TX_BUF);
}

void OpenMVPluginIO::getFirmwareVersion()
{
    QByteArray buffer;
    serializeByte(buffer, __USBDBG_CMD);
    serializeByte(buffer, __USBDBG_FW_VERSION);
    serializeLong(buffer, FW_VERSION_RESPONSE_LEN);
    m_commandQueue.enqueue(buffer);

    m_expectedHeaderQueue.enqueue(__USBDBG_FW_VERSION);
    m_expectedDataQueue.enqueue(FW_VERSION_RESPONSE_LEN);
}

void OpenMVPluginIO::frameSizeDump()
{
    QByteArray buffer;
    serializeByte(buffer, __USBDBG_CMD);
    serializeByte(buffer, __USBDBG_FRAME_SIZE);
    serializeLong(buffer, FRAME_SIZE_RESPONSE_LEN);
    m_commandQueue.enqueue(buffer);

    m_expectedHeaderQueue.enqueue(__USBDBG_FRAME_SIZE);
    m_expectedDataQueue.enqueue(FRAME_SIZE_RESPONSE_LEN);
}

void OpenMVPluginIO::frameUpdate()
{
    QByteArray buffer;
    serializeByte(buffer, __USBDBG_CMD);
    serializeByte(buffer, __USBDBG_FRAME_UPDATE);
    serializeLong(buffer, 0);
    m_commandQueue.enqueue(buffer);

    m_expectedHeaderQueue.enqueue(0);
    m_expectedDataQueue.enqueue(0);
}

void OpenMVPluginIO::scriptExec(const QByteArray &data)
{
    QByteArray buffer;
    serializeByte(buffer, __USBDBG_CMD);
    serializeByte(buffer, __USBDBG_SCRIPT_EXEC);
    serializeLong(buffer, data.size());
    m_commandQueue.enqueue(buffer);

    m_expectedHeaderQueue.enqueue(0);
    m_expectedDataQueue.enqueue(0);

    m_commandQueue.enqueue(data);

    m_expectedHeaderQueue.enqueue(0);
    m_expectedDataQueue.enqueue(0);
}

void OpenMVPluginIO::scriptStop()
{
    QByteArray buffer;
    serializeByte(buffer, __USBDBG_CMD);
    serializeByte(buffer, __USBDBG_SCRIPT_STOP);
    serializeLong(buffer, 0);
    m_commandQueue.enqueue(buffer);

    m_expectedHeaderQueue.enqueue(0);
    m_expectedDataQueue.enqueue(0);
}

void OpenMVPluginIO::getScriptRunning()
{
    QByteArray buffer;
    serializeByte(buffer, __USBDBG_CMD);
    serializeByte(buffer, __USBDBG_SCRIPT_RUNNING);
    serializeLong(buffer, SCRIPT_RUNNING_RESPONSE_LEN);
    m_commandQueue.enqueue(buffer);

    m_expectedHeaderQueue.enqueue(__USBDBG_SCRIPT_RUNNING);
    m_expectedDataQueue.enqueue(SCRIPT_RUNNING_RESPONSE_LEN);
}

void OpenMVPluginIO::templateSave(long x, long y, long w, long h, const QByteArray &path)
{
    QByteArray buffer;
    serializeByte(buffer, __USBDBG_CMD);
    serializeByte(buffer, __USBDBG_TEMPLATE_SAVE);
    serializeLong(buffer, sizeof(x) + sizeof(y) + sizeof(w) + sizeof(h) + path.size());
    m_commandQueue.enqueue(buffer);

    m_expectedHeaderQueue.enqueue(0);
    m_expectedDataQueue.enqueue(0);

    buffer = QByteArray();
    serializeLong(buffer, x);
    serializeLong(buffer, y);
    serializeLong(buffer, w);
    serializeLong(buffer, h);
    m_commandQueue.enqueue(buffer + path);

    m_expectedHeaderQueue.enqueue(0);
    m_expectedDataQueue.enqueue(0);
}

void OpenMVPluginIO::descriptorSave(short x, short y, short w, short h, const QByteArray &path)
{
    QByteArray buffer;
    serializeByte(buffer, __USBDBG_CMD);
    serializeByte(buffer, __USBDBG_DESCRIPTOR_SAVE);
    serializeLong(buffer, sizeof(x) + sizeof(y) + sizeof(w) + sizeof(h) + path.size());
    m_commandQueue.enqueue(buffer);

    m_expectedHeaderQueue.enqueue(0);
    m_expectedDataQueue.enqueue(0);

    buffer = QByteArray();
    serializeLong(buffer, x);
    serializeLong(buffer, y);
    serializeLong(buffer, w);
    serializeLong(buffer, h);
    m_commandQueue.enqueue(buffer + path);

    m_expectedHeaderQueue.enqueue(0);
    m_expectedDataQueue.enqueue(0);
}

void OpenMVPluginIO::getAttribute(short attribute)
{
    QByteArray buffer;
    serializeByte(buffer, __USBDBG_CMD);
    serializeByte(buffer, __USBDBG_ATTR_READ);
    serializeLong(buffer, ATTR_READ_RESPONSE_LEN);
    serializeWord(buffer, attribute);
    m_commandQueue.enqueue(buffer);

    m_expectedHeaderQueue.enqueue(__USBDBG_ATTR_READ);
    m_expectedDataQueue.enqueue(ATTR_READ_RESPONSE_LEN);
}

void OpenMVPluginIO::setAttribute(short attribute, short value)
{
    QByteArray buffer;
    serializeByte(buffer, __USBDBG_CMD);
    serializeByte(buffer, __USBDBG_ATTR_WRITE);
    serializeLong(buffer, 0);
    serializeWord(buffer, attribute);
    serializeWord(buffer, value);
    m_commandQueue.enqueue(buffer);

    m_expectedHeaderQueue.enqueue(0);
    m_expectedDataQueue.enqueue(0);
}

void OpenMVPluginIO::sysReset()
{
    QByteArray buffer;
    serializeByte(buffer, __USBDBG_CMD);
    serializeByte(buffer, __USBDBG_SYS_RESET);
    serializeLong(buffer, 0);
    m_commandQueue.enqueue(buffer);

    m_expectedHeaderQueue.enqueue(0);
    m_expectedDataQueue.enqueue(0);
}

void OpenMVPluginIO::jpegEnable(bool enabled)
{
    QByteArray buffer;
    serializeByte(buffer, __USBDBG_CMD);
    serializeByte(buffer, __USBDBG_JPEG_ENABLE);
    serializeLong(buffer, 0);
    serializeWord(buffer, enabled ? true : false);
    m_commandQueue.enqueue(buffer);

    m_expectedHeaderQueue.enqueue(0);
    m_expectedDataQueue.enqueue(0);
}

void OpenMVPluginIO::getTxBuffer()
{
    QByteArray buffer;
    serializeByte(buffer, __USBDBG_CMD);
    serializeByte(buffer, __USBDBG_TX_BUF_LEN);
    serializeLong(buffer, TX_BUF_LEN_RESPONSE_LEN);
    m_commandQueue.enqueue(buffer);

    m_expectedHeaderQueue.enqueue(__USBDBG_TX_BUF_LEN);
    m_expectedDataQueue.enqueue(TX_BUF_LEN_RESPONSE_LEN);
}

void OpenMVPluginIO::bootloaderStart()
{
    QByteArray buffer;
    serializeLong(buffer, __BOOTLDR_START);
    m_commandQueue.enqueue(buffer);

    m_expectedHeaderQueue.enqueue(__BOOTLDR_START);
    m_expectedDataQueue.enqueue(BOOTLDR_START_RESPONSE_LEN);
}

void OpenMVPluginIO::bootloaderReset()
{
    QByteArray buffer;
    serializeLong(buffer, __BOOTLDR_RESET);
    m_commandQueue.enqueue(buffer);

    m_expectedHeaderQueue.enqueue(0);
    m_expectedDataQueue.enqueue(0);
}

void OpenMVPluginIO::flashErase(long sector)
{
    QByteArray buffer;
    serializeLong(buffer, __BOOTLDR_ERASE);
    serializeLong(buffer, sector);
    m_commandQueue.enqueue(buffer);

    m_expectedHeaderQueue.enqueue(0);
    m_expectedDataQueue.enqueue(0);
}

void OpenMVPluginIO::flashWrite(const QByteArray &data)
{
    QByteArray buffer;
    serializeLong(buffer, __BOOTLDR_WRITE);
    m_commandQueue.enqueue(buffer + data);

    m_expectedHeaderQueue.enqueue(0);
    m_expectedDataQueue.enqueue(0);
}

void OpenMVPluginIO::close()
{
    m_commandQueue.enqueue(QByteArray());

    m_expectedHeaderQueue.enqueue(0);
    m_expectedDataQueue.enqueue(0);
}
