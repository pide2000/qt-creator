#ifndef OPENMVPLUGIN_H
#define OPENMVPLUGIN_H

#include <QtCore>
#include <QtGui>
#include <QtWidgets>
#include <QtSerialPort>

#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/coreconstants.h>
#include <coreplugin/fancyactionbar.h>
#include <coreplugin/fancytabwidget.h>
#include <coreplugin/icore.h>
#include <coreplugin/id.h>
#include <coreplugin/messagemanager.h>
#include <coreplugin/minisplitter.h>
#include <texteditor/texteditor.h>
#include <extensionsystem/iplugin.h>
#include <extensionsystem/pluginmanager.h>
#include <utils/environment.h>
#include <utils/hostosinfo.h>
#include <utils/styledbar.h>
#include <utils/synchronousprocess.h>

#include "openmvpluginserialport.h"
#include "openmvpluginio.h"
#include "openmvpluginfb.h"
#include "histogram/openmvpluginhistogram.h"

#define ICON_PATH ":/openmv/openmv-media/icons/openmv-icon/openmv.png"
#define SPLASH_PATH ":/openmv/openmv-media/splash/openmv-splash-slate/splash-small.png"
#define START_PATH ":/openmv/projectexplorer/images/debugger_start.png"
#define STOP_PATH ":/openmv/debugger/images/debugger_stop_32.png"
#define CONNECT_PATH ":/openmv/images/connect.png"
#define DISCONNECT_PATH ":/openmv/images/disconnect.png"

#define SETTINGS_GROUP "OpenMV"
#define EDITOR_MANAGER_STATE "EditorManagerState"
#define HSPLITTER_STATE "HSplitterState"
#define VSPLITTER_STATE "VSplitterState"
#define ZOOM_STATE "ZoomState"
#define JPG_COMPRESS_STATE "JPGCompressState"
#define DISABLE_FRAME_BUFFER_STATE "DisableFrameBufferState"
#define HISTOGRAM_COLOR_SPACE_STATE "HistogramColorSpace"
#define LAST_SERIAL_PORT_STATE "LastSerialPortState"
#define LAST_SAVE_IMAGE_PATH "LastSaveImagePath"
#define LAST_SAVE_TEMPLATE_PATH "LastSaveTemplatePath"
#define LAST_SAVE_DESCIPTOR_PATH "LastSaveDescriptorPath"

#define SERIAL_PORT_SETTINGS_GROUP "OpenMVSerialPort"

#define OPENMVCAM_VENDOR_ID 0x1209
#define OPENMVCAM_PRODUCT_ID 0xABD1

namespace OpenMV {
namespace Internal {

class OpenMVPlugin : public ExtensionSystem::IPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QtCreatorPlugin" FILE "OpenMV.json")

public:

    OpenMVPlugin();
    ~OpenMVPlugin();

    bool initialize(const QStringList &arguments, QString *errorMessage);
    void extensionsInitialized();

public slots:

    void connectClicked(); // 1
    void connectClickedResult(const QString &errorMessage); // 2
    void firmwareVersion(long major, long minor, long patch); // 3
    void disconnectClicked(); // 4
    void shutdown(const QString &errorMessage); // 5
    void closeResponse(); // 6

    void startClicked(); // 1
    void startFinished(); // 2

    void processEvents();
    void errorFilter(const QByteArray &data);

    void saveImage(const QPixmap &data);
    void saveTemplate(const QRect &rect);
    void saveDescriptor(const QRect &rect);

private:

    QList<QAction *> aboutToShowExamplesRecursive(const QString &path, QMenu *parent, QObject *object);

    QString getSerialPortPath() const;
    void setSerialPortPath();

    Core::ActionContainer *m_examplesMenu;

    Core::Command *m_connectCommand;
    Core::Command *m_disconnectCommand;
    Core::Command *m_startCommand;
    Core::Command *m_stopCommand;

    Core::Command *m_docsCommand;
    Core::Command *m_forumsCommand;
    Core::Command *m_pinoutCommand;
    Core::Command *m_aboutCommand;

    Core::MiniSplitter *m_hsplitter;
    Core::MiniSplitter *m_vsplitter;

    QToolButton *m_zoom;
    QToolButton *m_jpgCompress;
    QToolButton *m_disableFrameBuffer;
    OpenMVPluginFB *m_frameBuffer;

    QComboBox *m_histogramColorSpace;
    OpenMVPluginHistogram *m_histogram;

    QLabel *m_portLabel;
    QToolButton *m_pathButton;
    QLabel *m_versionLabel;
    QLabel *m_fpsLabel;

    OpenMVPluginSerialPort *m_ioport;
    OpenMVPluginIO *m_iodevice;

    bool m_operating;
    bool m_connected;
    QString m_portName;
    int m_major;
    int m_minor;
    int m_patch;
    QElapsedTimer m_timer;
    qint64 m_timerLast;
    QRegularExpression m_errorFilterRegex;
    QString m_errorFilterString;
};

} // namespace Internal
} // namespace OpenMV

#endif // OPENMVPLUGIN_H
