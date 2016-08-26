#include "openmvplugin.h"

#include "app/app_version.h"

namespace OpenMV {
namespace Internal {

OpenMVPlugin::OpenMVPlugin() : IPlugin()
{
    m_ioport = new OpenMVPluginSerialPort(this);
    m_iodevice = new OpenMVPluginIO(m_ioport, this);

    m_working = false;
    m_connected = false;
    m_running = false;
    m_portName = QString();
    m_timer = QElapsedTimer();
    m_queue = QQueue<qint64>();
    m_errorFilterRegex = QRegularExpression(QStringLiteral(
        "  File \"(.+?)\", line (\\d+).*?\n"
        "(?!Exception: IDE interrupt)(.+?:.+?)\n"));
    m_errorFilterString = QString();

    QTimer *timer = new QTimer(this);

    connect(timer, &QTimer::timeout,
            this, &OpenMVPlugin::processEvents);

    timer->start(1);
}

bool OpenMVPlugin::initialize(const QStringList &arguments, QString *errorMessage)
{
    Q_UNUSED(arguments)
    Q_UNUSED(errorMessage)

    QSplashScreen *splashScreen = new QSplashScreen(QPixmap(QStringLiteral(SPLASH_PATH)));
    connect(Core::ICore::instance(), &Core::ICore::coreOpened, splashScreen, &QSplashScreen::deleteLater);
    splashScreen->show();

    return true;
}

void OpenMVPlugin::extensionsInitialized()
{
    QApplication::setApplicationDisplayName(tr("OpenMV IDE"));
    QApplication::setWindowIcon(QIcon(QStringLiteral(ICON_PATH)));

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
                QObject::tr("Cannot open the new file!"));
        }
    });

    Core::ActionContainer *filesMenu = Core::ActionManager::actionContainer(Core::Constants::M_FILE);
    Core::ActionContainer *examplesMenu = Core::ActionManager::createMenu(Core::Id("OpenMV.Examples"));
    filesMenu->addMenu(Core::ActionManager::actionContainer(Core::Constants::M_FILE_RECENTFILES), examplesMenu, Core::Constants::G_FILE_OPEN);
    examplesMenu->menu()->setTitle(tr("Examples"));
    examplesMenu->setOnAllDisabledBehavior(Core::ActionContainer::Show);
    connect(filesMenu->menu(), &QMenu::aboutToShow, this, [this, examplesMenu] {
        examplesMenu->menu()->clear();
        QMap<QString, QAction *> actions = aboutToShowExamplesRecursive(Core::ICore::resourcePath() + QStringLiteral("/examples"), examplesMenu->menu());
        examplesMenu->menu()->addActions(actions.values());
        examplesMenu->menu()->setDisabled(actions.values().isEmpty());
    });

    ///////////////////////////////////////////////////////////////////////////

    Core::ActionContainer *toolsMenu = Core::ActionManager::actionContainer(Core::Constants::M_TOOLS);
    Core::ActionContainer *helpMenu = Core::ActionManager::actionContainer(Core::Constants::M_HELP);

    QAction *saveCommand = new QAction(
         tr("Save script to OpenMV Cam"), this);
    m_saveCommand = Core::ActionManager::registerAction(saveCommand, Core::Id("OpenMV.Save"));
    toolsMenu->addAction(m_saveCommand);
    saveCommand->setEnabled(false);
    connect(saveCommand, &QAction::triggered, this, &OpenMVPlugin::saveScript);

    QAction *resetCommand = new QAction(
         tr("Reset OpenMV Cam"), this);
    m_resetCommand = Core::ActionManager::registerAction(resetCommand, Core::Id("OpenMV.Reset"));
    toolsMenu->addAction(m_resetCommand);
    resetCommand->setEnabled(false);
    connect(resetCommand, &QAction::triggered, this, &OpenMVPlugin::resetClicked);

    QAction *docsCommand = new QAction(
         tr("OpenMV Docs"), this);
    m_docsCommand = Core::ActionManager::registerAction(docsCommand, Core::Id("OpenMV.Docs"));
    helpMenu->addAction(m_docsCommand, Core::Constants::G_HELP_SUPPORT);
    docsCommand->setEnabled(true);
    connect(docsCommand, &QAction::triggered, this, [this] {
        QDesktopServices::openUrl(QUrl(Core::ICore::resourcePath() + QStringLiteral("/html/index.html")));
    });

    QAction *forumsCommand = new QAction(
         tr("OpenMV Forums"), this);
    m_forumsCommand = Core::ActionManager::registerAction(forumsCommand, Core::Id("OpenMV.Forums"));
    helpMenu->addAction(m_forumsCommand, Core::Constants::G_HELP_SUPPORT);
    forumsCommand->setEnabled(true);
    connect(forumsCommand, &QAction::triggered, this, [this] {
        QDesktopServices::openUrl(QUrl(QStringLiteral("http://openmv.io/forums/")));
    });

    QAction *pinoutAction = new QAction(
         tr("About OpenMV Cam..."), this);
    pinoutAction->setMenuRole(QAction::ApplicationSpecificRole);
    m_pinoutCommand = Core::ActionManager::registerAction(pinoutAction, Core::Id("OpenMV.Pinout"));
    helpMenu->addAction(m_pinoutCommand, Core::Constants::G_HELP_ABOUT);
    pinoutAction->setEnabled(true);
    connect(pinoutAction, &QAction::triggered, this, [this] {
        QDesktopServices::openUrl(QUrl(Core::ICore::resourcePath() + QStringLiteral("/html/_images/pinout.png")));
    });

    QAction *aboutAction = new QAction(QIcon::fromTheme(QStringLiteral("help-about")),
        Utils::HostOsInfo::isMacHost() ? tr("About OpenMV IDE") : tr("About OpenMV IDE..."), this);
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
    connect(m_disconnectCommand->action(), &QAction::triggered, this, [this] {disconnectClicked();});

    m_startCommand =
        Core::ActionManager::registerAction(new QAction(QIcon(QStringLiteral(START_PATH)),
        tr("Start (run script)"), this), Core::Id("OpenMV.Start"));
    m_startCommand->setDefaultKeySequence(tr("Ctrl+R"));
    m_startCommand->action()->setEnabled(false);
    m_startCommand->action()->setVisible(true);
    connect(m_startCommand->action(), &QAction::triggered, this, &OpenMVPlugin::startClicked);
    connect(Core::EditorManager::instance(), &Core::EditorManager::currentEditorChanged, [this] (Core::IEditor *editor) {
        if(m_connected)
        {
            m_saveCommand->action()->setEnabled((!getSerialPortPath().isEmpty()) && (editor ? (editor->document() ? (!editor->document()->contents().isEmpty()) : false) : false));
            m_startCommand->action()->setEnabled((!m_running) && (editor ? (editor->document() ? (!editor->document()->contents().isEmpty()) : false) : false));
            m_startCommand->action()->setVisible(!m_running);
            m_stopCommand->action()->setEnabled(m_running);
            m_stopCommand->action()->setVisible(m_running);
        }
    });

    m_stopCommand =
        Core::ActionManager::registerAction(new QAction(QIcon(QStringLiteral(STOP_PATH)),
        tr("Stop"), this), Core::Id("OpenMV.Stop"));
    m_stopCommand->setDefaultKeySequence(tr("Ctrl+T"));
    m_stopCommand->action()->setEnabled(false);
    m_stopCommand->action()->setVisible(false);
    connect(m_stopCommand->action(), &QAction::triggered, this, &OpenMVPlugin::stopClicked);
    connect(m_iodevice, &OpenMVPluginIO::scriptRunning, this, [this] (long running) {
        if(m_connected)
        {
            Core::IEditor *editor = Core::EditorManager::currentEditor();
            m_saveCommand->action()->setEnabled((!getSerialPortPath().isEmpty()) && (editor ? (editor->document() ? (!editor->document()->contents().isEmpty()) : false) : false));
            m_startCommand->action()->setEnabled((!running) && (editor ? (editor->document() ? (!editor->document()->contents().isEmpty()) : false) : false));
            m_startCommand->action()->setVisible(!running);
            m_stopCommand->action()->setEnabled(running);
            m_stopCommand->action()->setVisible(running);
            m_running = bool(running);
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
    ///// Disable JPEG Compress /////
    m_jpgCompress->setVisible(false);
    styledBar0Layout->addWidget(m_jpgCompress);
    connect(m_jpgCompress, &QToolButton::clicked, this, [this] {
        if(m_connected)
        {
            if(!m_working)
            {
                m_iodevice->jpegEnable(m_jpgCompress->isChecked());
            }
            else
            {
                QMessageBox::critical(Core::ICore::dialogParent(),
                    tr("JPG"),
                    tr("Busy... please wait..."));
            }
        }
    });

    m_disableFrameBuffer = new QToolButton;
    m_disableFrameBuffer->setText(tr("Disable"));
    m_disableFrameBuffer->setToolTip(tr("Disable the Frame Buffer for maximum performance"));
    m_disableFrameBuffer->setCheckable(true);
    m_disableFrameBuffer->setChecked(false);
    styledBar0Layout->addWidget(m_disableFrameBuffer);
    connect(m_disableFrameBuffer, &QToolButton::clicked, this, [this] {
        if(m_connected)
        {
            if(!m_working)
            {
                m_iodevice->fbEnable(!m_disableFrameBuffer->isChecked());
            }
            else
            {
                QMessageBox::critical(Core::ICore::dialogParent(),
                    tr("Disable"),
                    tr("Busy... please wait..."));
            }
        }
    });

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
    connect(m_histogramColorSpace, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), m_histogram, &OpenMVPluginHistogram::colorSpaceChanged);
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
        if(m_timer.isValid())
        {
            m_queue.push_back(m_timer.restart());

            if(m_queue.size() > 10)
            {
                m_queue.pop_front();
            }

            qint64 average = 0;

            for(int i = 0; i < m_queue.size(); i++)
            {
                average += m_queue.at(i);
            }

            average /= m_queue.size();

            if(average)
            {
                m_fpsLabel->setText(tr("FPS: %L1").arg(1000.0 / average, 5, 'f', 1));
            }
        }
    });

    ///////////////////////////////////////////////////////////////////////////

    m_portLabel = new QLabel(tr("Serial Port:"));
    m_portLabel->setDisabled(true);
    Core::ICore::statusBar()->addPermanentWidget(m_portLabel);

    m_pathButton = new QToolButton;
    m_pathButton->setText(tr("Drive:"));
    m_pathButton->setToolTip(tr("Drive associated with the port"));
    m_pathButton->setCheckable(false);
    m_pathButton->setDisabled(true);
    Core::ICore::statusBar()->addPermanentWidget(m_pathButton);
    connect(m_pathButton, &QToolButton::clicked, this, [this] {
        setSerialPortPath(true);
    });

    m_versionLabel = new QLabel(tr("Firmware Version:"));
    m_versionLabel->setDisabled(true);
    Core::ICore::statusBar()->addPermanentWidget(m_versionLabel);

    m_fpsLabel = new QLabel(tr("FPS:"));
    m_fpsLabel->setToolTip(tr("May be different from camera FPS"));
    m_fpsLabel->setDisabled(true);
    Core::ICore::statusBar()->addPermanentWidget(m_fpsLabel);

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

    ///////////////////////////////////////////////////////////////////////////

    Core::IEditor *editor = Core::EditorManager::currentEditor();

    if(editor ? (editor->document() ? editor->document()->contents().isEmpty() : true) : true)
    {
        QString filePath = Core::ICore::resourcePath() + QStringLiteral("/examples/01-Basics/helloworld.py");

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
        }
    }

    ///////////////////////////////////////////////////////////////////////////

    connect(Core::ICore::instance(), &Core::ICore::coreOpened, this, [this] {
        QNetworkAccessManager *manager = new QNetworkAccessManager(this);
        connect(manager, &QNetworkAccessManager::finished, this, [this] (QNetworkReply *reply) {
            QRegularExpressionMatch match = QRegularExpression(QStringLiteral("(\\d+)\\.(\\d+)\\.(\\d+)")).match(QString::fromLatin1(reply->readAll()));
            if((reply->error() == QNetworkReply::NoError) && ((OMV_IDE_VERSION_MAJOR < match.captured(1).toInt()) || (OMV_IDE_VERSION_MINOR < match.captured(2).toInt()) || (OMV_IDE_VERSION_RELEASE < match.captured(3).toInt())))
            {
                QMessageBox::information(Core::ICore::dialogParent(),
                    tr("Update Available"),
                    tr("A new version of OpenMV IDE is available for <a href=\"http://openmv.io/download\">download</a>."));
            }
            reply->deleteLater();
        });
        QNetworkReply *reply = manager->get(QNetworkRequest(QUrl(QStringLiteral("http://openmv.io/upload/openmv-ide-version.txt"))));
        if(reply)
        {
            connect(reply, &QNetworkReply::sslErrors, reply, static_cast<void (QNetworkReply::*)(void)>(&QNetworkReply::ignoreSslErrors));
        }
    });
}

