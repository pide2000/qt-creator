#include "openmvplugin.h"

#include "app/app_version.h"

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

    QApplication::setWindowIcon(QIcon(QStringLiteral(ICON_PATH)));

    QSplashScreen *splashScreen = new QSplashScreen(QPixmap(QStringLiteral(SPLASH_PATH)));
    connect(Core::ICore::instance(), &Core::ICore::coreOpened, splashScreen, &QSplashScreen::close);
    splashScreen->show();

    return true;
}

void OpenMVPlugin::extensionsInitialized()
{
    m_startCommand =
        Core::ActionManager::registerAction(new QAction(QIcon(QStringLiteral(START_PATH)),
        tr("Start"), this), Core::Id("OpenMV.Start"));
    m_startCommand->setDefaultKeySequence(tr("Ctrl+R"));

    m_stopCommand =
        Core::ActionManager::registerAction(new QAction(QIcon(QStringLiteral(STOP_PATH)),
        tr("Stop"), this), Core::Id("OpenMV.Stop"));

    ///////////////////////////////////////////////////////////////////////////

    QMainWindow *mainWindow =
        qobject_cast<QMainWindow *>(Core::ICore::mainWindow());

    if(mainWindow)
    {
        Core::Internal::FancyTabWidget *widget =
            qobject_cast<Core::Internal::FancyTabWidget *>(mainWindow->centralWidget());

        if(widget)
        {
            Core::Internal::FancyActionBar *actionBar0 =
                new Core::Internal::FancyActionBar(widget);

            widget->insertCornerWidget(0, actionBar0);

            actionBar0->insertAction(0,
                Core::ActionManager::command(Core::Constants::NEW)->action());

            actionBar0->insertAction(1,
                Core::ActionManager::command(Core::Constants::OPEN)->action());

            actionBar0->insertAction(2,
                Core::ActionManager::command(Core::Constants::SAVE)->action());

            actionBar0->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

            ///////////////////////////////////////////////////////////////////

            Core::Internal::FancyActionBar *actionBar1 =
                new Core::Internal::FancyActionBar(widget);

            widget->insertCornerWidget(1, actionBar1);

            actionBar1->insertAction(0,
                Core::ActionManager::command(Core::Constants::UNDO)->action());

            actionBar1->insertAction(1,
                Core::ActionManager::command(Core::Constants::REDO)->action());

            actionBar1->insertAction(2,
                Core::ActionManager::command(Core::Constants::CUT)->action());

            actionBar1->insertAction(3,
                Core::ActionManager::command(Core::Constants::COPY)->action());

            actionBar1->insertAction(4,
                Core::ActionManager::command(Core::Constants::PASTE)->action());

            actionBar1->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

            ///////////////////////////////////////////////////////////////////

            Core::Internal::FancyActionBar *actionBar2 =
                new Core::Internal::FancyActionBar(widget);

            widget->insertCornerWidget(2, actionBar2);

            actionBar2->insertAction(0, m_startCommand->action());
            actionBar2->insertAction(1, m_stopCommand->action());

            actionBar2->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
        }
    }

    ///////////////////////////////////////////////////////////////////////////

    Core::ActionContainer *helpMenu = Core::ActionManager::actionContainer(Core::Constants::M_HELP);
    QIcon helpIcon = QIcon::fromTheme(QStringLiteral("help-about"));

    QAction *docsCommand = new QAction(
         tr("OpenMV &Docs"), this);
    m_docsCommand = Core::ActionManager::registerAction(docsCommand, Core::Id("OpenMV.Docs"));
    helpMenu->addAction(m_docsCommand, Core::Constants::G_HELP_SUPPORT);
    docsCommand->setEnabled(true);
    connect(docsCommand, &QAction::triggered, this, &OpenMVPlugin::docsClicked);

    QAction *forumsCommand = new QAction(
         tr("OpenMV &Forums"), this);
    m_forumsCommand = Core::ActionManager::registerAction(forumsCommand, Core::Id("OpenMV.Forums"));
    helpMenu->addAction(m_forumsCommand, Core::Constants::G_HELP_SUPPORT);
    forumsCommand->setEnabled(true);
    connect(forumsCommand, &QAction::triggered, this, &OpenMVPlugin::forumsClicked);

    QAction *pinoutAction = new QAction(
         tr("About &OpenMV Cam..."), this);
    pinoutAction->setMenuRole(QAction::ApplicationSpecificRole);
    m_pinoutCommand = Core::ActionManager::registerAction(pinoutAction, Core::Id("OpenMV.Pinout"));
    helpMenu->addAction(m_pinoutCommand, Core::Constants::G_HELP_ABOUT);
    pinoutAction->setEnabled(true);
    connect(pinoutAction, &QAction::triggered, this, &OpenMVPlugin::pinoutClicked);

    QAction *aboutAction = new QAction(helpIcon,
        Utils::HostOsInfo::isMacHost() ? tr("About &OpenMV IDE") : tr("About &OpenMV IDE..."), this);
    aboutAction->setMenuRole(QAction::AboutRole);
    m_aboutCommand = Core::ActionManager::registerAction(aboutAction, Core::Id("OpenMV.About"));
    helpMenu->addAction(m_aboutCommand, Core::Constants::G_HELP_ABOUT);
    aboutAction->setEnabled(true);
    connect(aboutAction, &QAction::triggered, this, &OpenMVPlugin::aboutClicked);

    ///////////////////////////////////////////////////////////////////////////

    QSettings *settings = ExtensionSystem::PluginManager::settings();
    settings->beginGroup(QStringLiteral(SETTINGS_GROUP));
    Core::EditorManager::instance()->restoreState(
        settings->value(QStringLiteral(EDITOR_MANAGER_STATE)).toByteArray());
    settings->endGroup();

    connect(Core::ICore::instance(), &Core::ICore::saveSettingsRequested,
        this, &OpenMVPlugin::saveSettingsRequested);
}

