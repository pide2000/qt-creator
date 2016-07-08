#include "openmvplugin.h"

#include "app/app_version.h"

namespace OpenMV {
namespace Internal {

OpenMVPlugin::OpenMVPlugin()
{
    m_ioport = new OpenMVPluginSerialPort(this);

    connect(m_ioport, &OpenMVPluginSerialPort::openResult,
            this, &OpenMVPlugin::connectClickedResult);

    connect(m_ioport, &OpenMVPluginSerialPort::shutdown,
            this, &OpenMVPlugin::shutdown);

    m_iodevice = new OpenMVPluginIO(m_ioport, this);

    connect(m_iodevice, &OpenMVPluginIO::firmwareVersion,
            this, &OpenMVPlugin::firmwareVersion);

    connect(m_iodevice, &OpenMVPluginIO::closeResponse,
            this, &OpenMVPlugin::closeResponse);

    m_operating = false;
    m_connected = false;
    m_portName = QString();
    m_major = int();
    m_minor = int();
    m_patch = int();
    m_timer = QElapsedTimer();
    m_timerLast = qint64();
    m_errorFilterRegex = QRegularExpression(QStringLiteral(
        "  File \"(.+?)\", line (\\d+).*?\n"
        "(?!Exception: IDE interrupt)(.+?:.+?)\n"));
    m_errorFilterString = QString();

    QTimer *timer = new QTimer(this);

    connect(timer, &QTimer::timeout,
            this, &OpenMVPlugin::processEvents);

    timer->start(1);
}

OpenMVPlugin::~OpenMVPlugin()
{

}

bool OpenMVPlugin::initialize(const QStringList &arguments, QString *errorMessage)
{
    Q_UNUSED(arguments)
    Q_UNUSED(errorMessage)

    QApplication::setApplicationDisplayName(tr("OpenMV IDE"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(ICON_PATH)));

    QSplashScreen *splashScreen = new QSplashScreen(QPixmap(QStringLiteral(SPLASH_PATH)));
    connect(Core::ICore::instance(), &Core::ICore::coreOpened, splashScreen, &QSplashScreen::close);
    splashScreen->show();

    return true;
}

void OpenMVPlugin::extensionsInitialized()
{
    connect(Core::ActionManager::command(Core::Constants::NEW)->action(), &QAction::triggered, this, [this] {
        Core::EditorManager::cutForwardNavigationHistory();
        Core::EditorManager::addCurrentPositionToNavigationHistory();
        QString titlePattern = tr("Untitled $.py");
        TextEditor::BaseTextEditor *editor = qobject_cast<TextEditor::BaseTextEditor *>(Core::EditorManager::openEditorWithContents(Core::Constants::K_DEFAULT_TEXT_EDITOR_ID, &titlePattern,
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
               "    img = sensor.snapshot()\n").
            arg(Utils::Environment::systemEnvironment().userName()).arg(QDate::currentDate().toString()).toLatin1()));
        if(editor)
        {
            editor->editorWidget()->configureGenericHighlighter();
            Core::EditorManager::activateEditor(editor);
        }
        else
        {
            QMessageBox::critical(Core::ICore::dialogParent(),
                QObject::tr("New File"),
                QObject::tr("Unable to open file!"));
        }
    });

    Core::ActionContainer *filesMenu = Core::ActionManager::actionContainer(Core::Constants::M_FILE);
    m_examplesMenu = Core::ActionManager::createMenu(Core::Id("OpenMV.Examples"));
    filesMenu->addMenu(Core::ActionManager::actionContainer(Core::Constants::M_FILE_RECENTFILES), m_examplesMenu, Core::Constants::G_FILE_OPEN);
    m_examplesMenu->menu()->setTitle(tr("&Examples"));
    m_examplesMenu->setOnAllDisabledBehavior(Core::ActionContainer::Show);
    connect(filesMenu->menu(), &QMenu::aboutToShow, this, [this] {
        m_examplesMenu->menu()->clear();
        QList<QAction *> actions = aboutToShowExamplesRecursive(Core::ICore::resourcePath() + QStringLiteral("/examples"), m_examplesMenu->menu(), this);
        m_examplesMenu->menu()->addActions(actions);
        m_examplesMenu->menu()->setDisabled(actions.isEmpty());
    });

    ///////////////////////////////////////////////////////////////////////////

    Core::ActionContainer *helpMenu = Core::ActionManager::actionContainer(Core::Constants::M_HELP);
    QIcon helpIcon = QIcon::fromTheme(QStringLiteral("help-about"));

