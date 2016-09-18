#ifndef OPENMVPLUGIN_H
#define OPENMVPLUGIN_H

#include <QtCore>
#include <QtGui>
#include <QtNetwork>
#include <QtSerialPort>
#include <QtWidgets>

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
#include <utils/pathchooser.h>
#include <utils/hostosinfo.h>
#include <utils/styledbar.h>
#include <utils/synchronousprocess.h>

#include "openmvpluginserialport.h"
#include "openmvpluginio.h"
#include "openmvpluginfb.h"
#include "histogram/openmvpluginhistogram.h"

#define ICON_PATH ":/openmv/openmv-media/icons/openmv-icon/openmv.png"
#define SPLASH_PATH ":/openmv/openmv-media/splash/openmv-splash-slate/splash-small.png"
#define CONNECT_PATH ":/openmv/images/connect.png"
#define DISCONNECT_PATH ":/openmv/images/disconnect.png"
#define START_PATH ":/openmv/projectexplorer/images/run.png"
#define STOP_PATH ":/openmv/images/application-exit.png"

#define SETTINGS_GROUP "OpenMV"
#define EDITOR_MANAGER_STATE "EditorManagerState"
#define HSPLITTER_STATE "HSplitterState"
#define VSPLITTER_STATE "VSplitterState"
#define ZOOM_STATE "ZoomState"
#define JPG_COMPRESS_STATE "JPGCompressState"
#define DISABLE_FRAME_BUFFER_STATE "DisableFrameBufferState"
#define HISTOGRAM_COLOR_SPACE_STATE "HistogramColorSpace"
#define LAST_FIRMWARE_PATH "LastFirmwarePath"
#define LAST_FLASH_FS_ERASE_STATE "LastFlashFSEraseState"
#define LAST_BOARD_TYPE_STATE "LastBoardTypeState"
#define LAST_SERIAL_PORT_STATE "LastSerialPortState"
#define LAST_SAVE_IMAGE_PATH "LastSaveImagePath"
#define LAST_SAVE_TEMPLATE_PATH "LastSaveTemplatePath"
#define LAST_SAVE_DESCIPTOR_PATH "LastSaveDescriptorPath"

#define SERIAL_PORT_SETTINGS_GROUP "OpenMVSerialPort"

#define OPENMVCAM_VENDOR_ID 0x1209
#define OPENMVCAM_PRODUCT_ID 0xABD1

#define FRAME_SIZE_DUMP_SPACING     10 // in ms
#define GET_SCRIPT_RUNNING_SPACING  100 // in ms
#define GET_TX_BUFFER_SPACING       10 // in ms

#define FPS_AVERAGE_BUFFER_DEPTH    10 // in samples
#define ERROR_FILTER_MAX_SIZE       1000 // in chars

#define OLD_API_MAJOR 1
#define OLD_API_MINOR 7
#define OLD_API_PATCH 0

#define OLD_API_BOARD "OMV2"

namespace OpenMV {
namespace Internal {

class OpenMVPlugin : public ExtensionSystem::IPlugin
{
    Q_OBJECT
    Q_PLUGIN_METADATA(IID "org.qt-project.Qt.QtCreatorPlugin" FILE "OpenMV.json")

public:

    explicit OpenMVPlugin();
    bool initialize(const QStringList &arguments, QString *errorMessage);
    void extensionsInitialized();
    ExtensionSystem::IPlugin::ShutdownFlag aboutToShutdown();

public slots: // private

    void bootloaderClicked();
    void connectClicked(bool forceBootloader = false, QString forceFirmwarePath = QString(), int forceFlashFSErase = int());
    void disconnectClicked(bool reset = false);
    void startClicked();
    void stopClicked();
    void processEvents();
    void errorFilter(const QByteArray &data);

    void saveScript();
    void saveImage(const QPixmap &data);
    void saveTemplate(const QRect &rect);
    void saveDescriptor(const QRect &rect);
    void setPortPath();

signals:

    void workingDone();
    void disconnectDone();

private:

    QMap<QString, QAction *> aboutToShowExamplesRecursive(const QString &path, QMenu *parent);

    Core::Command *m_bootloaderCommand;
    Core::Command *m_saveCommand;
    Core::Command *m_resetCommand;
    Core::Command *m_docsCommand;
    Core::Command *m_forumsCommand;
    Core::Command *m_pinoutCommand;
    Core::Command *m_aboutCommand;

    Core::Command *m_connectCommand;
    Core::Command *m_disconnectCommand;
    Core::Command *m_startCommand;
    Core::Command *m_stopCommand;

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

    QElapsedTimer m_frameSizeDumpTimer;
    QElapsedTimer m_getScriptRunningTimer;
    QElapsedTimer m_getTxBufferTimer;

    QElapsedTimer m_timer;
    QQueue<qint64> m_queue;

    bool m_working;
    bool m_connected;
    bool m_running;
    QString m_portName;
    QString m_portPath;

    QRegularExpression m_errorFilterRegex;
    QString m_errorFilterString;
};

} // namespace Internal
} // namespace OpenMV

#endif // OPENMVPLUGIN_H