void OpenMVPlugin::saveSettingsRequested()
{
    QSettings *settings = ExtensionSystem::PluginManager::settings();
    settings->beginGroup(QStringLiteral(SETTINGS_GROUP));
    settings->setValue(QStringLiteral(EDITOR_MANAGER_STATE),
        Core::EditorManager::instance()->saveState());
    settings->endGroup();
}

void OpenMVPlugin::docsClicked()
{
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://openmv.io/docs/")));
}

void OpenMVPlugin::forumsClicked()
{
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://openmv.io/forums/")));
}

void OpenMVPlugin::pinoutClicked()
{
    QDesktopServices::openUrl(QUrl(QStringLiteral("https://openmv.io/docs/_images/pinout.png")));
}

void OpenMVPlugin::aboutClicked()
{
    QMessageBox::about(Core::ICore::mainWindow(), tr("About OpenMV IDE"), tr(
    "<p><b>About OpenMV IDE %L1</b></p>"
    "<p>By: Ibrahim Abdelkader & Kwabena W. Agyeman</p>"
    "<p><b>GNU GENERAL PUBLIC LICENSE</b></p>"
    "<p>Copyright (C) %L2 %L3</p>"
    "<p>This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the <a href=\"http://github.com/openmv/qt-creator/raw/master/LICENSE.GPL3-EXCEPT\">GNU General Public License</a> for more details.</p>"
    "<p><b>Questions or Comments?</b></p>"
    "<p>Contact us at <a href=\"mailto:openmv@openmv.io\">openmv@openmv.io</a>.</p>"
    ).arg(QLatin1String(Core::Constants::OMV_IDE_VERSION_LONG)).arg(QLatin1String(Core::Constants::OMV_IDE_YEAR)).arg(QLatin1String(Core::Constants::OMV_IDE_AUTHOR)));
}

} // namespace Internal
} // namespace OpenMV
