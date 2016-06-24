#ifndef OPENMVPLUGIN_H
#define OPENMVPLUGIN_H

#include <QtCore>
#include <QtGui>
#include <QtWidgets>

#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/editormanager/editormanager.h>
#include <coreplugin/coreconstants.h>
#include <coreplugin/fancyactionbar.h>
#include <coreplugin/fancytabwidget.h>
#include <coreplugin/icore.h>
#include <coreplugin/id.h>

#include <extensionsystem/iplugin.h>
#include <extensionsystem/pluginmanager.h>

#include <utils/hostosinfo.h>

#define ICON_PATH ":/openmv-media/icons/openmv-icon/openmv.png"
#define SPLASH_PATH ":/openmv-media/splash/openmv-splash/splash-small.png"
#define START_PATH ":/projectexplorer/images/debugger_start.png"
#define STOP_PATH ":/debugger/images/debugger_stop_32.png"

#define SETTINGS_GROUP "OpenMV"
#define EDITOR_MANAGER_STATE "EditorManagerState"

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

    void docsClicked();
    void forumsClicked();
    void pinoutClicked();
    void aboutClicked();

private:

    Core::Command *m_startCommand;
    Core::Command *m_stopCommand;

    Core::Command *m_docsCommand;
    Core::Command *m_forumsCommand;
    Core::Command *m_pinoutCommand;
    Core::Command *m_aboutCommand;
};

} // namespace Internal
} // namespace OpenMV

#endif // OPENMVPLUGIN_H