    QAction *docsCommand = new QAction(
         tr("OpenMV &Docs"), this);
    m_docsCommand = Core::ActionManager::registerAction(docsCommand, Core::Id("OpenMV.Docs"));
    helpMenu->addAction(m_docsCommand, Core::Constants::G_HELP_SUPPORT);
    docsCommand->setEnabled(true);
    connect(docsCommand, &QAction::triggered, this, [this] {
        QDesktopServices::openUrl(QUrl(QStringLiteral("http://openmv.io/docs/")));
    });

    QAction *forumsCommand = new QAction(
         tr("OpenMV &Forums"), this);
    m_forumsCommand = Core::ActionManager::registerAction(forumsCommand, Core::Id("OpenMV.Forums"));
    helpMenu->addAction(m_forumsCommand, Core::Constants::G_HELP_SUPPORT);
    forumsCommand->setEnabled(true);
    connect(forumsCommand, &QAction::triggered, this, [this] {
        QDesktopServices::openUrl(QUrl(QStringLiteral("http://openmv.io/forums/")));
    });

    QAction *pinoutAction = new QAction(
         tr("About &OpenMV Cam..."), this);
    pinoutAction->setMenuRole(QAction::ApplicationSpecificRole);
    m_pinoutCommand = Core::ActionManager::registerAction(pinoutAction, Core::Id("OpenMV.Pinout"));
    helpMenu->addAction(m_pinoutCommand, Core::Constants::G_HELP_ABOUT);
    pinoutAction->setEnabled(true);
    connect(pinoutAction, &QAction::triggered, this, [this] {
        QDesktopServices::openUrl(QUrl(QStringLiteral("http://openmv.io/docs/_images/pinout.png")));
    });

    QAction *aboutAction = new QAction(helpIcon,
        Utils::HostOsInfo::isMacHost() ? tr("About &OpenMV IDE") : tr("About &OpenMV IDE..."), this);
    aboutAction->setMenuRole(QAction::AboutRole);
    m_aboutCommand = Core::ActionManager::registerAction(aboutAction, Core::Id("OpenMV.About"));
    helpMenu->addAction(m_aboutCommand, Core::Constants::G_HELP_ABOUT);
    aboutAction->setEnabled(true);
    connect(aboutAction, &QAction::triggered, this, [this] {
        QMessageBox::about(Core::ICore::dialogParent(), tr("About OpenMV IDE"), tr(
        "<p><b>About OpenMV IDE %L1</b></p>"
        "<p>By: Ibrahim Abdelkader & Kwabena W. Agyeman</p>"
        "<p><b>GNU GENERAL PUBLIC LICENSE</b></p>"
        "<p>Copyright (C) %L2 %L3</p>"
        "<p>This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the <a href=\"http://github.com/openmv/qt-creator/raw/master/LICENSE.GPL3-EXCEPT\">GNU General Public License</a> for more details.</p>"
        "<p><b>Questions or Comments?</b></p>"
        "<p>Contact us at <a href=\"mailto:openmv@openmv.io\">openmv@openmv.io</a>.</p>"
        ).arg(QLatin1String(Core::Constants::OMV_IDE_VERSION_LONG)).arg(QLatin1String(Core::Constants::OMV_IDE_YEAR)).arg(QLatin1String(Core::Constants::OMV_IDE_AUTHOR)));
    });

    ///////////////////////////////////////////////////////////////////////////

    m_connectCommand =
        Core::ActionManager::registerAction(new QAction(QIcon(QStringLiteral(CONNECT_PATH)),
        tr("Connect"), this), Core::Id("OpenMV.Connect"));
    m_connectCommand->action()->setEnabled(true);
    m_connectCommand->action()->setVisible(true);
    connect(m_connectCommand->action(), &QAction::triggered, this, &OpenMVPlugin::connectClicked);

    m_disconnectCommand =
        Core::ActionManager::registerAction(new QAction(QIcon(QStringLiteral(DISCONNECT_PATH)),
        tr("Disconnect"), this), Core::Id("OpenMV.Disconnect"));
    m_disconnectCommand->action()->setEnabled(false);
    m_disconnectCommand->action()->setVisible(false);
    connect(m_disconnectCommand->action(), &QAction::triggered, this, &OpenMVPlugin::disconnectClicked);

    m_startCommand =
        Core::ActionManager::registerAction(new QAction(QIcon(QStringLiteral(START_PATH)),
        tr("Start"), this), Core::Id("OpenMV.Start"));
    m_startCommand->setDefaultKeySequence(tr("Ctrl+R"));
    m_startCommand->action()->setDisabled(true);
    connect(m_startCommand->action(), &QAction::triggered, this, &OpenMVPlugin::startClicked);
    connect(Core::EditorManager::instance(), &Core::EditorManager::currentEditorChanged, [this] (Core::IEditor *editor) {
        if(m_connected)
        {
            m_startCommand->action()->setEnabled(editor ? (editor->document() ? (!editor->document()->contents().isEmpty()) : false) : false);
        }
    });

