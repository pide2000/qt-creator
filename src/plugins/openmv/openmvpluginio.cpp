#include "openmvpluginio.h"

#define __USBDBG_CMD                    0x30
#define __USBDBG_FW_VERSION             0x80
#define __USBDBG_FRAME_SIZE             0x81
#define __USBDBG_FRAME_DUMP             0x82
#define __USBDBG_ARCH_STR               0x83
#define __USBDBG_SCRIPT_EXEC            0x05
#define __USBDBG_SCRIPT_STOP            0x06
#define __USBDBG_SCRIPT_RUNNING         0x87
#define __USBDBG_TEMPLATE_SAVE          0x08
#define __USBDBG_DESCRIPTOR_SAVE        0x09
#define __USBDBG_ATTR_READ              0x8A
#define __USBDBG_ATTR_WRITE             0x0B
#define __USBDBG_SYS_RESET              0x0C
#define __USBDBG_FB_ENABLE              0x0D
#define __USBDBG_JPEG_ENABLE            0x0E
#define __USBDBG_TX_BUF_LEN             0x8E
#define __USBDBG_TX_BUF                 0x8F

#define __BOOTLDR_START                 static_cast<int>(0xABCD0001)
#define __BOOTLDR_RESET                 static_cast<int>(0xABCD0002)
#define __BOOTLDR_ERASE                 static_cast<int>(0xABCD0004)
#define __BOOTLDR_WRITE                 static_cast<int>(0xABCD0008)

#define FW_VERSION_RESPONSE_LEN         12
#define ARCH_STR_RESPONSE_LEN           64
#define FRAME_SIZE_RESPONSE_LEN         12
#define SCRIPT_RUNNING_RESPONSE_LEN     4
#define ATTR_READ_RESPONSE_LEN          1
#define TX_BUF_LEN_RESPONSE_LEN         4

#define BOOTLDR_START_RESPONSE_LEN      4

#define IS_JPG(bpp)                     ((bpp) >= 3)
#define IS_RGB(bpp)                     ((bpp) == 2)
#define IS_GS(bpp)                      ((bpp) == 1)
#define IS_BINARY(bpp)                  ((bpp) == 0)

#define FW_VERSION_START_DELAY          100
#define FW_VERSION_END_DELAY            0
#define FRAME_SIZE_START_DELAY          0
#define FRAME_SIZE_END_DELAY            0
#define FRAME_DUMP_START_DELAY          0
#define FRAME_DUMP_END_DELAY            0
#define ARCH_STR_START_DELAY            0
#define ARCH_STR_END_DELAY              0
#define SCRIPT_EXEC_START_DELAY         0
#define SCRIPT_EXEC_END_DELAY           0
#define SCRIPT_EXEC_2_START_DELAY       0
#define SCRIPT_EXEC_2_END_DELAY         0
#define SCRIPT_STOP_START_DELAY         50
#define SCRIPT_STOP_END_DELAY           50
#define SCRIPT_RUNNING_START_DELAY      0
#define SCRIPT_RUNNING_END_DELAY        0
#define TEMPLATE_SAVE_START_DELAY       0
#define TEMPLATE_SAVE_END_DELAY         0
#define TEMPLATE_SAVE_2_START_DELAY     0
#define TEMPLATE_SAVE_2_END_DELAY       0
#define DESCRIPTOR_SAVE_START_DELAY     0
#define DESCRIPTOR_SAVE_END_DELAY       0
#define DESCRIPTOR_SAVE_2_START_DELAY   0
#define DESCRIPTOR_SAVE_2_END_DELAY     0
#define ATTR_READ_START_DELAY           0
#define ATTR_READ_END_DELAY             0
#define ATTR_WRITE_START_DELAY          0
#define ATTR_WRITE_END_DELAY            0
#define SYS_RESET_START_DELAY           0
#define SYS_RESET_END_DELAY             0
#define FB_ENABLE_START_DELAY           0
#define FB_ENABLE_END_DELAY             0
#define JPEG_ENABLE_START_DELAY         0
#define JPEG_ENABLE_END_DELAY           0
#define TX_BUF_LEN_START_DELAY          0
#define TX_BUF_LEN_END_DELAY            0
#define TX_BUF_START_DELAY              0
#define TX_BUF_END_DELAY                0