ExtensionSystem::IPlugin::ShutdownFlag OpenMVPlugin::aboutToShutdown()
{
    if(!m_connected)
    {
        if(!m_working)
        {
            return ExtensionSystem::IPlugin::SynchronousShutdown;
        }
        else
        {
            connect(this, &OpenMVPlugin::workingDone, this, [this] {disconnectClicked();});
            connect(this, &OpenMVPlugin::disconnectDone, this, &OpenMVPlugin::asynchronousShutdownFinished);
            return ExtensionSystem::IPlugin::AsynchronousShutdown;
        }
    }
    else
    {
        if(!m_working)
        {
            connect(this, &OpenMVPlugin::disconnectDone, this, &OpenMVPlugin::asynchronousShutdownFinished);
            QTimer::singleShot(0, this, [this] {disconnectClicked();});
            return ExtensionSystem::IPlugin::AsynchronousShutdown;
        }
        else
        {
            connect(this, &OpenMVPlugin::workingDone, this, [this] {disconnectClicked();});
            connect(this, &OpenMVPlugin::disconnectDone, this, &OpenMVPlugin::asynchronousShutdownFinished);
            return ExtensionSystem::IPlugin::AsynchronousShutdown;
        }
    }
}

#define CONNECT_END() \
do { \
    m_working = false; \
    QTimer::singleShot(0, this, OpenMVPlugin::workingDone); \
    return; \
} while(0)