    m_stopCommand =
        Core::ActionManager::registerAction(new QAction(QIcon(QStringLiteral(STOP_PATH)),
        tr("Stop"), this), Core::Id("OpenMV.Stop"));
    m_stopCommand->action()->setDisabled(true);
    connect(m_stopCommand->action(), &QAction::triggered, this, [this] {
        if(m_connected)
        {
            m_iodevice->scriptStop();
        }
    });
    connect(m_iodevice, &OpenMVPluginIO::scriptRunning, this, [this] (long running) {
        if(m_connected)
        {
            m_stopCommand->action()->setEnabled(running);
        }
    });

    ///////////////////////////////////////////////////////////////////////////

    QMainWindow *mainWindow = q_check_ptr(qobject_cast<QMainWindow *>(Core::ICore::mainWindow()));
    Core::Internal::FancyTabWidget *widget = q_check_ptr(qobject_cast<Core::Internal::FancyTabWidget *>(mainWindow->centralWidget()));

    Core::Internal::FancyActionBar *actionBar0 = new Core::Internal::FancyActionBar(widget);
    widget->insertCornerWidget(0, actionBar0);

    actionBar0->insertAction(0, Core::ActionManager::command(Core::Constants::NEW)->action());
    actionBar0->insertAction(1, Core::ActionManager::command(Core::Constants::OPEN)->action());
    actionBar0->insertAction(2, Core::ActionManager::command(Core::Constants::SAVE)->action());

    actionBar0->setProperty("no_separator", true);
    actionBar0->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    Core::Internal::FancyActionBar *actionBar1 = new Core::Internal::FancyActionBar(widget);
    widget->insertCornerWidget(1, actionBar1);

    actionBar1->insertAction(0, Core::ActionManager::command(Core::Constants::UNDO)->action());
    actionBar1->insertAction(1, Core::ActionManager::command(Core::Constants::REDO)->action());
    actionBar1->insertAction(2, Core::ActionManager::command(Core::Constants::CUT)->action());
    actionBar1->insertAction(3, Core::ActionManager::command(Core::Constants::COPY)->action());
    actionBar1->insertAction(4, Core::ActionManager::command(Core::Constants::PASTE)->action());

    actionBar1->setProperty("no_separator", false);
    actionBar1->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Minimum);

    Core::Internal::FancyActionBar *actionBar2 = new Core::Internal::FancyActionBar(widget);
    widget->insertCornerWidget(2, actionBar2);

    actionBar2->insertAction(0, m_connectCommand->action());
    actionBar2->insertAction(1, m_disconnectCommand->action());
    actionBar2->insertAction(2, m_startCommand->action());
    actionBar2->insertAction(3, m_stopCommand->action());

    actionBar2->setProperty("no_separator", false);
    actionBar2->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    ///////////////////////////////////////////////////////////////////////////

    Utils::StyledBar *styledBar0 = new Utils::StyledBar;
    QHBoxLayout *styledBar0Layout = new QHBoxLayout;
    styledBar0Layout->setMargin(0);
    styledBar0Layout->setSpacing(0);
    styledBar0Layout->addSpacing(4);
    styledBar0Layout->addWidget(new QLabel(tr("Frame Buffer")));
    styledBar0Layout->addSpacing(6);
    styledBar0->setLayout(styledBar0Layout);

    m_zoom = new QToolButton;
    m_zoom->setText(tr("Zoom"));
    m_zoom->setToolTip(tr("Zoom to fit"));
    m_zoom->setCheckable(true);
    m_zoom->setChecked(false);
    styledBar0Layout->addWidget(m_zoom);

    m_jpgCompress = new QToolButton;
    m_jpgCompress->setText(tr("JPG"));
    m_jpgCompress->setToolTip(tr("JPEG compress the Frame Buffer for higher performance"));
    m_jpgCompress->setCheckable(true);
    m_jpgCompress->setChecked(true);
    styledBar0Layout->addWidget(m_jpgCompress);
    connect(m_jpgCompress, &QToolButton::clicked, this, [this] {
        if(m_connected)
        {
            m_iodevice->jpegEnable(m_jpgCompress->isChecked());
        }
    });

    m_disableFrameBuffer = new QToolButton;
    m_disableFrameBuffer->setText(tr("Disable"));
    m_disableFrameBuffer->setToolTip(tr("Disable the Frame Buffer for maximum performance"));
    m_disableFrameBuffer->setCheckable(true);
    m_disableFrameBuffer->setChecked(false);
    styledBar0Layout->addWidget(m_disableFrameBuffer);

