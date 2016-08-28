#ifndef OPENMVPLUGINIO_H
#define OPENMVPLUGINIO_H

#include <QtCore>
#include <QtGui>

#include "openmvpluginserialport.h"

#define USBDBG_COMMAND_SPACING  1 // in ms
#define USBDBG_COMMAND_TIMEOUT  100 // in ms

#define ATTR_CONTRAST       0
#define ATTR_BRIGHTNESS     1
#define ATTR_SATURATION     2
#define ATTR_GAINCEILING    3

#define FLASH_SECTOR_START      4
#define FLASH_SECTOR_END        11
#define FLASH_SECTOR_ALL_START  1
#define FLASH_SECTOR_ALL_END    11

class OpenMVPluginIO : public QObject
{
    Q_OBJECT

public:

    explicit OpenMVPluginIO(OpenMVPluginSerialPort *port, QObject *parent = Q_NULLPTR);

    bool frameSizeDumpQueued() const;
    bool getScriptRunningQueued() const;
    bool getAttributeQueued() const;
    bool getTxBufferQueued() const;

public slots:

    void getFirmwareVersion();
    void frameSizeDump();
    void getArchString();
    void scriptExec(const QByteArray &data);
    void scriptStop();
    void getScriptRunning();
    void templateSave(long x, long y, long w, long h, const QByteArray &path);
    void descriptorSave(short x, short y, short w, short h, const QByteArray &path);
    void setAttribute(short attribute, short value);
    void getAttribute(short attribute);
    void sysReset();
    void fbEnable(bool enable);
    void jpegEnable(bool enabled);
    void getTxBuffer();
    void bootloaderStart();
    void bootloaderReset();
    void flashErase(long sector);
    void flashWrite(const QByteArray &data);
    void close();

public slots: // private

    void processEvents();
    void readAll(const QByteArray &data);
    void timeout();

signals:

    void firmwareVersion(long major, long minor, long patch);
    void archString(const QString &arch);
    void frameBufferData(const QPixmap &data);
    void scriptRunning(long);
    void attribute(char);
    void printData(const QByteArray &data);
    void gotBootloaderStart(bool);
    void closeResponse();

private:

    OpenMVPluginSerialPort *m_port;
    QTimer *m_timer;
    QElapsedTimer m_spacer;
    int m_processingResponse;
    QQueue<QByteArray> m_commandQueue;
    QQueue<int> m_expectedHeaderQueue;
    QQueue<int> m_expectedDataQueue;
    QByteArray m_receivedBytes;
    long m_frameSizeW;
    long m_frameSizeH;
    long m_frameSizeBPP;
};

#endif // OPENMVPLUGINIO_H