#define RECONNECT_END() \
do { \
    m_working = false; \
    QTimer::singleShot(0, this, &OpenMVPlugin::connectClicked); \
    return; \
} while(0)

#define CLOSE_CONNECT_END() \
do { \
    QEventLoop loop; \
    connect(m_iodevice, &OpenMVPluginIO::closeResponse, &loop, &QEventLoop::quit); \
    m_iodevice->close(); \
    loop.exec(); \
    m_working = false; \
    QTimer::singleShot(0, this, OpenMVPlugin::workingDone); \
    return; \
} while(0)

#define CLOSE_RECONNECT_END() \
do { \
    QEventLoop loop; \
    connect(m_iodevice, &OpenMVPluginIO::closeResponse, &loop, &QEventLoop::quit); \
    m_iodevice->close(); \
    loop.exec(); \
    m_working = false; \
    QTimer::singleShot(0, this, &OpenMVPlugin::connectClicked); \
    return; \
} while(0)

void OpenMVPlugin::connectClicked()
{
    if(!m_working)
    {
        m_working = true;

        QStringList stringList;

        foreach(QSerialPortInfo port, QSerialPortInfo::availablePorts())
        {
            if(port.hasVendorIdentifier() && (port.vendorIdentifier() == OPENMVCAM_VENDOR_ID)
            && port.hasProductIdentifier() && (port.productIdentifier() == OPENMVCAM_PRODUCT_ID))
            {
                stringList.append(port.portName());
            }
        }

        if(Utils::HostOsInfo::isMacHost())
        {
            stringList = stringList.filter(QStringLiteral("cu"), Qt::CaseInsensitive);
        }

        QString selectedPort;

        bool forceBootloader = bool();
        QString forceFirmwarePath = QString();
        int forceFlashFSErase = int();

        if(stringList.isEmpty())
        {
            QMessageBox::critical(Core::ICore::dialogParent(),
                tr("Connect"),
                tr("No OpenMV Cams found!"));

            QFile boards(Core::ICore::resourcePath() + QStringLiteral("/firmware/boards.txt"));

            if(boards.open(QIODevice::ReadOnly))
            {
                QMap<QString, QString> mappings;
                QTextStream stream(&boards);

                while(!stream.atEnd())
                {
                    QRegularExpressionMatch mapping = QRegularExpression(QStringLiteral("(\\S+)\\s+(\\S+)\\s+(\\S+)")).match(stream.readLine());
                    mappings.insert(mapping.captured(2).replace(QStringLiteral("_"), QStringLiteral(" ")), mapping.captured(3).replace(QStringLiteral("_"), QStringLiteral(" ")));
                }

                if(QMessageBox::question(Core::ICore::dialogParent(),
                    tr("Connect"),
                    tr("Do you have an OpenMV Cam attached and is it bricked?"),
                    QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes)
                == QMessageBox::Yes)
                {
                    bool ok;
                    QString temp = QInputDialog::getItem(Core::ICore::dialogParent(),
                        tr("Connect"), tr("Please select the board type."),
                        mappings.keys(), 0, false, &ok,
                        Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
                        (Utils::HostOsInfo::isMacHost() ? Qt::WindowType(0) : Qt::WindowCloseButtonHint));

                    if(ok)
                    {
                        forceFirmwarePath = Core::ICore::resourcePath() + QStringLiteral("/firmware/") + mappings.value(temp) + QStringLiteral("/firmware.bin");
                        forceFlashFSErase = QMessageBox::question(Core::ICore::dialogParent(),
                            tr("Connect"),
                            tr("Erase internal file system?"),
                            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::No);

                        if(forceFlashFSErase != QMessageBox::Cancel)
                        {
                            QProgressDialog dialog(tr("Disconnect your OpenMV Cam and then reconnect it..."), tr("Cancel"), 0, 0, Core::ICore::dialogParent(),
                                Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
                                (Utils::HostOsInfo::isMacHost() ? Qt::WindowType(0) : Qt::WindowCloseButtonHint));
                            dialog.show();

                            forever
                            {
                                QApplication::processEvents();

                                if(dialog.wasCanceled())
                                {
                                    break;
                                }

                                foreach(QSerialPortInfo port, QSerialPortInfo::availablePorts())
                                {
                                    if(port.hasVendorIdentifier() && (port.vendorIdentifier() == OPENMVCAM_VENDOR_ID)
                                    && port.hasProductIdentifier() && (port.productIdentifier() == OPENMVCAM_PRODUCT_ID))
                                    {
                                        stringList.append(port.portName());
                                    }
                                }

                                if(Utils::HostOsInfo::isMacHost())
                                {
                                    stringList = stringList.filter(QStringLiteral("cu"), Qt::CaseInsensitive);
                                }

                                if(!stringList.isEmpty())
                                {
                                    break;
                                }

                                // Wait 100 ms ////////////////////////////////
                                {
                                    QEventLoop eventLoop;

                                    QTimer::singleShot(100, &eventLoop, &QEventLoop::quit);

                                    eventLoop.exec();
                                }
                            }

                            if(!stringList.isEmpty())
                            {
                                selectedPort = stringList.first();
                                forceBootloader = true;
                            }
                        }
                    }
                }
            }
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

        if(selectedPort.isEmpty())
        {
            CONNECT_END();
        }

        // Open Port //////////////////////////////////////////////////////////
        {
            QString errorMessage2 = QString();
            QString *errorMessage2Ptr = &errorMessage2;

            QMetaObject::Connection conn = connect(m_ioport, &OpenMVPluginSerialPort::openResult,
                this, [this, errorMessage2Ptr] (const QString &errorMessage) {
                *errorMessage2Ptr = errorMessage;
            });

            QProgressDialog dialog(tr("Connecting..."), tr("Cancel"), 0, 0, Core::ICore::dialogParent(),
                Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
                (Utils::HostOsInfo::isMacHost() ? Qt::WindowType(0) : Qt::WindowCloseButtonHint));

            forever
            {
                // Always set this each loop...
                errorMessage2 = tr("Application force quit!");

                QEventLoop loop;

                connect(m_ioport, &OpenMVPluginSerialPort::openResult, &loop, &QEventLoop::quit);

                emit m_ioport->open(selectedPort);

                loop.exec();

                if(errorMessage2.isEmpty() || dialog.wasCanceled() || (Utils::HostOsInfo::isLinuxHost() && errorMessage2.contains(QStringLiteral("Permission Denied"), Qt::CaseInsensitive)))
                {
                    break;
                }

                dialog.show();

                // Wait 100 ms ////////////////////////////////////////////////
                {
                    QEventLoop eventLoop;

                    QTimer::singleShot(100, &eventLoop, &QEventLoop::quit);

                    eventLoop.exec();
                }
            }

            disconnect(conn);

            if(!errorMessage2.isEmpty())
            {
                QMessageBox::critical(Core::ICore::dialogParent(),
                    tr("Connect"),
                    tr("Error: %L1").arg(errorMessage2));

                if(Utils::HostOsInfo::isLinuxHost() && errorMessage2.contains(QStringLiteral("Permission Denied"), Qt::CaseInsensitive))
                {
                    QMessageBox::information(Core::ICore::dialogParent(),
                        tr("Connect"),
                        tr("Try doing:\n\nsudo adduser %L1 dialout\n\n...in a terminal and then restart your computer.").arg(Utils::Environment::systemEnvironment().userName()));
                }

                CONNECT_END();
            }
        }

        // Get Version ////////////////////////////////////////////////////////

        long major2 = long();
        long minor2 = long();
        long patch2 = long();

        if(!forceBootloader)
        {
            long *major2Ptr = &major2;
            long *minor2Ptr = &minor2;
            long *patch2Ptr = &patch2;

            QMetaObject::Connection conn = connect(m_iodevice, &OpenMVPluginIO::firmwareVersion,
                this, [this, major2Ptr, minor2Ptr, patch2Ptr] (long major, long minor, long patch) {
                *major2Ptr = major;
                *minor2Ptr = minor;
                *patch2Ptr = patch;
            });

            QEventLoop loop;

            connect(m_iodevice, &OpenMVPluginIO::firmwareVersion, &loop, &QEventLoop::quit);

            m_iodevice->getFirmwareVersion();

            loop.exec();

            disconnect(conn);

            if((!major2) && (!minor2) && (!patch2))
            {
                QMessageBox::critical(Core::ICore::dialogParent(),
                    tr("Connect"),
                    tr("Timeout error while getting firmware version!"));

                QMessageBox::warning(Core::ICore::dialogParent(),
                    tr("Connect"),
                    tr("Do not connect while the green light on your OpenMV Cam is on!"));

                if(QMessageBox::question(Core::ICore::dialogParent(),
                    tr("Connect"),
                    tr("Try to connect again?"),
                    QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes)
                == QMessageBox::Yes)
                {
                    CLOSE_RECONNECT_END();
                }

                CLOSE_CONNECT_END();
            }
        }

        // Bootloader /////////////////////////////////////////////////////////

        QFile file(Core::ICore::resourcePath() + QStringLiteral("/firmware/firmware.txt"));

        if(file.open(QIODevice::ReadOnly))
        {
            QRegularExpressionMatch match = QRegularExpression(QStringLiteral("(\\d+)\\.(\\d+)\\.(\\d+)\\s+(\\d+)\\.(\\d+)\\.(\\d+)\\s+(\\S+)")).match(QString::fromLatin1(file.readAll()));

            if(forceBootloader || ((file.error() == QFile::NoError) && ((major2 < match.captured(1).toInt()) || (minor2 < match.captured(2).toInt()) || (patch2 < match.captured(3).toInt()))))
            {
                QString path = forceBootloader ? forceFirmwarePath : QString();

                if(!forceBootloader)
                {
                    if((major2 <= match.captured(4).toInt()) || (minor2 <= match.captured(5).toInt()) || (patch2 <= match.captured(6).toInt()))
                    {
                        path = Core::ICore::resourcePath() + QStringLiteral("/firmware/") + match.captured(7).replace(QStringLiteral("_"), QStringLiteral(" ")) + QStringLiteral("/firmware.bin");
                    }
                    else
                    {
                        QString arch2 = QString();
                        QString *arch2Ptr = &arch2;

                        QMetaObject::Connection conn = connect(m_iodevice, &OpenMVPluginIO::archString,
                            this, [this, arch2Ptr] (const QString &arch) {
                            *arch2Ptr = arch;
                        });

                        QEventLoop loop;

                        connect(m_iodevice, &OpenMVPluginIO::archString, &loop, &QEventLoop::quit);

                        m_iodevice->getArchString();

                        loop.exec();

                        disconnect(conn);

                        if(!arch2.isEmpty())
                        {
                            QFile boards(Core::ICore::resourcePath() + QStringLiteral("/firmware/boards.txt"));

                            if(boards.open(QIODevice::ReadOnly))
                            {
                                QMap<QString, QString> mappings;
                                QTextStream stream(&boards);

                                while(!stream.atEnd())
                                {
                                    QRegularExpressionMatch mapping = QRegularExpression(QStringLiteral("(\\S+)\\s+(\\S+)\\s+(\\S+)")).match(stream.readLine());
                                    mappings.insert(mapping.captured(1).replace(QStringLiteral("_"), QStringLiteral(" ")), mapping.captured(3).replace(QStringLiteral("_"), QStringLiteral(" ")));
                                }

                                QString value = mappings.value(QRegularExpression(QStringLiteral("(\\S+)")).match(arch2.simplified().replace(QStringLiteral("_"), QStringLiteral(" "))).captured(1));

                                if(!value.isEmpty())
                                {
                                    path = Core::ICore::resourcePath() + QStringLiteral("/firmware/") + value + QStringLiteral("/firmware.bin");
                                }
                            }
                        }
                    }
                }

                if(!path.isEmpty())
                {
                    QFile firmware(path);

                    if(firmware.open(QIODevice::ReadOnly))
                    {
                        QByteArray rawData = firmware.readAll();
                        QList<QByteArray> dataChunks;

                        for(int i = 0; i < rawData.size(); i += 60)
                        {
                            dataChunks.append(rawData.mid(i, qMin(60, rawData.size() - i)));
                        }

                        if((!forceBootloader) && QMessageBox::information(Core::ICore::dialogParent(),
                            tr("Connect"),
                            tr("OpenMV IDE must upgrade your OpenMV Cam's firmware to continue."),
                            QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Ok)
                        != QMessageBox::Ok)
                        {
                            CLOSE_CONNECT_END();
                        }

                        int answer = forceBootloader ? forceFlashFSErase : QMessageBox::question(Core::ICore::dialogParent(),
                            tr("Connect"),
                            tr("Erase internal file system?"),
                            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::No);

                        if((answer != QMessageBox::Yes) && (answer != QMessageBox::No))
                        {
                            CLOSE_CONNECT_END();
                        }

                        int flash_start = (answer == QMessageBox::Yes) ? FLASH_SECTOR_ALL_START : FLASH_SECTOR_START;
                        int flash_end = (answer == QMessageBox::Yes) ? FLASH_SECTOR_ALL_END : FLASH_SECTOR_END;

                        // Send Reset /////////////////////////////////////////

                        if(!forceBootloader)
                        {
                            QEventLoop loop;

                            connect(m_iodevice, &OpenMVPluginIO::closeResponse,
                                    &loop, &QEventLoop::quit);

                            m_iodevice->sysReset();
                            m_iodevice->close();

                            loop.exec();
                        }

                        // Reconnect //////////////////////////////////////////

                        if(!forceBootloader)
                        {
                            QProgressDialog dialog(tr("Reconnecting..."), tr("Cancel"), 0, 0, Core::ICore::dialogParent(),
                                Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
                                (Utils::HostOsInfo::isMacHost() ? Qt::WindowType(0) : Qt::WindowCloseButtonHint));
                            dialog.show();

                            // Wait 100 ms ////////////////////////////////////
                            {
                                QEventLoop eventLoop;

                                QTimer::singleShot(100, &eventLoop, &QEventLoop::quit);

                                eventLoop.exec();
                            }

                            QString errorMessage2 = QString();
                            QString *errorMessage2Ptr = &errorMessage2;

                            QMetaObject::Connection conn = connect(m_ioport, &OpenMVPluginSerialPort::openResult,
                                this, [this, errorMessage2Ptr] (const QString &errorMessage) {
                                *errorMessage2Ptr = errorMessage;
                            });

                            forever
                            {
                                // Always set this each loop...
                                errorMessage2 = tr("Application force quit!");

                                QStringList stringList2;

                                foreach(QSerialPortInfo port, QSerialPortInfo::availablePorts())
                                {
                                    if(port.hasVendorIdentifier() && (port.vendorIdentifier() == OPENMVCAM_VENDOR_ID)
                                    && port.hasProductIdentifier() && (port.productIdentifier() == OPENMVCAM_PRODUCT_ID))
                                    {
                                        stringList2.append(port.portName());
                                    }
                                }

                                if(Utils::HostOsInfo::isMacHost())
                                {
                                    stringList2 = stringList2.filter(QStringLiteral("cu"), Qt::CaseInsensitive);
                                }

                                if(!stringList2.isEmpty())
                                {
                                    QEventLoop loop;

                                    connect(m_ioport, &OpenMVPluginSerialPort::openResult, &loop, &QEventLoop::quit);

                                    emit m_ioport->open(stringList2.contains(selectedPort) ? selectedPort : stringList2.first());

                                    loop.exec();
                                }
                                else
                                {
                                    QApplication::processEvents();

                                    errorMessage2 = tr("No OpenMV Cams found!");
                                }

                                if(errorMessage2.isEmpty() || dialog.wasCanceled())
                                {
                                    break;
                                }

                                // Wait 100 ms ////////////////////////////////
                                {
                                    QEventLoop eventLoop;

                                    QTimer::singleShot(100, &eventLoop, &QEventLoop::quit);

                                    eventLoop.exec();
                                }
                            }

                            disconnect(conn);

                            if(!errorMessage2.isEmpty())
                            {
                                QMessageBox::critical(Core::ICore::dialogParent(),
                                    tr("Connect"),
                                    tr("Error: %L1").arg(errorMessage2));

                                CONNECT_END();
                            }
                        }

                        // Start Bootloader ///////////////////////////////////
                        {
                            bool ok2 = bool();
                            bool *ok2Ptr = &ok2;

                            QMetaObject::Connection conn = connect(m_iodevice, &OpenMVPluginIO::gotBootloaderStart,
                                this, [this, ok2Ptr] (bool ok) {
                                *ok2Ptr = ok;
                            });

                            QEventLoop loop;

                            connect(m_iodevice, &OpenMVPluginIO::gotBootloaderStart,
                                    &loop, &QEventLoop::quit);

                            m_iodevice->bootloaderStart();

                            loop.exec();

                            disconnect(conn);

                            if(!ok2)
                            {
                                QMessageBox::critical(Core::ICore::dialogParent(),
                                    tr("Connect"),
                                    tr("Unable to connect to the bootloader!"));

                                CLOSE_CONNECT_END();
                            }
                        }

                        // Erase Flash ////////////////////////////////////////
                        {
                            QProgressDialog dialog(tr("Erasing..."), tr("Cancel"), flash_start, flash_end, Core::ICore::dialogParent(),
                                Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
                                (Utils::HostOsInfo::isMacHost() ? Qt::WindowType(0) : Qt::WindowCloseButtonHint));
                            dialog.show();

                            for(int i = flash_start; i <= flash_end; i++)
                            {
                                QEventLoop loop;

                                QTimer::singleShot(1000, &loop, &QEventLoop::quit);

                                m_iodevice->flashErase(i);

                                loop.exec();

                                if(dialog.wasCanceled())
                                {
                                    m_iodevice->bootloaderReset();

                                    QMessageBox::warning(Core::ICore::dialogParent(),
                                        tr("Connect"),
                                        tr("Your OpenMV Cam is now bricked - reconnect to unbrick it."));

                                    CLOSE_CONNECT_END();
                                }

                                dialog.setValue(i);
                            }
                        }

                        // Program Flash //////////////////////////////////////
                        {
                            QProgressDialog dialog(tr("Programming..."), tr("Cancel"), 0, dataChunks.size() - 1, Core::ICore::dialogParent(),
                                Qt::WindowTitleHint | Qt::WindowSystemMenuHint |
                                (Utils::HostOsInfo::isMacHost() ? Qt::WindowType(0) : Qt::WindowCloseButtonHint));
                            dialog.show();

                            for(int i = 0; i < dataChunks.size(); i++)
                            {
                                QEventLoop loop;

                                QTimer::singleShot(1, &loop, &QEventLoop::quit);

                                m_iodevice->flashWrite(dataChunks.at(i));

                                loop.exec();

                                if(dialog.wasCanceled())
                                {
                                    m_iodevice->bootloaderReset();

                                    QMessageBox::warning(Core::ICore::dialogParent(),
                                        tr("Connect"),
                                        tr("Your OpenMV Cam is now bricked - reconnect to unbrick it."));

                                    CLOSE_CONNECT_END();
                                }

                                dialog.setValue(i);
                            }
                        }

                        // Reset Bootloader ///////////////////////////////////
                        {
                            QEventLoop loop;

                            connect(m_iodevice, &OpenMVPluginIO::closeResponse,
                                    &loop, &QEventLoop::quit);

                            m_iodevice->bootloaderReset();
                            m_iodevice->close();

                            loop.exec();

                            QMessageBox::information(Core::ICore::dialogParent(),
                                tr("Connect"),
                                tr("Done with upgrading your OpenMV Cam firmware!\n\nPlease wait for your OpenMV Cam to reconnect to your computer before continuing."));

                            RECONNECT_END();
                        }
                    }
                    else if(forceBootloader)
                    {
                        QMessageBox::critical(Core::ICore::dialogParent(),
                            tr("Connect"),
                            tr("Error: %L1").arg(firmware.errorString()));

                        CLOSE_CONNECT_END();
                    }
                }
            }
        }
        else if(forceBootloader)
        {
            QMessageBox::critical(Core::ICore::dialogParent(),
                tr("Connect"),
                tr("Error: %L1").arg(file.errorString()));

            CLOSE_CONNECT_END();
        }

        m_iodevice->scriptStop();
        m_iodevice->fbEnable(!m_disableFrameBuffer->isChecked());
        m_iodevice->jpegEnable(m_jpgCompress->isChecked());
        Core::MessageManager::grayOutOldContent();

        ///////////////////////////////////////////////////////////////////////

        m_connected = true;
        m_portName = selectedPort;

        setSerialPortPath();

        m_resetCommand->action()->setEnabled(true);
        m_connectCommand->action()->setEnabled(false);
        m_connectCommand->action()->setVisible(false);
        m_disconnectCommand->action()->setEnabled(true);
        m_disconnectCommand->action()->setVisible(true);
        Core::IEditor *editor = Core::EditorManager::currentEditor();
        m_startCommand->action()->setEnabled(editor ? (editor->document() ? (!editor->document()->contents().isEmpty()) : false) : false);
        m_startCommand->action()->setVisible(true);
        m_stopCommand->action()->setEnabled(false);
        m_stopCommand->action()->setVisible(false);

        m_portLabel->setEnabled(true);
        m_portLabel->setText(tr("Serial Port: %L1").arg(m_portName));
        m_pathButton->setEnabled(true);
        m_versionLabel->setEnabled(true);
        m_versionLabel->setText(tr("Firmware Version: %L1.%L2.%L3").arg(major2).arg(minor2).arg(patch2));
        m_fpsLabel->setEnabled(true);
        m_fpsLabel->setText(tr("FPS: 0"));

        ///////////////////////////////////////////////////////////////////////

        CONNECT_END();
    }
    else
    {
        QMessageBox::critical(Core::ICore::dialogParent(),
            tr("Connect"),
            tr("Busy... please wait..."));
    }
}

void OpenMVPlugin::disconnectClicked(bool reset)
{
    if(m_connected)
    {
        if(!m_working)
        {
            m_working = true;

            while(m_iodevice->frameSizeDumpQueued() && m_iodevice->getScriptRunningQueued() && m_iodevice->getTxBufferQueued())
            {
                QApplication::processEvents();
            }

            // Closing ////////////////////////////////////////////////////////
            {
                QEventLoop loop;

                connect(m_iodevice, &OpenMVPluginIO::closeResponse,
                        &loop, &QEventLoop::quit);

                m_iodevice->scriptStop();

                if(reset)
                {
                    m_iodevice->sysReset();
                }

                m_iodevice->close();

                loop.exec();
            }

            ///////////////////////////////////////////////////////////////////

            m_connected = false;
            m_portName = QString();
            m_timer.invalidate();
            m_queue.clear();
            m_errorFilterString = QString();

            m_saveCommand->action()->setEnabled(false);
            m_resetCommand->action()->setEnabled(false);
            m_connectCommand->action()->setEnabled(true);
            m_connectCommand->action()->setVisible(true);
            m_disconnectCommand->action()->setVisible(false);
            m_disconnectCommand->action()->setEnabled(false);
            m_startCommand->action()->setEnabled(false);
            m_startCommand->action()->setVisible(true);
            m_stopCommand->action()->setEnabled(false);
            m_stopCommand->action()->setVisible(false);

            m_portLabel->setDisabled(true);
            m_portLabel->setText(tr("Serial Port:"));
            m_pathButton->setDisabled(true);
            m_pathButton->setText(tr("Drive:"));
            m_versionLabel->setDisabled(true);
            m_versionLabel->setText(tr("Firmware Version:"));
            m_fpsLabel->setDisabled(true);
            m_fpsLabel->setText(tr("FPS:"));

            m_frameBuffer->enableSaveTemplate(false);
            m_frameBuffer->enableSaveDescriptor(false);

            ///////////////////////////////////////////////////////////////////

            m_working = false;
        }
        else
        {
            QMessageBox::critical(Core::ICore::dialogParent(),
                reset ? tr("Reset") : tr("Disconnect"),
                tr("Busy... please wait..."));
        }
    }

    QTimer::singleShot(0, this, &OpenMVPlugin::workingDone);
}

void OpenMVPlugin::startClicked()
{
    if(!m_working)
    {
        m_working = true;

        while(m_iodevice->frameSizeDumpQueued() && m_iodevice->getScriptRunningQueued() && m_iodevice->getTxBufferQueued())
        {
            QApplication::processEvents();
        }

        // Stopping ///////////////////////////////////////////////////////////
        {
            long running2 = long();
            long *running2Ptr = &running2;

            QMetaObject::Connection conn = connect(m_iodevice, &OpenMVPluginIO::scriptRunning,
                this, [this, running2Ptr] (long running) {
                *running2Ptr = running;
            });

            QEventLoop loop;

            connect(m_iodevice, &OpenMVPluginIO::scriptRunning,
                    &loop, &QEventLoop::quit);

            m_iodevice->getScriptRunning();

            loop.exec();

            if(running2)
            {
                m_iodevice->scriptStop();
                m_iodevice->getScriptRunning();

                loop.exec();

                Core::MessageManager::grayOutOldContent();
            }

            disconnect(conn);
        }

        ///////////////////////////////////////////////////////////////////////

        m_iodevice->scriptExec(Core::EditorManager::currentEditor()->document()->contents());

        m_timer.start();
        m_queue.clear();

        ///////////////////////////////////////////////////////////////////////

        m_working = false;

        QTimer::singleShot(0, this, &OpenMVPlugin::workingDone);
    }
    else
    {
        QMessageBox::critical(Core::ICore::dialogParent(),
            tr("Start"),
            tr("Busy... please wait..."));
    }
}

void OpenMVPlugin::stopClicked()
{
    if(!m_working)
    {
        m_working = true;

        while(m_iodevice->frameSizeDumpQueued() && m_iodevice->getScriptRunningQueued() && m_iodevice->getTxBufferQueued())
        {
            QApplication::processEvents();
        }

        // Stopping ///////////////////////////////////////////////////////////
        {
            long running2 = long();
            long *running2Ptr = &running2;

            QMetaObject::Connection conn = connect(m_iodevice, &OpenMVPluginIO::scriptRunning,
                this, [this, running2Ptr] (long running) {
                *running2Ptr = running;
            });

            QEventLoop loop;

            connect(m_iodevice, &OpenMVPluginIO::scriptRunning,
                    &loop, &QEventLoop::quit);

            m_iodevice->getScriptRunning();

            loop.exec();

            if(running2)
            {
                m_iodevice->scriptStop();
                m_iodevice->getScriptRunning();

                loop.exec();

                Core::MessageManager::grayOutOldContent();
            }

            disconnect(conn);
        }

        ///////////////////////////////////////////////////////////////////////

        m_working = false;

        QTimer::singleShot(0, this, &OpenMVPlugin::workingDone);
    }
    else
    {
        QMessageBox::critical(Core::ICore::dialogParent(),
            tr("Stop"),
            tr("Busy... please wait..."));
    }
}

void OpenMVPlugin::resetClicked()
{
    disconnectClicked(true);
}

void OpenMVPlugin::processEvents()
{
    if((!m_working) && m_connected)
    {
        if((!m_disableFrameBuffer->isChecked()) && (!m_iodevice->frameSizeDumpQueued()))
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

        if(m_timer.isValid() && m_timer.hasExpired(2000))
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

void OpenMVPlugin::saveScript()
{
    if(!m_working)
    {
        int answer = QMessageBox::question(Core::ICore::dialogParent(),
            tr("Save Script"),
            tr("Strip comments?"),
            QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel, QMessageBox::Yes);

        if((answer == QMessageBox::Yes) || (answer == QMessageBox::No))
        {
            QFile file(QDir::cleanPath(QDir::fromNativeSeparators(getSerialPortPath())) + QStringLiteral("main.py"));

            if(file.open(QIODevice::WriteOnly))
            {
                QByteArray contents = Core::EditorManager::currentEditor()->document()->contents();

                if(answer == QMessageBox::Yes)
                {
                    QString bytes = QString::fromLatin1(contents);

                    bytes.remove(QRegularExpression(QStringLiteral("^\\s*?\n"), QRegularExpression::MultilineOption));
                    bytes.remove(QRegularExpression(QStringLiteral("^\\s*#.*?\n"), QRegularExpression::MultilineOption));
                    bytes.remove(QRegularExpression(QStringLiteral("\\s*#.*?$"), QRegularExpression::MultilineOption));
                    bytes.replace(QStringLiteral("    "), QStringLiteral("\t"));

                    contents = bytes.toLatin1();
                }

                if(file.write(contents) != contents.size())
                {
                    QMessageBox::critical(Core::ICore::dialogParent(),
                        tr("Save Script"),
                        tr("Error: %L1").arg(file.errorString()));
                }
            }
            else
            {
                QMessageBox::critical(Core::ICore::dialogParent(),
                    tr("Save Script"),
                    tr("Error: %L1").arg(file.errorString()));
            }
        }
    }
    else
    {
        QMessageBox::critical(Core::ICore::dialogParent(),
            tr("Save Script"),
            tr("Busy... please wait..."));
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
    if(!m_working)
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
    else
    {
        QMessageBox::critical(Core::ICore::dialogParent(),
            tr("Save Template"),
            tr("Busy... please wait..."));
    }
}

void OpenMVPlugin::saveDescriptor(const QRect &rect)
{
    if(!m_working)
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
    else
    {
        QMessageBox::critical(Core::ICore::dialogParent(),
            tr("Save Descriptor"),
            tr("Busy... please wait..."));
    }
}

QMap<QString, QAction *> OpenMVPlugin::aboutToShowExamplesRecursive(const QString &path, QMenu *parent)
{
    QMap<QString, QAction *> actions;
    QDirIterator it(path, QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);

    while(it.hasNext())
    {
        QString filePath = it.next();

        if(it.fileInfo().isDir())
        {
            QMenu *menu = new QMenu(it.fileName(), parent);
            QMap<QString, QAction *> menuActions = aboutToShowExamplesRecursive(filePath, menu);
            menu->addActions(menuActions.values());
            menu->setDisabled(menuActions.values().isEmpty());
            actions.insertMulti(it.fileName(), menu->menuAction());
        }
        else
        {
            QAction *action = new QAction(it.fileName(), parent);
            QObject::connect(action, &QAction::triggered, this, [this, filePath]
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
                            tr("Cannot open the example file \"%L1\"!").arg(filePath));
                    }
                }
                else
                {
                    QMessageBox::critical(Core::ICore::dialogParent(),
                        tr("Open Example"),
                        tr("Error: %L1").arg(file.errorString()));
                }
            });

            actions.insertMulti(it.fileName(), action);
        }
    }

    return actions;
}