    m_frameBuffer = new OpenMVPluginFB;
    QWidget *tempWidget0 = new QWidget;
    QVBoxLayout *tempLayout0 = new QVBoxLayout;
    tempLayout0->setMargin(0);
    tempLayout0->setSpacing(0);
    tempLayout0->addWidget(styledBar0);
    tempLayout0->addWidget(m_frameBuffer);
    tempWidget0->setLayout(tempLayout0);
    connect(m_zoom, &QToolButton::toggled, m_frameBuffer, &OpenMVPluginFB::enableFitInView);
    connect(m_iodevice, &OpenMVPluginIO::frameBufferData, m_frameBuffer, &OpenMVPluginFB::frameBufferData);
    connect(m_frameBuffer, &OpenMVPluginFB::saveImage, this, &OpenMVPlugin::saveImage);
    connect(m_frameBuffer, &OpenMVPluginFB::saveTemplate, this, &OpenMVPlugin::saveTemplate);
    connect(m_frameBuffer, &OpenMVPluginFB::saveDescriptor, this, &OpenMVPlugin::saveDescriptor);

    Utils::StyledBar *styledBar1 = new Utils::StyledBar;
    QHBoxLayout *styledBar1Layout = new QHBoxLayout;
    styledBar1Layout->setMargin(0);
    styledBar1Layout->setSpacing(0);
    styledBar1Layout->addSpacing(4);
    styledBar1Layout->addWidget(new QLabel(tr("Histogram")));
    styledBar1Layout->addSpacing(6);
    styledBar1->setLayout(styledBar1Layout);

    m_histogramColorSpace = new QComboBox;
    m_histogramColorSpace->setProperty("hideborder", true);
    m_histogramColorSpace->setProperty("drawleftborder", false);
    m_histogramColorSpace->insertItem(RGB_COLOR_SPACE, tr("RGB Color Space"));
    m_histogramColorSpace->insertItem(GRAYSCALE_COLOR_SPACE, tr("Grayscale Color Space"));
    m_histogramColorSpace->insertItem(LAB_COLOR_SPACE, tr("LAB Color Space"));
    m_histogramColorSpace->insertItem(YUV_COLOR_SPACE, tr("YUV Color Space"));
    m_histogramColorSpace->setCurrentIndex(RGB_COLOR_SPACE);
    m_histogramColorSpace->setToolTip(tr("Use Grayscale/LAB for color tracking"));
    styledBar1Layout->addWidget(m_histogramColorSpace);

    m_histogram = new OpenMVPluginHistogram;
    QWidget *tempWidget1 = new QWidget;
    QVBoxLayout *tempLayout1 = new QVBoxLayout;
    tempLayout1->setMargin(0);
    tempLayout1->setSpacing(0);
    tempLayout1->addWidget(styledBar1);
    tempLayout1->addWidget(m_histogram);
    tempWidget1->setLayout(tempLayout1);
    connect(m_histogramColorSpace, QOverload<int>::of(&QComboBox::currentIndexChanged), m_histogram, &OpenMVPluginHistogram::colorSpaceChanged);
    connect(m_frameBuffer, &OpenMVPluginFB::pixmapUpdate, m_histogram, &OpenMVPluginHistogram::pixmapUpdate);

    m_hsplitter = widget->m_hsplitter;
    m_vsplitter = widget->m_vsplitter;
    m_vsplitter->insertWidget(0, tempWidget0);
    m_vsplitter->insertWidget(1, tempWidget1);
    m_vsplitter->setStretchFactor(0, 0);
    m_vsplitter->setStretchFactor(1, 1);

    connect(m_iodevice, &OpenMVPluginIO::printData, Core::MessageManager::instance(), &Core::MessageManager::printData);
    connect(m_iodevice, &OpenMVPluginIO::printData, this, &OpenMVPlugin::errorFilter);
    connect(m_iodevice, &OpenMVPluginIO::frameBufferData, this, [this] {
        qint64 elapsed = m_timer.elapsed();
        qint64 diff = elapsed - m_timerLast;
        m_timerLast = elapsed;
        m_fpsLabel->setText(tr("FPS: %L1").arg(round(1000.0 / diff)));
    });

    ///////////////////////////////////////////////////////////////////////////

    m_portLabel = new QLabel(tr("Serial Port:"));
    m_portLabel->setDisabled(true);
    Core::ICore::statusBar()->insertPermanentWidget(2, m_portLabel);

