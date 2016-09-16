#ifndef OPENMVPLUGINIO_H
#define OPENMVPLUGINIO_H

#include <QtCore>
#include <QtGui>

#include "openmvpluginserialport.h"

#define USBDBG_COMMAND_TIMEOUT  200 // in ms
#define USBDBG_COMMAND_RETRY    5

#define ATTR_CONTRAST       0
#define ATTR_BRIGHTNESS     1
#define ATTR_SATURATION     2
#define ATTR_GAINCEILING    3

#define TEMPLATE_SAVE_PATH_MAX_LEN      55
#define DESCRIPTOR_SAVE_PATH_MAX_LEN    55

#define FLASH_SECTOR_START      4
#define FLASH_SECTOR_END        11
#define FLASH_SECTOR_ALL_START  1
#define FLASH_SECTOR_ALL_END    11

#define FLASH_WRITE_CHUNK_SIZE  60

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
    void templateSave(int x, int y, int w, int h, const QByteArray &path);
    void descriptorSave(int x, int y, int w, int h, const QByteArray &path);
    void setAttribute(int attribute, int value);
    void getAttribute(int attribute);
    void sysReset();
    void fbEnable(bool enable);
    void jpegEnable(bool enabled);
    void getTxBuffer();
    void bootloaderStart();
    void bootloaderReset();
    void flashErase(int sector);
    void flashWrite(const QByteArray &data);
    void close();

public slots: // private

    void processEvents();
    void readAll(const QByteArray &data);
    void timeout();

signals:

    void firmwareVersion(int major, int minor, int patch);
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
    int m_processingResponse;
    int m_retryCounter;
    QQueue<QByteArray> m_commandQueue;
    QQueue<int> m_expectedHeaderQueue;
    QQueue<int> m_expectedDataQueue;
    QByteArray m_receivedBytes;
    int m_frameSizeW;
    int m_frameSizeH;
    int m_frameSizeBPP;
    QByteArray m_lineBuffer;
};

#endif // OPENMVPLUGINIO_H