QString OpenMVPlugin::getSerialPortPath()
{
    QSettings *settings = ExtensionSystem::PluginManager::settings();
    settings->beginGroup(QStringLiteral(SERIAL_PORT_SETTINGS_GROUP));
    QString path = settings->value(m_portName).toString();
    settings->endGroup();
    return path;
}

void OpenMVPlugin::setSerialPortPath(bool dialog)
{
    QStringList drives;

    foreach(const QStorageInfo &info, QStorageInfo::mountedVolumes())
    {
        if(info.isValid()
        && info.isReady()
        && (!info.isRoot())
        && (!info.isReadOnly())
        && (QString::fromLatin1(info.fileSystemType()).contains(Utils::HostOsInfo::isMacHost() ? QStringLiteral("msdos") : QStringLiteral("FAT"), Qt::CaseInsensitive)))
        {
            drives.append(info.rootPath());
        }
    }

    QString path;

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
        path = drives.first();
        settings->setValue(m_portName, path);

        if(dialog)
        {
            QMessageBox::information(Core::ICore::dialogParent(),
                tr("Select Drive"),
                tr("\"%L1\" is the best drive for your OpenMV Cam").arg(path));
        }
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
            path = temp;
            settings->setValue(m_portName, path);
        }
    }

    settings->endGroup();

    m_pathButton->setText((!path.isEmpty()) ? tr("Drive: %L1").arg(path) : tr("Drive:"));

    Core::IEditor *editor = Core::EditorManager::currentEditor();
    m_saveCommand->action()->setEnabled((!path.isEmpty()) && (editor ? (editor->document() ? (!editor->document()->contents().isEmpty()) : false) : false));

    m_frameBuffer->enableSaveTemplate(!path.isEmpty());
    m_frameBuffer->enableSaveDescriptor(!path.isEmpty());
}

} // namespace Internal
} // namespace OpenMV