    m_pathButton = new QToolButton;
    m_pathButton->setText(tr("Drive:"));
    m_pathButton->setToolTip(tr("Drive associated with the port"));
    m_pathButton->setCheckable(false);
    m_pathButton->setDisabled(true);
    Core::ICore::statusBar()->insertPermanentWidget(3, m_pathButton);
    connect(m_pathButton, &QToolButton::clicked, this, [this] {
        setSerialPortPath();
        m_frameBuffer->enableSaveTemplate(!getSerialPortPath().isEmpty());
        m_frameBuffer->enableSaveDescriptor(!getSerialPortPath().isEmpty());
    });

    m_versionLabel = new QLabel(tr("Firmware Version:"));
    m_versionLabel->setDisabled(true);
    Core::ICore::statusBar()->insertPermanentWidget(4, m_versionLabel);

    m_fpsLabel = new QLabel(tr("FPS:"));
    m_fpsLabel->setDisabled(true);
    Core::ICore::statusBar()->insertPermanentWidget(5, m_fpsLabel);

    ///////////////////////////////////////////////////////////////////////////

    QSettings *settings = ExtensionSystem::PluginManager::settings();
    settings->beginGroup(QStringLiteral(SETTINGS_GROUP));
    Core::EditorManager::restoreState(
        settings->value(QStringLiteral(EDITOR_MANAGER_STATE)).toByteArray());
    m_hsplitter->restoreState(
        settings->value(QStringLiteral(HSPLITTER_STATE)).toByteArray());
    m_vsplitter->restoreState(
        settings->value(QStringLiteral(VSPLITTER_STATE)).toByteArray());
    m_zoom->setChecked(
        settings->value(QStringLiteral(ZOOM_STATE), m_zoom->isChecked()).toBool());
    m_jpgCompress->setChecked(
        settings->value(QStringLiteral(JPG_COMPRESS_STATE), m_jpgCompress->isChecked()).toBool());
    m_disableFrameBuffer->setChecked(
        settings->value(QStringLiteral(DISABLE_FRAME_BUFFER_STATE), m_disableFrameBuffer->isChecked()).toBool());
    m_histogramColorSpace->setCurrentIndex(
        settings->value(QStringLiteral(HISTOGRAM_COLOR_SPACE_STATE), m_histogramColorSpace->currentIndex()).toInt());
    settings->endGroup();

    connect(Core::ICore::instance(), &Core::ICore::saveSettingsRequested, this, [this] {
        QSettings *settings = ExtensionSystem::PluginManager::settings();
        settings->beginGroup(QStringLiteral(SETTINGS_GROUP));
        settings->setValue(QStringLiteral(EDITOR_MANAGER_STATE),
            Core::EditorManager::saveState());
        settings->setValue(QStringLiteral(HSPLITTER_STATE),
            m_hsplitter->saveState());
        settings->setValue(QStringLiteral(VSPLITTER_STATE),
            m_vsplitter->saveState());
        settings->setValue(QStringLiteral(ZOOM_STATE),
            m_zoom->isChecked());
        settings->setValue(QStringLiteral(JPG_COMPRESS_STATE),
            m_jpgCompress->isChecked());
        settings->setValue(QStringLiteral(DISABLE_FRAME_BUFFER_STATE),
            m_disableFrameBuffer->isChecked());
        settings->setValue(QStringLiteral(HISTOGRAM_COLOR_SPACE_STATE),
            m_histogramColorSpace->currentIndex());
        settings->endGroup();
    });
}

void OpenMVPlugin::connectClicked()
{
    if(!m_operating)
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
                tr("Connect"),
                tr("No OpenMV Cams found!"));
        }
        else if(stringList.size() == 1)
        {
            selectedPort = stringList.first();
        }
        else
        {
            QSettings *settings = ExtensionSystem::PluginManager::settings();
            settings->beginGroup(QStringLiteral(SETTINGS_GROUP));
            int index = stringList.indexOf(settings->value(QStringLiteral(LAST_SERIAL_PORT_STATE)).toString());

            bool ok;
            QString temp = QInputDialog::getItem(Core::ICore::dialogParent(),
                tr("Connect"), tr("Please select a serial port"),
                stringList, (index != -1) ? index : 0, false, &ok,
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
            emit m_ioport->open(selectedPort);
            m_portName = selectedPort;
            m_operating = true;
        }
    }
}

void OpenMVPlugin::connectClickedResult(const QString &errorMessage)
{
    if(errorMessage.isEmpty())
    {
        setSerialPortPath();
        m_iodevice->getFirmwareVersion();
    }
    else
    {
        m_portName = QString();
        m_operating = false;
        QMessageBox::critical(Core::ICore::dialogParent(),
            tr("Connect"),
            tr("Error: %L1").arg(errorMessage));
    }
}