#define BOOTLDR_START_START_DELAY       0
#define BOOTLDR_START_END_DELAY         0
#define BOOTLDR_RESET_START_DELAY       0
#define BOOTLDR_RESET_END_DELAY         0
#define BOOTLDR_ERASE_START_DELAY       0
#define BOOTLDR_ERASE_END_DELAY         0
#define BOOTLDR_WRITE_START_DELAY       0
#define BOOTLDR_WRITE_END_DELAY         0

static void serializeByte(QByteArray &buffer, int value) // LittleEndian
{
    buffer.append(reinterpret_cast<const char *>(&value), 1);
}

static void serializeWord(QByteArray &buffer, int value) // LittleEndian
{
    buffer.append(reinterpret_cast<const char *>(&value), 2);
}

static void serializeLong(QByteArray &buffer, int value) // LittleEndian
{
    buffer.append(reinterpret_cast<const char *>(&value), 4);
}

static int deserializeByte(QByteArray &buffer) // LittleEndian
{
    int r = int();
    memcpy(&r, buffer.data(), 1);
    buffer = buffer.mid(1);
    return r;
}

//static int deserializeWord(QByteArray &buffer) // LittleEndian
//{
//    int r = int();
//    memcpy(&r, buffer.data(), 2);
//    buffer = buffer.mid(2);
//    return r;
//}

static int deserializeLong(QByteArray &buffer) // LittleEndian
{
    int r = int();
    memcpy(&r, buffer.data(), 4);
    buffer = buffer.mid(4);
    return r;
}

static QByteArray byteSwap(QByteArray buffer, bool ok)
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

    m_processingResponse = int();
    m_commandQueue = QQueue<OpenMVPluginSerialPortData>();
    m_expectedHeaderQueue = QQueue<int>();
    m_expectedDataQueue = QQueue<int>();
    m_receivedBytes = QByteArray();
    m_frameSizeW = int();
    m_frameSizeH = int();
    m_frameSizeBPP = int();
    m_lineBuffer = QByteArray();

    QTimer *timer = new QTimer(this);

    connect(timer, &QTimer::timeout,
            this, &OpenMVPluginIO::processEvents);

    timer->start(1);
}

void OpenMVPluginIO::processEvents()
{
    if((!m_commandQueue.isEmpty()) && (!m_processingResponse))
    {
        m_port->write(m_commandQueue.dequeue());

        if(m_expectedHeaderQueue.head() && m_expectedDataQueue.head())
        {
            m_timer->start(USBDBG_COMMAND_TIMEOUT);
            m_processingResponse = m_expectedHeaderQueue.head();
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
        if(m_lineBuffer.size())
        {
            emit printData(m_lineBuffer);
            m_lineBuffer.clear();
        }

        emit closeResponse();
    }
    else if((!m_expectedHeaderQueue.isEmpty()) && (!m_expectedDataQueue.isEmpty()))
    {
        m_receivedBytes.append(data);

        if(m_receivedBytes.size() >= m_expectedDataQueue.head())
        {
            m_timer->stop();
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
                case __USBDBG_ARCH_STR:
                {
                    emit archString(QString::fromLatin1(m_receivedBytes.left(receivedBytes)));
                    m_receivedBytes = m_receivedBytes.mid(receivedBytes);
                    break;
                }
                case __USBDBG_FRAME_SIZE:
                {
                    int w = deserializeLong(m_receivedBytes);
                    int h = deserializeLong(m_receivedBytes);
                    int bpp = deserializeLong(m_receivedBytes);

                    if(w && h)
                    {
                        int size = IS_JPG(bpp) ? bpp : ((IS_RGB(bpp) || IS_GS(bpp)) ? (w * h * bpp) : (((w+7)/8) * h));

                        QByteArray buffer;
                        serializeByte(buffer, __USBDBG_CMD);
                        serializeByte(buffer, __USBDBG_FRAME_DUMP);
                        serializeLong(buffer, size);
                        m_commandQueue.push_front(OpenMVPluginSerialPortData(buffer, SET_START_END_DELAY(FRAME_DUMP_START_DELAY, FRAME_DUMP_END_DELAY)));

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
                    QPixmap pixmap = QPixmap::fromImage(IS_JPG(m_frameSizeBPP)
                    ? QImage::fromData(m_receivedBytes.left(receivedBytes), "JPG")
                    : QImage(reinterpret_cast<const uchar *>(byteSwap(m_receivedBytes.left(receivedBytes),
                        IS_RGB(m_frameSizeBPP)).constData()), m_frameSizeW, m_frameSizeH, IS_BINARY(m_frameSizeBPP) ? ((m_frameSizeW+7)/8) : (m_frameSizeW * m_frameSizeBPP),
                        IS_RGB(m_frameSizeBPP) ? QImage::Format_RGB16 : (IS_GS(m_frameSizeBPP) ? QImage::Format_Grayscale8 : QImage::Format_MonoLSB)));

                    if(!pixmap.isNull())
                    {
                        emit frameBufferData(pixmap);
                    }

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
                        m_commandQueue.push_front(OpenMVPluginSerialPortData(buffer, SET_START_END_DELAY(TX_BUF_START_DELAY, TX_BUF_END_DELAY)));

                        m_expectedHeaderQueue.push_front(__USBDBG_TX_BUF);
                        m_expectedDataQueue.push_front(len);
                    }
                    else if(m_lineBuffer.size())
                    {
                        emit printData(m_lineBuffer);
                        m_lineBuffer.clear();
                    }

                    break;
                }
                case __USBDBG_TX_BUF:
                {
                    m_lineBuffer.append(m_receivedBytes.left(receivedBytes));
                    QByteArrayList list = m_lineBuffer.split('\n');
                    m_lineBuffer = list.takeLast();

                    if(list.size())
                    {
                        emit printData(list.join('\n') + '\n');
                    }

                    m_receivedBytes = m_receivedBytes.mid(receivedBytes);
                    break;
                }
                case __BOOTLDR_START:
                {
                    emit gotBootloaderStart(deserializeLong(m_receivedBytes) == __BOOTLDR_START);
                    break;
                }
            }

            m_receivedBytes.clear();
            m_processingResponse = int();
        }
    }
}

