#ifndef OPENMVPLUGIN_H
#define OPENMVPLUGIN_H

#include <QtCore>
#include <QtGui>
#include <QtWidgets>

#include <extensionsystem/iplugin.h>

#include <coreplugin/actionmanager/actionmanager.h>
#include <coreplugin/actionmanager/actioncontainer.h>
#include <coreplugin/coreconstants.h>
#include <coreplugin/icore.h>

#define SPLASH_PATH ":/openmv-media/splash/openmv-splash/splash-small.png"
#define PINOUT_PATH ":/openmv-media/graphics/pinout/pinout.png"

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
};

} // namespace Internal
} // namespace OpenMV

#endif // OPENMVPLUGIN_H