void OpenMVPlugin::firmwareVersion(long major, long minor, long patch)
{
    if((!major) && (!minor) && (!patch))
    {
        m_iodevice->close();
        QMessageBox::critical(Core::ICore::dialogParent(),
            tr("Connect"),
            tr("Timeout error while getting firmware version!"));
    }
    else
    {
        m_operating = false;
        m_connected = true;
        m_major = major;
        m_minor = minor;
        m_patch = patch;
        m_timer.start();
        m_timerLast = m_timer.elapsed() - 2000;
        m_errorFilterString = QString();

        m_connectCommand->action()->setEnabled(false);
        m_connectCommand->action()->setVisible(false);
        m_disconnectCommand->action()->setEnabled(true);
        m_disconnectCommand->action()->setVisible(true);
        m_startCommand->action()->setEnabled(Core::EditorManager::currentDocument());
        m_stopCommand->action()->setEnabled(false);

        m_frameBuffer->enableSaveTemplate(!getSerialPortPath().isEmpty());
        m_frameBuffer->enableSaveDescriptor(!getSerialPortPath().isEmpty());

        m_portLabel->setEnabled(true);
        m_portLabel->setText(tr("Serial Port: %L1").arg(m_portName));
        m_pathButton->setEnabled(true);
        m_pathButton->setText((!getSerialPortPath().isEmpty()) ? tr("Drive: %L1").arg(getSerialPortPath()) : tr("Drive:"));
        m_versionLabel->setEnabled(true);
        m_versionLabel->setText(tr("Firmware Version: %L1.%L2.%L3").arg(major).arg(minor).arg(patch));
        m_fpsLabel->setEnabled(true);
        m_fpsLabel->setText(tr("FPS: 0"));

        m_iodevice->scriptStop();
        m_iodevice->jpegEnable(m_jpgCompress->isChecked());
    }
}

void OpenMVPlugin::disconnectClicked()
{
    if(m_connected)
    {
        m_connected = false;

        m_iodevice->scriptStop();
        m_iodevice->close();
    }
}

void OpenMVPlugin::shutdown(const QString &errorMessage)
{
    if(m_operating)
    {
        QMessageBox::critical(Core::ICore::dialogParent(),
            tr("Connect"),
            tr("Error: %L1").arg(errorMessage));
    }
    else
    {
        m_connected = false;

        m_iodevice->close();

        QMessageBox::critical(Core::ICore::dialogParent(),
            tr("Disconnect"),
            tr("Error: %L1").arg(errorMessage));
    }
}

void OpenMVPlugin::closeResponse()
{
    m_operating = false;
    m_portName = QString();
    m_major = int();
    m_minor = int();
    m_patch = int();
    m_timer.invalidate();
    m_timerLast = qint64();
    m_errorFilterString = QString();

    m_connectCommand->action()->setEnabled(true);
    m_connectCommand->action()->setVisible(true);
    m_disconnectCommand->action()->setVisible(false);
    m_disconnectCommand->action()->setEnabled(false);
    m_startCommand->action()->setEnabled(false);
    m_stopCommand->action()->setEnabled(false);

    m_frameBuffer->enableSaveTemplate(false);
    m_frameBuffer->enableSaveDescriptor(false);

    m_portLabel->setDisabled(true);
    m_portLabel->setText(tr("Serial Port:"));
    m_pathButton->setDisabled(true);
    m_pathButton->setText(tr("Drive:"));
    m_versionLabel->setDisabled(true);
    m_versionLabel->setText(tr("Firmware Version:"));
    m_fpsLabel->setDisabled(true);
    m_fpsLabel->setText(tr("FPS:"));
}

void OpenMVPlugin::startClicked()
{
    if(m_connected)
    {
        Core::MessageManager::grayOutOldContent();

        if(m_stopCommand->action()->isEnabled())
        {
            m_iodevice->scriptStop();
        }

        m_iodevice->scriptExec(Core::EditorManager::currentDocument()->contents());
    }
}

void OpenMVPlugin::startFinished()
{

}

void OpenMVPlugin::processEvents()
{
    if(m_connected)
    {
        if((!m_disableFrameBuffer->isChecked())
        && (!m_iodevice->frameSizeDumpQueued()))
        {
            m_iodevice->frameSizeDump();
        }

        if(!m_iodevice->getScriptRunningQueued())
        {
            m_iodevice->getScriptRunning();
        }

        if(!m_iodevice->getTxBufferQueued())
        {
            m_iodevice->getTxBuffer();
        }

        if((m_timer.elapsed() - m_timerLast) >= 2000)
        {
            m_fpsLabel->setText(tr("FPS: 0"));
        }
    }
}

