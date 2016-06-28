#ifndef OPENMVPLUGINIO_H
#define OPENMVPLUGINIO_H

#include <QtCore>
#include <QtGui>
#include <QSerialPort>

#define ATTR_CONTRAST       0
#define ATTR_BRIGHTNESS     1
#define ATTR_SATURATION     2
#define ATTR_GAINCEILING    3

class OpenMVPluginIO : public QObject
{
    Q_OBJECT

public:

    explicit OpenMVPluginIO(QObject *parent = nullptr);
    QSerialPort *serialPort() const;
    bool getDisablePrint() const;
    bool getDisableFrameBuffer() const;
    bool getBootloaderMode() const;

public slots:

    void setSerialPort(QSerialPort *port);
    void processEvents();

    void getFirmwareVersion();
    void frameUpdate();
    void scriptExec(const QByteArray &data);
    void scriptStop();
    void getScriptRunning();
    void templateSave(long x, long y, long w, long h, const QByteArray &path);
    void descriptorSave(short x, short y, short w, short h, const QByteArray &path);
    void set_attr(short attribute, short value);
    void getAttribute(short attribute);
    void sysReset();
    void jpegEnable(bool enable);
    void disablePrint(bool enable);
    void disableFrameBuffer(bool enable);

    void bootloaderMode(bool enable);
    void bootloaderStart();
    void bootloaderReset();
    void flashErase(long sector);
    void flashWrite(const QByteArray &data);

signals:

    void serialPortChanged(QSerialPort *oldPort, QSerialPort *newPort);

    void printData(const QByteArray &data);
    void frameBufferData(const QPixmap &data);

    void firmwareVersion(long, long, long);
    void scriptRunning(long);
    void attribute(char);

    void gotBootloaderStart(bool);

private:

    QSerialPort *m_port;
    QByteArray m_receivedBytes;

    QQueue<QByteArray> m_commandQueue;
    QQueue<int> m_expectedHeaderQueue, m_expectedDataQueue;

    bool m_disablePrint, m_disableFrameBuffer, m_bootloaderMode;
    long m_frameSizeW, m_frameSizeH, m_frameSizeBPP; // temp vars...
};

#endif // OPENMVPLUGINIO_H
