#ifndef OPENMVPLUGIN_H
#define OPENMVPLUGIN_H

#include <QtCore>
#include <QtGui>
#include <QtWidgets>
#include <QtSerialPort>

#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/coreconstants.h>
#include <coreplugin/fancyactionbar.h>
#include <coreplugin/fancytabwidget.h>
#include <coreplugin/icore.h>
#include <coreplugin/id.h>
#include <coreplugin/minisplitter.h>

#include <texteditor/texteditor.h>

#include <extensionsystem/iplugin.h>
#include <extensionsystem/pluginmanager.h>

#include <utils/environment.h>
#include <utils/hostosinfo.h>
#include <utils/styledbar.h>

#include "openmvpluginio.h"
#include "openmvframebuffer.h"

#define ICON_PATH ":/openmv/openmv-media/icons/openmv-icon/openmv.png"
#define SPLASH_PATH ":/openmv/openmv-media/splash/openmv-splash/splash-small.png"
#define START_PATH ":/openmv/projectexplorer/images/debugger_start.png"
#define STOP_PATH ":/openmv/debugger/images/debugger_stop_32.png"
#define CONNECT_PATH ":/openmv/images/connect.png"
#define DISCONNECT_PATH ":/openmv/images/disconnect.png"

#define SETTINGS_GROUP "OpenMV"
#define EDITOR_MANAGER_STATE "EditorManagerState"
#define HSPLITTER_STATE "HSplitterState"
#define VSPLITTER_STATE "VSplitterState"
#define JPG_COMPRESS_STATE "JPGCompressState"
#define DISABLE_FRAME_BUFFER_STATE "DisableFrameBufferState"
#define HISTOGRAM_CHANNEL_STATE "HistogramChannelState"
#define LAST_SERIAL_PORT_STATE "LastSerialPortState"

#define OPENMVCAM_VENDOR_ID 0x1209
#define OPENMVCAM_PRODUCT_ID 0xABD1

#define OPENMVCAM_BAUD_RATE 12000000 // 12 Mbps
#define OPENMVCAM_POLL_RATE 10 // 10 ms = 100 Hz

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

    void saveSettingsRequested();

    void aboutToShowExamples();

    void docsClicked();
    void forumsClicked();
    void pinoutClicked();
    void aboutClicked();

    void connectClicked();
    void disconnectClicked();
    void startClicked();
    void stopClicked();

    void processEvents();

private:

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

    QToolButton *m_jpgCompress;
    QToolButton *m_disableFrameBuffer;
    OpenMVFrameBuffer *m_frameBuffer;

    QComboBox *m_histogramChannel;
    QGraphicsView *m_histogram;

    OpenMVPluginIO *m_iodevice;
    QTimer *m_timer;
    QSerialPort *m_serialPort;
};

} // namespace Internal
} // namespace OpenMV

#endif // OPENMVPLUGIN_H