void OpenMVPlugin::errorFilter(const QByteArray &data)
{
    int errorFilterMaxSize = 1000;
    m_errorFilterString.append(Utils::SynchronousProcess::normalizeNewlines(QString::fromLatin1(data)));

    QRegularExpressionMatch match;
    int index = m_errorFilterString.indexOf(m_errorFilterRegex, 0, &match);

    if(index != -1)
    {
        QString fileName = match.captured(1);
        int lineNumber = match.captured(2).toInt();
        QString errorMessage = match.captured(3);

        Core::EditorManager::cutForwardNavigationHistory();
        Core::EditorManager::addCurrentPositionToNavigationHistory();

        TextEditor::BaseTextEditor *editor = Q_NULLPTR;

        if(fileName == QStringLiteral("<stdin>"))
        {
            editor = qobject_cast<TextEditor::BaseTextEditor *>(Core::EditorManager::currentEditor());
        }
        else if(!getSerialPortPath().isEmpty())
        {
            editor = qobject_cast<TextEditor::BaseTextEditor *>(Core::EditorManager::openEditor(QDir::cleanPath(QDir::fromNativeSeparators(QString(fileName).prepend(getSerialPortPath())))));
        }

        if(editor)
        {
            Core::EditorManager::addCurrentPositionToNavigationHistory();
            editor->gotoLine(lineNumber);

            QTextCursor cursor = editor->textCursor();

            if(cursor.movePosition(QTextCursor::EndOfBlock, QTextCursor::KeepAnchor))
            {
                editor->editorWidget()->setBlockSelection(cursor);
            }

            Core::EditorManager::activateEditor(editor);
        }

        QMessageBox::critical(Core::ICore::dialogParent(),
            QString(),
            errorMessage);

        m_errorFilterString = m_errorFilterString.mid(index + match.capturedLength(0));
    }

    if(m_errorFilterString.size() > errorFilterMaxSize)
    {
        m_errorFilterString = m_errorFilterString.right(errorFilterMaxSize);
    }
}

void OpenMVPlugin::saveImage(const QPixmap &data)
{
    QSettings *settings = ExtensionSystem::PluginManager::settings();
    settings->beginGroup(QStringLiteral(SETTINGS_GROUP));

    QString path =
        QFileDialog::getSaveFileName(Core::ICore::dialogParent(), tr("Save Image"),
            settings->value(QStringLiteral(LAST_SAVE_IMAGE_PATH), QDir::homePath()).toString(),
            tr("Image Files (*.bmp *.jpg *.jpeg *.png *.ppm)"));

    if(!path.isEmpty())
    {
        if(data.save(path))
        {
            settings->setValue(QStringLiteral(LAST_SAVE_IMAGE_PATH), path);
        }
        else
        {
            QMessageBox::critical(Core::ICore::dialogParent(),
                tr("Save Image"),
                tr("Failed to save the image file for an unknown reason!"));
        }
    }

    settings->endGroup();
}

void OpenMVPlugin::saveTemplate(const QRect &rect)
{
    if(m_connected)
    {
        // Get the drive path first because it uses the settings too...
        QString drivePath = QDir::cleanPath(QDir::fromNativeSeparators(getSerialPortPath()));

        QSettings *settings = ExtensionSystem::PluginManager::settings();
        settings->beginGroup(QStringLiteral(SETTINGS_GROUP));

        QString path =
            QFileDialog::getSaveFileName(Core::ICore::dialogParent(), tr("Save Template"),
                settings->value(QStringLiteral(LAST_SAVE_TEMPLATE_PATH), drivePath).toString(),
                tr("Image Files (*.bmp *.jpg *.jpeg *.pgm *.ppm)"));

        if(!path.isEmpty())
        {
            path = QDir::cleanPath(QDir::fromNativeSeparators(path));

            if((!path.startsWith(drivePath))
            || (!QDir(QFileInfo(path).path()).exists()))
            {
                QMessageBox::critical(Core::ICore::dialogParent(),
                    tr("Save Template"),
                    tr("Please select a valid path on the OpenMV Cam!"));
            }
            else
            {
                m_iodevice->templateSave(rect.x(), rect.y(), rect.width(), rect.height(),
                    QString(path).remove(0, drivePath.size()).prepend(QChar::fromLatin1('/')).toLatin1());
                settings->setValue(QStringLiteral(LAST_SAVE_TEMPLATE_PATH), path);
            }
        }

        settings->endGroup();
    }
}