void OpenMVPluginIO::timeout()
{
    m_expectedDataQueue.dequeue();
    m_receivedBytes.clear();
    m_frameSizeW = int();
    m_frameSizeH = int();
    m_frameSizeBPP = int();

    switch(m_expectedHeaderQueue.dequeue())
    {
        case __USBDBG_FW_VERSION:
        {
            emit firmwareVersion(int(), int(), int());
            break;
        }
        case __USBDBG_ARCH_STR:
        {
            emit archString(QString());
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
            emit scriptRunning(bool());
            break;
        }
        case __USBDBG_ATTR_READ:
        {
            emit attribute(int());
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

    m_processingResponse = int();
}

bool OpenMVPluginIO::frameSizeDumpQueued() const
{
    return m_expectedHeaderQueue.contains(__USBDBG_FRAME_SIZE) ||
           m_expectedHeaderQueue.contains(__USBDBG_FRAME_DUMP) ||
           (m_processingResponse == __USBDBG_FRAME_SIZE) ||
           (m_processingResponse == __USBDBG_FRAME_DUMP);
}

bool OpenMVPluginIO::getScriptRunningQueued() const
{
    return m_expectedHeaderQueue.contains(__USBDBG_SCRIPT_RUNNING) ||
           (m_processingResponse == __USBDBG_SCRIPT_RUNNING);
}

bool OpenMVPluginIO::getAttributeQueued() const
{
    return m_expectedHeaderQueue.contains(__USBDBG_ATTR_READ) ||
           (m_processingResponse == __USBDBG_ATTR_READ);
}

bool OpenMVPluginIO::getTxBufferQueued() const
{
    return m_expectedHeaderQueue.contains(__USBDBG_TX_BUF_LEN) ||
           m_expectedHeaderQueue.contains(__USBDBG_TX_BUF) ||
           (m_processingResponse == __USBDBG_TX_BUF_LEN) ||
           (m_processingResponse == __USBDBG_TX_BUF);
}

void OpenMVPluginIO::getFirmwareVersion()
{
    QByteArray buffer;
    serializeByte(buffer, __USBDBG_CMD);
    serializeByte(buffer, __USBDBG_FW_VERSION);
    serializeLong(buffer, FW_VERSION_RESPONSE_LEN);
    m_commandQueue.enqueue(OpenMVPluginSerialPortData(buffer, SET_START_END_DELAY(FW_VERSION_START_DELAY, FW_VERSION_END_DELAY)));
    m_expectedHeaderQueue.enqueue(__USBDBG_FW_VERSION);
    m_expectedDataQueue.enqueue(FW_VERSION_RESPONSE_LEN);
}

void OpenMVPluginIO::getArchString()
{
    QByteArray buffer;
    serializeByte(buffer, __USBDBG_CMD);
    serializeByte(buffer, __USBDBG_ARCH_STR);
    serializeLong(buffer, ARCH_STR_RESPONSE_LEN);
    m_commandQueue.enqueue(OpenMVPluginSerialPortData(buffer, SET_START_END_DELAY(ARCH_STR_START_DELAY, ARCH_STR_END_DELAY)));
    m_expectedHeaderQueue.enqueue(__USBDBG_ARCH_STR);
    m_expectedDataQueue.enqueue(ARCH_STR_RESPONSE_LEN);
}

void OpenMVPluginIO::frameSizeDump()
{
    QByteArray buffer;
    serializeByte(buffer, __USBDBG_CMD);
    serializeByte(buffer, __USBDBG_FRAME_SIZE);
    serializeLong(buffer, FRAME_SIZE_RESPONSE_LEN);
    m_commandQueue.enqueue(OpenMVPluginSerialPortData(buffer, SET_START_END_DELAY(FRAME_SIZE_START_DELAY, FRAME_SIZE_END_DELAY)));
    m_expectedHeaderQueue.enqueue(__USBDBG_FRAME_SIZE);
    m_expectedDataQueue.enqueue(FRAME_SIZE_RESPONSE_LEN);
}

void OpenMVPluginIO::scriptExec(const QByteArray &data)
{
    QByteArray buffer, script = (data.size() % TABOO_PACKET_SIZE) ? data : (data + '\n');
    serializeByte(buffer, __USBDBG_CMD);
    serializeByte(buffer, __USBDBG_SCRIPT_EXEC);
    serializeLong(buffer, script.size());
    m_commandQueue.enqueue(OpenMVPluginSerialPortData(buffer, SET_START_END_DELAY(SCRIPT_EXEC_START_DELAY, SCRIPT_EXEC_END_DELAY)));
    m_expectedHeaderQueue.enqueue(int());
    m_expectedDataQueue.enqueue(int());
    m_commandQueue.enqueue(OpenMVPluginSerialPortData(script, SET_START_END_DELAY(SCRIPT_EXEC_2_START_DELAY, SCRIPT_EXEC_2_END_DELAY)));
    m_expectedHeaderQueue.enqueue(int());
    m_expectedDataQueue.enqueue(int());
}

void OpenMVPluginIO::scriptStop()
{
    QByteArray buffer;
    serializeByte(buffer, __USBDBG_CMD);
    serializeByte(buffer, __USBDBG_SCRIPT_STOP);
    serializeLong(buffer, int());
    m_commandQueue.enqueue(OpenMVPluginSerialPortData(buffer, SET_START_END_DELAY(SCRIPT_STOP_START_DELAY, SCRIPT_STOP_END_DELAY)));
    m_expectedHeaderQueue.enqueue(int());
    m_expectedDataQueue.enqueue(int());
}

void OpenMVPluginIO::getScriptRunning()
{
    QByteArray buffer;
    serializeByte(buffer, __USBDBG_CMD);
    serializeByte(buffer, __USBDBG_SCRIPT_RUNNING);
    serializeLong(buffer, SCRIPT_RUNNING_RESPONSE_LEN);
    m_commandQueue.enqueue(OpenMVPluginSerialPortData(buffer, SET_START_END_DELAY(SCRIPT_RUNNING_START_DELAY, SCRIPT_RUNNING_END_DELAY)));
    m_expectedHeaderQueue.enqueue(__USBDBG_SCRIPT_RUNNING);
    m_expectedDataQueue.enqueue(SCRIPT_RUNNING_RESPONSE_LEN);
}

void OpenMVPluginIO::templateSave(int x, int y, int w, int h, const QByteArray &path)
{
    QByteArray buffer;
    serializeByte(buffer, __USBDBG_CMD);
    serializeByte(buffer, __USBDBG_TEMPLATE_SAVE);
    serializeLong(buffer, 2 + 2 + 2 + 2 + path.size());
    m_commandQueue.enqueue(OpenMVPluginSerialPortData(buffer, SET_START_END_DELAY(TEMPLATE_SAVE_START_DELAY, TEMPLATE_SAVE_END_DELAY)));
    m_expectedHeaderQueue.enqueue(int());
    m_expectedDataQueue.enqueue(int());
    buffer.clear();
    serializeWord(buffer, x);
    serializeWord(buffer, y);
    serializeWord(buffer, w);
    serializeWord(buffer, h);
    m_commandQueue.enqueue(OpenMVPluginSerialPortData(buffer + path, SET_START_END_DELAY(TEMPLATE_SAVE_2_START_DELAY, TEMPLATE_SAVE_2_END_DELAY)));
    m_expectedHeaderQueue.enqueue(int());
    m_expectedDataQueue.enqueue(int());
}

void OpenMVPluginIO::descriptorSave(int x, int y, int w, int h, const QByteArray &path)
{
    QByteArray buffer;
    serializeByte(buffer, __USBDBG_CMD);
    serializeByte(buffer, __USBDBG_DESCRIPTOR_SAVE);
    serializeLong(buffer, 2 + 2 + 2 + 2 + path.size());
    m_commandQueue.enqueue(OpenMVPluginSerialPortData(buffer, SET_START_END_DELAY(DESCRIPTOR_SAVE_START_DELAY, DESCRIPTOR_SAVE_END_DELAY)));
    m_expectedHeaderQueue.enqueue(int());
    m_expectedDataQueue.enqueue(int());
    buffer.clear();
    serializeWord(buffer, x);
    serializeWord(buffer, y);
    serializeWord(buffer, w);
    serializeWord(buffer, h);
    m_commandQueue.enqueue(OpenMVPluginSerialPortData(buffer + path, SET_START_END_DELAY(DESCRIPTOR_SAVE_2_START_DELAY, DESCRIPTOR_SAVE_2_END_DELAY)));
    m_expectedHeaderQueue.enqueue(int());
    m_expectedDataQueue.enqueue(int());
}

void OpenMVPluginIO::setAttribute(int attribute, int value)
{
    QByteArray buffer;
    serializeByte(buffer, __USBDBG_CMD);
    serializeByte(buffer, __USBDBG_ATTR_WRITE);
    serializeLong(buffer, int());
    serializeWord(buffer, attribute);
    serializeWord(buffer, value);
    m_commandQueue.enqueue(OpenMVPluginSerialPortData(buffer, SET_START_END_DELAY(ATTR_READ_START_DELAY, ATTR_READ_END_DELAY)));
    m_expectedHeaderQueue.enqueue(int());
    m_expectedDataQueue.enqueue(int());
}

void OpenMVPluginIO::getAttribute(int attribute)
{
    QByteArray buffer;
    serializeByte(buffer, __USBDBG_CMD);
    serializeByte(buffer, __USBDBG_ATTR_READ);
    serializeLong(buffer, ATTR_READ_RESPONSE_LEN);
    serializeWord(buffer, attribute);
    m_commandQueue.enqueue(OpenMVPluginSerialPortData(buffer, SET_START_END_DELAY(ATTR_WRITE_START_DELAY, ATTR_WRITE_END_DELAY)));
    m_expectedHeaderQueue.enqueue(__USBDBG_ATTR_READ);
    m_expectedDataQueue.enqueue(ATTR_READ_RESPONSE_LEN);
}

void OpenMVPluginIO::sysReset()
{
    QByteArray buffer;
    serializeByte(buffer, __USBDBG_CMD);
    serializeByte(buffer, __USBDBG_SYS_RESET);
    serializeLong(buffer, int());
    m_commandQueue.enqueue(OpenMVPluginSerialPortData(buffer, SET_START_END_DELAY(SYS_RESET_START_DELAY, SYS_RESET_END_DELAY)));
    m_expectedHeaderQueue.enqueue(int());
    m_expectedDataQueue.enqueue(int());
}

void OpenMVPluginIO::fbEnable(bool enabled)
{
    QByteArray buffer;
    serializeByte(buffer, __USBDBG_CMD);
    serializeByte(buffer, __USBDBG_FB_ENABLE);
    serializeLong(buffer, int());
    serializeWord(buffer, enabled ? true : false);
    m_commandQueue.enqueue(OpenMVPluginSerialPortData(buffer, SET_START_END_DELAY(FB_ENABLE_START_DELAY, FB_ENABLE_END_DELAY)));
    m_expectedHeaderQueue.enqueue(int());
    m_expectedDataQueue.enqueue(int());
}

void OpenMVPluginIO::jpegEnable(bool enabled)
{
    Q_UNUSED(enabled)

//  QByteArray buffer;
//  serializeByte(buffer, __USBDBG_CMD);
//  serializeByte(buffer, __USBDBG_JPEG_ENABLE);
//  serializeLong(buffer, int());
//  serializeWord(buffer, enabled ? true : false);
//  m_commandQueue.enqueue(OpenMVPluginSerialPortData(buffer, SET_START_END_DELAY(JPEG_ENABLE_START_DELAY, JPEG_ENABLE_END_DELAY)));
//  m_expectedHeaderQueue.enqueue(int());
//  m_expectedDataQueue.enqueue(int());
}

void OpenMVPluginIO::getTxBuffer()
{
    QByteArray buffer;
    serializeByte(buffer, __USBDBG_CMD);
    serializeByte(buffer, __USBDBG_TX_BUF_LEN);
    serializeLong(buffer, TX_BUF_LEN_RESPONSE_LEN);
    m_commandQueue.enqueue(OpenMVPluginSerialPortData(buffer, SET_START_END_DELAY(TX_BUF_LEN_START_DELAY, TX_BUF_LEN_END_DELAY)));
    m_expectedHeaderQueue.enqueue(__USBDBG_TX_BUF_LEN);
    m_expectedDataQueue.enqueue(TX_BUF_LEN_RESPONSE_LEN);
}

void OpenMVPluginIO::bootloaderStart()
{
    QByteArray buffer;
    serializeLong(buffer, __BOOTLDR_START);
    m_commandQueue.enqueue(OpenMVPluginSerialPortData(buffer, SET_START_END_DELAY(BOOTLDR_START_START_DELAY, BOOTLDR_START_END_DELAY)));
    m_expectedHeaderQueue.enqueue(__BOOTLDR_START);
    m_expectedDataQueue.enqueue(BOOTLDR_START_RESPONSE_LEN);
}

void OpenMVPluginIO::bootloaderReset()
{
    QByteArray buffer;
    serializeLong(buffer, __BOOTLDR_RESET);
    m_commandQueue.enqueue(OpenMVPluginSerialPortData(buffer, SET_START_END_DELAY(BOOTLDR_RESET_START_DELAY, BOOTLDR_RESET_END_DELAY)));
    m_expectedHeaderQueue.enqueue(int());
    m_expectedDataQueue.enqueue(int());
}

void OpenMVPluginIO::flashErase(int sector)
{
    QByteArray buffer;
    serializeLong(buffer, __BOOTLDR_ERASE);
    serializeLong(buffer, sector);
    m_commandQueue.enqueue(OpenMVPluginSerialPortData(buffer, SET_START_END_DELAY(BOOTLDR_ERASE_START_DELAY, BOOTLDR_ERASE_END_DELAY)));
    m_expectedHeaderQueue.enqueue(int());
    m_expectedDataQueue.enqueue(int());
}

void OpenMVPluginIO::flashWrite(const QByteArray &data)
{
    QByteArray buffer;
    serializeLong(buffer, __BOOTLDR_WRITE);
    m_commandQueue.enqueue(OpenMVPluginSerialPortData(buffer + data, SET_START_END_DELAY(BOOTLDR_WRITE_START_DELAY, BOOTLDR_WRITE_END_DELAY)));
    m_expectedHeaderQueue.enqueue(int());
    m_expectedDataQueue.enqueue(int());
}

void OpenMVPluginIO::close()
{
    m_commandQueue.enqueue(OpenMVPluginSerialPortData(QByteArray(), int()));
    m_expectedHeaderQueue.enqueue(int());
    m_expectedDataQueue.enqueue(int());
}
