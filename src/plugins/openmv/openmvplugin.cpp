#include "openmvplugin.h"

namespace OpenMV {
namespace Internal {

OpenMVPlugin::OpenMVPlugin()
{

}

OpenMVPlugin::~OpenMVPlugin()
{

}

bool OpenMVPlugin::initialize(const QStringList &arguments, QString *errorMessage)
{
    Q_UNUSED(arguments)
    Q_UNUSED(errorMessage)

    QSplashScreen *splashscreen = new QSplashScreen(QPixmap(QStringLiteral(SPLASH_PATH)));
    connect(Core::ICore::instance(), &Core::ICore::coreOpened, splashscreen, &QSplashScreen::close);
    splashscreen->show();

    return true;
}

void OpenMVPlugin::extensionsInitialized()
{

}

} // namespace Internal
} // namespace OpenMV