void OpenMVPlugin::saveDescriptor(const QRect &rect)
{
    if(m_connected)
    {
        // Get the drive path first because it uses the settings too...
        QString drivePath = QDir::cleanPath(QDir::fromNativeSeparators(getSerialPortPath()));

        QSettings *settings = ExtensionSystem::PluginManager::settings();
        settings->beginGroup(QStringLiteral(SETTINGS_GROUP));

        QString path =
            QFileDialog::getSaveFileName(Core::ICore::dialogParent(), tr("Save Descriptor"),
                settings->value(QStringLiteral(LAST_SAVE_DESCIPTOR_PATH), drivePath).toString(),
                tr("Image Files (*.lbp *.ff)"));

        if(!path.isEmpty())
        {
            path = QDir::cleanPath(QDir::fromNativeSeparators(path));

            if((!path.startsWith(drivePath))
            || (!QDir(QFileInfo(path).path()).exists()))
            {
                QMessageBox::critical(Core::ICore::dialogParent(),
                    tr("Save Descriptor"),
                    tr("Please select a valid path on the OpenMV Cam!"));
            }
            else
            {
                m_iodevice->descriptorSave(rect.x(), rect.y(), rect.width(), rect.height(),
                    QString(path).remove(0, drivePath.size()).prepend(QChar::fromLatin1('/')).toLatin1());
                settings->setValue(QStringLiteral(LAST_SAVE_DESCIPTOR_PATH), path);
            }
        }

        settings->endGroup();
    }
}

QList<QAction *> OpenMVPlugin::aboutToShowExamplesRecursive(const QString &path, QMenu *parent, QObject *object)
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
            QObject::connect(action, &QAction::triggered, object, [filePath, this]
            {
                QFile file(filePath);

                if(file.open(QIODevice::ReadOnly))
                {
                    Core::EditorManager::cutForwardNavigationHistory();
                    Core::EditorManager::addCurrentPositionToNavigationHistory();

                    QString titlePattern = QFileInfo(filePath).baseName().simplified() + QStringLiteral(" $.") + QFileInfo(filePath).completeSuffix();
                    TextEditor::BaseTextEditor *editor = qobject_cast<TextEditor::BaseTextEditor *>(Core::EditorManager::openEditorWithContents(Core::Constants::K_DEFAULT_TEXT_EDITOR_ID, &titlePattern, file.readAll()));

                    if(editor)
                    {
                        editor->editorWidget()->configureGenericHighlighter();
                        Core::EditorManager::activateEditor(editor);
                    }
                    else
                    {
                        QMessageBox::critical(Core::ICore::dialogParent(),
                            tr("Open Example"),
                            tr("Unable to open the example file \"%L1\"!").arg(filePath));
                    }
                }
                else
                {
                    QMessageBox::critical(Core::ICore::dialogParent(),
                        tr("Open Example"),
                        tr("Error: %L1").arg(file.errorString()));
                }
            });

            actions.append(action);
        }
    }

    return actions;
}

QString OpenMVPlugin::getSerialPortPath() const
{
    QSettings *settings = ExtensionSystem::PluginManager::settings();
    settings->beginGroup(QStringLiteral(SERIAL_PORT_SETTINGS_GROUP));
    QString path = settings->value(m_portName).toString();
    settings->endGroup();
    return path;
}

void OpenMVPlugin::setSerialPortPath()
{
    QStringList drives;

    foreach(const QStorageInfo &info, QStorageInfo::mountedVolumes())
    {
        if(info.isValid()
        && info.isReady()
        && (!info.isRoot())
        && (!info.isReadOnly())
        && (QString::fromLatin1(info.fileSystemType()).contains(QStringLiteral("FAT"), Qt::CaseInsensitive)))
        {
            drives.append(info.rootPath());
        }
    }

    QSettings *settings = ExtensionSystem::PluginManager::settings();
    settings->beginGroup(QStringLiteral(SERIAL_PORT_SETTINGS_GROUP));

    if(drives.isEmpty())
    {
        QMessageBox::critical(Core::ICore::dialogParent(),
            tr("Select Drive"),
            tr("No valid drives were found to associate with your OpenMV Cam!"));
    }
    else if(drives.size() == 1)
    {
        settings->setValue(m_portName, drives.first());
    }
    else
    {
        int index = drives.indexOf(settings->value(m_portName).toString());

        bool ok;
        QString temp = QInputDialog::getItem(Core::ICore::dialogParent(),
            tr("Select Drive"), tr("Please associate a drive with your OpenMV Cam"),
            drives, (index != -1) ? index : 0, false, &ok,
            Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
            (Utils::HostOsInfo::isMacHost() ? Qt::WindowType(0) : Qt::WindowCloseButtonHint));

        if(ok)
        {
            settings->setValue(m_portName, temp);
        }
    }

    settings->endGroup();
}

} // namespace Internal
} // namespace OpenMV
