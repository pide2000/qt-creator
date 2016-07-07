#ifndef OPENMVPLUGINIO_H
#define OPENMVPLUGINIO_H

#include <QtCore>
#include <QtGui>

#include "openmvpluginserialport.h"

#define ATTR_CONTRAST       0
#define ATTR_BRIGHTNESS     1
#define ATTR_SATURATION     2
#define ATTR_GAINCEILING    3

class OpenMVPluginIO : public QObject
{
    Q_OBJECT

public:

    explicit OpenMVPluginIO(OpenMVPluginSerialPort *port, QObject *parent = Q_NULLPTR);
    void reset();

    // To shutdown this object close() must be called which will prevent any new
    // commands from being executed (that are not already in the serial buffer
    // queues). The close command will then be sent to the serial thread and
    // will clear out the write and then read signal queues. Lastly, close
    // will return as the close reponse signal after which you can reset
    // (above) the object.

    bool frameSizeDumpQueued() const;
    bool getScriptRunningQueued() const;
    bool getAttributeQueued() const;
    bool getTxBufferQueued() const;

    bool commandsInQueue() const { return !m_commandQueue.isEmpty(); }

public slots:

    void getFirmwareVersion();
    void frameSizeDump();
    void frameUpdate();
    void scriptExec(const QByteArray &data);
    void scriptStop();
    void getScriptRunning();
    void templateSave(long x, long y, long w, long h, const QByteArray &path);
    void descriptorSave(short x, short y, short w, short h, const QByteArray &path);
    void setAttribute(short attribute, short value);
    void getAttribute(short attribute);
    void sysReset();
    void jpegEnable(bool enable);
    void getTxBuffer();
    void bootloaderStart();
    void bootloaderReset();
    void flashErase(long sector);
    void flashWrite(const QByteArray &data);
    void close();

public slots: // private

    void processEvents();
    void readAll(const QByteArray &data);

signals:

    void firmwareVersion(long major, long minor, long patch);
    void frameBufferData(const QPixmap &data);
    void scriptRunning(long);
    void attribute(char);
    void printData(const QByteArray &data);
    void gotBootloaderStart(bool);
    void closeResponse();

private:

    OpenMVPluginSerialPort *m_port;

    QQueue<QByteArray> m_commandQueue;
    QQueue<int> m_expectedHeaderQueue, m_expectedDataQueue;
    int m_expectedNumber;
    bool m_shutdown, m_closed;
    QByteArray m_receivedBytes;
    long m_frameSizeW, m_frameSizeH, m_frameSizeBPP;
};

#endif // OPENMVPLUGINIO_H
