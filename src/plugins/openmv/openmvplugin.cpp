#include "openmvplugin.h"

#include "app/app_version.h"

namespace OpenMV {
namespace Internal {

OpenMVPlugin::OpenMVPlugin()
{
    m_iodevice = new OpenMVPluginIO(this);
    m_timer = new QTimer(this);
    m_serialPort = NULL;

    connect(m_timer, &QTimer::timeout, m_iodevice, &OpenMVPluginIO::processEvents);
    m_timer->start(OPENMVCAM_POLL_RATE);
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
    connect(Core::ActionManager::command(Core::Constants::NEW)->action(), &QAction::triggered, this, [] {
        QString titlePattern = tr("Untitled $.py");
        Core::IEditor *editor = Core::EditorManager::openEditorWithContents(Core::Constants::K_DEFAULT_TEXT_EDITOR_ID, &titlePattern,
        tr("# Untitled - By: %L1 - %L2\n"
           "\n"
           "import sensor\n"
           "\n"
           "sensor.reset()\n"
           "sensor.set_pixformat(sensor.RGB565)\n"
           "sensor.set_framesize(sensor.QVGA)\n"
           "sensor.skip_frames()\n"
           "\n"
           "while(True):\n"
           "    img = sensor.snapshot()\n").arg(Utils::Environment::systemEnvironment().userName()).arg(QDate::currentDate().toString()).toLatin1());
        qobject_cast<TextEditor::BaseTextEditor *>(editor)->editorWidget()->configureGenericHighlighter();
    });

    ///////////////////////////////////////////////////////////////////////////

    Core::ActionContainer *filesMenu = Core::ActionManager::actionContainer(Core::Constants::M_FILE);
    m_examplesMenu = Core::ActionManager::createMenu(Core::Id("OpenMV.Examples"));
    filesMenu->addMenu(Core::ActionManager::actionContainer(Core::Constants::M_FILE_RECENTFILES), m_examplesMenu, Core::Constants::G_FILE_OPEN);
    m_examplesMenu->menu()->setTitle(tr("&Examples"));
    m_examplesMenu->setOnAllDisabledBehavior(Core::ActionContainer::Show);
    connect(filesMenu->menu(), &QMenu::aboutToShow, this, &OpenMVPlugin::aboutToShowExamples);

    ///////////////////////////////////////////////////////////////////////////

    m_connectCommand =
        Core::ActionManager::registerAction(new QAction(QIcon(QStringLiteral(CONNECT_PATH)),
        tr("Connect"), this), Core::Id("OpenMV.Connect"));
    connect(m_connectCommand->action(), &QAction::triggered, this, &OpenMVPlugin::connectClicked);

    m_disconnectCommand =
        Core::ActionManager::registerAction(new QAction(QIcon(QStringLiteral(DISCONNECT_PATH)),
        tr("Disconnect"), this), Core::Id("OpenMV.Disconnect"));
    m_disconnectCommand->action()->setVisible(false);
    connect(m_disconnectCommand->action(), &QAction::triggered, this, &OpenMVPlugin::disconnectClicked);

    m_startCommand =
        Core::ActionManager::registerAction(new QAction(QIcon(QStringLiteral(START_PATH)),
        tr("Start"), this), Core::Id("OpenMV.Start"));
    m_startCommand->setDefaultKeySequence(tr("Ctrl+R"));
    m_startCommand->action()->setDisabled(true);
    connect(m_startCommand->action(), &QAction::triggered, this, &OpenMVPlugin::startClicked);

    m_stopCommand =
        Core::ActionManager::registerAction(new QAction(QIcon(QStringLiteral(STOP_PATH)),
        tr("Stop"), this), Core::Id("OpenMV.Stop"));
    m_stopCommand->action()->setDisabled(true);
    connect(m_stopCommand->action(), &QAction::triggered, this, &OpenMVPlugin::stopClicked);

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

            actionBar0->setProperty("no_separator", true);
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

            actionBar1->setProperty("no_separator", false);
            actionBar1->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

            ///////////////////////////////////////////////////////////////////

            Core::Internal::FancyActionBar *actionBar2 =
                new Core::Internal::FancyActionBar(widget);

            widget->insertCornerWidget(2, actionBar2);

            actionBar2->insertAction(0, m_connectCommand->action());
            actionBar2->insertAction(1, m_disconnectCommand->action());
            actionBar2->insertAction(2, m_startCommand->action());
            actionBar2->insertAction(3, m_stopCommand->action());

            actionBar2->setProperty("no_separator", false);
            actionBar2->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

            ///////////////////////////////////////////////////////////////////

            Utils::StyledBar *styledBar0 = new Utils::StyledBar;
            QHBoxLayout *styledBar0Layout = new QHBoxLayout;
            styledBar0Layout->setMargin(0);
            styledBar0Layout->setSpacing(0);
            styledBar0Layout->addSpacing(5);
            styledBar0Layout->addWidget(new QLabel(tr("Frame Buffer")));
            styledBar0Layout->addSpacing(10);
            styledBar0->setLayout(styledBar0Layout);

            m_jpgCompress = new QToolButton;
            m_jpgCompress->setText(tr("JPG"));
            m_jpgCompress->setToolTip(tr("JPEG compress the Frame Buffer for higher performance."));
            m_jpgCompress->setCheckable(true);
            m_jpgCompress->setChecked(true); // inital value
            styledBar0Layout->addWidget(m_jpgCompress);

            m_disableFrameBuffer = new QToolButton;
            m_disableFrameBuffer->setText(tr("Disable"));
            m_disableFrameBuffer->setToolTip(tr("Disable the Frame Buffer for maximum performance."));
            m_disableFrameBuffer->setCheckable(true);
            m_disableFrameBuffer->setChecked(false); // inital value
            styledBar0Layout->addWidget(m_disableFrameBuffer);

            m_frameBuffer = new OpenMVFrameBuffer;
            m_frameBuffer->setFrameShape(QFrame::NoFrame);

            QWidget *tempWidget0 = new QWidget;
            QVBoxLayout *tempLayout0 = new QVBoxLayout;
            tempLayout0->setMargin(0);
            tempLayout0->setSpacing(0);
            tempLayout0->addWidget(styledBar0);
            tempLayout0->addWidget(m_frameBuffer);
            tempWidget0->setLayout(tempLayout0);

            Utils::StyledBar *styledBar1 = new Utils::StyledBar;
            QHBoxLayout *styledBar1Layout = new QHBoxLayout;
            styledBar1Layout->setMargin(0);
            styledBar1Layout->setSpacing(0);
            styledBar1Layout->addSpacing(5);
            styledBar1Layout->addWidget(new QLabel(tr("Histogram")));
            styledBar1Layout->addSpacing(10);
            styledBar1->setLayout(styledBar1Layout);

            m_histogramChannel = new QComboBox;
            m_histogramChannel->addItems(QStringList()
            << tr("Red Channel")
            << tr("Green Channel")
            << tr("Blue Channel")
            << tr("Grayscale Channel")
            << tr("L Channel")
            << tr("A Channel")
            << tr("B Channel")
            << tr("Y Channel")
            << tr("U Channel")
            << tr("V Channel"));
            m_histogramChannel->setToolTip(tr("Frame Buffer Histogram color channel."));
            styledBar1Layout->addWidget(m_histogramChannel);

            m_histogram = new QGraphicsView;
            m_histogram->setFrameShape(QFrame::NoFrame);

            QWidget *tempWidget1 = new QWidget;
            QVBoxLayout *tempLayout1 = new QVBoxLayout;
            tempLayout1->setMargin(0);
            tempLayout1->setSpacing(0);
            tempLayout1->addWidget(styledBar1);
            tempLayout1->addWidget(m_histogram);
            tempWidget1->setLayout(tempLayout1);

            m_hsplitter = widget->m_hsplitter;
            m_vsplitter = widget->m_vsplitter;
            m_vsplitter->insertWidget(0, tempWidget0);
            m_vsplitter->insertWidget(1, tempWidget1);
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
    Core::EditorManager::restoreState(
        settings->value(QStringLiteral(EDITOR_MANAGER_STATE)).toByteArray());
    m_hsplitter->restoreState(
        settings->value(QStringLiteral(HSPLITTER_STATE)).toByteArray());
    m_vsplitter->restoreState(
        settings->value(QStringLiteral(VSPLITTER_STATE)).toByteArray());
    m_jpgCompress->setChecked(
        settings->value(QStringLiteral(JPG_COMPRESS_STATE)).toBool());
    m_disableFrameBuffer->setChecked(
        settings->value(QStringLiteral(DISABLE_FRAME_BUFFER_STATE)).toBool());
    m_histogramChannel->setCurrentIndex(
        settings->value(QStringLiteral(HISTOGRAM_CHANNEL_STATE)).toInt());
    settings->endGroup();

    connect(Core::ICore::instance(), &Core::ICore::saveSettingsRequested,
        this, &OpenMVPlugin::saveSettingsRequested);
}

void OpenMVPlugin::saveSettingsRequested()
{
    QSettings *settings = ExtensionSystem::PluginManager::settings();
    settings->beginGroup(QStringLiteral(SETTINGS_GROUP));
    settings->setValue(QStringLiteral(EDITOR_MANAGER_STATE),
        Core::EditorManager::saveState());
    settings->setValue(QStringLiteral(HSPLITTER_STATE),
        m_hsplitter->saveState());
    settings->setValue(QStringLiteral(VSPLITTER_STATE),
        m_vsplitter->saveState());
    settings->setValue(QStringLiteral(JPG_COMPRESS_STATE),
        m_jpgCompress->isChecked());
    settings->setValue(QStringLiteral(DISABLE_FRAME_BUFFER_STATE),
        m_disableFrameBuffer->isChecked());
    settings->setValue(QStringLiteral(HISTOGRAM_CHANNEL_STATE),
        m_histogramChannel->currentIndex());
    settings->endGroup();
}

QList<QAction *> aboutToShowExamplesRecursive(const QString &path, QMenu *parent, QObject *object)
{
    QList<QAction *> actions;
    QDirIterator it(path, QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);

    while(it.hasNext())
    {
        QString filePath = it.next();

        if(it.fileInfo().isDir())
        {
            QMenu *menu = new QMenu(it.fileName(), parent);
            QList<QAction *> menuActions = aboutToShowExamplesRecursive(filePath, menu, object);
            menu->addActions(menuActions);
            menu->setDisabled(menuActions.isEmpty());
            actions.append(menu->menuAction());
        }
        else
        {
            QAction *action = new QAction(it.fileName(), parent);
            QObject::connect(action, &QAction::triggered, object, [filePath]
            {
                Core::EditorManager::openEditor(filePath);
            });
            actions.append(action);
        }
    }

    return actions;
}

void OpenMVPlugin::aboutToShowExamples()
{
    m_examplesMenu->menu()->clear();
    QList<QAction *> actions = aboutToShowExamplesRecursive(Core::ICore::resourcePath() + QStringLiteral("/examples"), m_examplesMenu->menu(), this);
    m_examplesMenu->menu()->addActions(actions);
    m_examplesMenu->menu()->setDisabled(actions.isEmpty());
}

void OpenMVPlugin::docsClicked()
{
    QDesktopServices::openUrl(QUrl(QStringLiteral("http://openmv.io/docs/")));
}

void OpenMVPlugin::forumsClicked()
{
    QDesktopServices::openUrl(QUrl(QStringLiteral("http://openmv.io/forums/")));
}

void OpenMVPlugin::pinoutClicked()
{
    QDesktopServices::openUrl(QUrl(QStringLiteral("http://openmv.io/docs/_images/pinout.png")));
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

void OpenMVPlugin::connectClicked()
{
    QStringList stringList;

    foreach(QSerialPortInfo port, QSerialPortInfo::availablePorts())
    {
        if(port.hasVendorIdentifier() && (port.vendorIdentifier() == OPENMVCAM_VENDOR_ID)
        && port.hasProductIdentifier() && (port.productIdentifier() == OPENMVCAM_PRODUCT_ID))
        {
            stringList.append(port.portName());
        }
    }

    QString selectedPort;

    if(stringList.isEmpty())
    {
        QMessageBox::critical(Core::ICore::dialogParent(),
            QApplication::applicationName(),
            tr("No OpenMV Cams found"));
    }
    else if(stringList.size() == 1)
    {
        selectedPort = stringList.first();
    }
    else
    {
        QSettings *settings = ExtensionSystem::PluginManager::settings();
        settings->beginGroup(QStringLiteral(SETTINGS_GROUP));
        QString lastPortName = settings->value(QStringLiteral(LAST_SERIAL_PORT_STATE)).toString();

        int index = 0;
        if(!lastPortName.isEmpty())
        {
            int tmp = stringList.indexOf(lastPortName);
            if(tmp != -1)
            {
                index = tmp;
            }
        }

        bool ok;
        QString temp = QInputDialog::getItem(Core::ICore::dialogParent(),
            QApplication::applicationName(), tr("Please select a serial port"),
            stringList, index, false, &ok,
             Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
            (Utils::HostOsInfo::isMacHost() ? Qt::WindowType(0) : Qt::WindowCloseButtonHint));

        if(ok)
        {
            selectedPort = temp;
            settings->setValue(QStringLiteral(LAST_SERIAL_PORT_STATE), temp);
        }

        settings->endGroup();
    }

    if(!selectedPort.isEmpty())
    {
        m_serialPort = new QSerialPort(selectedPort, this);

        if(!m_serialPort->open(QIODevice::ReadWrite))
        {
            QMessageBox::critical(Core::ICore::dialogParent(),
                QApplication::applicationName(),
                tr("Open Failure: %L1").arg(m_serialPort->errorString()));

            delete m_serialPort;
            m_serialPort = NULL;
            return;
        }

        if(!m_serialPort->setBaudRate(OPENMVCAM_BAUD_RATE))
        {
            QMessageBox::critical(Core::ICore::dialogParent(),
                QApplication::applicationName(),
                tr("SetBaudRate Failure: %L1").arg(m_serialPort->errorString()));

            delete m_serialPort;
            m_serialPort = NULL;
            return;
        }

        m_connectCommand->action()->setVisible(false);
        m_disconnectCommand->action()->setVisible(true);
        m_startCommand->action()->setEnabled(true);

        connect(m_serialPort, static_cast<void(QSerialPort::*)(QSerialPort::SerialPortError)>(&QSerialPort::error), [=](QSerialPort::SerialPortError error)
        {
            Q_UNUSED(error);
            disconnectClicked();
        });
    }
}

void OpenMVPlugin::disconnectClicked()
{
    m_connectCommand->action()->setVisible(true);
    m_disconnectCommand->action()->setVisible(false);
    m_startCommand->action()->setDisabled(true);
    m_stopCommand->action()->setDisabled(true);

    delete m_serialPort;
    m_serialPort = NULL;
}

void OpenMVPlugin::startClicked()
{
    m_startCommand->action()->setDisabled(true);
    m_stopCommand->action()->setEnabled(true);
}

void OpenMVPlugin::stopClicked()
{
    m_startCommand->action()->setEnabled(true);
    m_stopCommand->action()->setDisabled(true);
}

void OpenMVPlugin::processEvents()
{
    if(m_serialPort)
    {

    }
}

} // namespace Internal
} // namespace OpenMV
