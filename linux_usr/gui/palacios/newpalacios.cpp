#include <QMessageBox>
#include <QDebug>
#include <QtGui>
#include <QtAlgorithms>

#include "newpalacios.h"
#include "defs.h"

NewPalacios::NewPalacios(QWidget *parent) :
		QMainWindow(parent) {

	// Call UI setup methods
	createWizard();
	createCentralWidget();
	createActions();
	createMenus();
	createToolBars();
	createStatusBar();
	createDockWindows();
       	readExistingVmsFile();

	setWindowTitle(tr(TITLE_MAIN_WINDOW));

        mExitAppFromMenu = false;
}

NewPalacios::~NewPalacios() {
    // Cleanup actions
    delete mExitApp;
    delete mVmNew;
    delete mVmStop;
    delete mVmPause;
    delete mVmStart;
    delete mVmActivate;
    delete mReloadVms;
    delete mAboutApp;

    // Clean up menu
    delete mFileMenu;
    delete mViewMenu;
    delete mVmMenu;
    delete mHelpMenu;

    // Clean up toolbar
    delete mVmToolBar;
    delete mVmCtrlToolBar;

    /*if (mLoadVmsThread != NULL)
        delete mLoadVmsThread;
    if (mAddVmThread != NULL)
        delete mAddVmThread;
    if (mDeleteVmThread != NULL)
        delete mDeleteVmThread;*/

    delete mVmTreeView;
    delete mVmTelemetryView;
    delete mVmWizard;
    delete mVmInfoWidget;

    // Delete the central widget at the end
    delete mVmControlPanel;
}

void NewPalacios::createCentralWidget() {
        // The VM View will be the central widget
        // of this window. The List of VMs will
        // be docked in the left corner
	mVmControlPanel = new QTabWidget();
        mVmControlPanel->setTabsClosable(true);
        connect(mVmControlPanel, SIGNAL(tabCloseRequested(int)), 
                this, SLOT(vmTabClosed(int)));

        mVmInfoWidget = new VmInfoWidget(mVmControlPanel);
        
        // Set the widget to the window
        this->setCentralWidget(mVmControlPanel);
}

void NewPalacios::createActions() {
	mVmNew = new QAction(QIcon(":/images/images/new_vm.png"),
			tr(FILE_MENU_NEW_VM), this);
	connect(mVmNew, SIGNAL(triggered()), this, SLOT(createVmInstance()));

	mExitApp = new QAction(QIcon(":/images/images/exit.png"),
			tr(FILE_MENU_EXIT), this);
	connect(mExitApp, SIGNAL(triggered()), this, SLOT(exitApplication()));

	mVmStart = new QAction(QIcon(":/images/images/start_vm.png"),
			tr(VM_MENU_START), this);
	connect(mVmStart, SIGNAL(triggered()), this, SLOT(selectVmMode()));

	mVmStop = new QAction(QIcon(":/images/images/stop_vm.png"),
			tr(VM_MENU_STOP), this);
        connect(mVmStop, SIGNAL(triggered()), this, SLOT(stopVm()));

	mVmPause = new QAction(QIcon(":/images/images/pause_vm.png"),
			tr(VM_MENU_PAUSE), this);
        connect(mVmPause, SIGNAL(triggered()), this, SLOT(pauseVm()));

	mVmActivate = new QAction(QIcon(":/images/images/activate_vm.png"),
			tr(VM_MENU_ACTIVATE), this);
        connect(mVmActivate, SIGNAL(triggered()), this, SLOT(activateVm()));

	mAboutApp = new QAction(tr(HELP_MENU_ABOUT), this);
	connect(mAboutApp, SIGNAL(triggered()), this, SLOT(aboutPalacios()));
	
	mReloadVms = new QAction(QIcon(":/images/images/reload_vm.png"), tr(VM_MENU_RELOAD), this);
	connect(mReloadVms, SIGNAL(triggered()), this, SLOT(reloadVms()));

	connect(mVmWizard, SIGNAL(accepted()), this, SLOT(addNewVm()));
}

void NewPalacios::createMenus() {
	mFileMenu = menuBar()->addMenu(tr(MENU_FILE));
	mFileMenu->addAction(mVmNew);
	mFileMenu->addSeparator();
	mFileMenu->addAction(mExitApp);

	mViewMenu = menuBar()->addMenu(tr(MENU_VIEW));

	mVmMenu = menuBar()->addMenu(tr(MENU_VM));
	mVmMenu->addAction(mVmStart);
	mVmMenu->addAction(mVmStop);
	mVmMenu->addAction(mVmPause);
	mVmMenu->addAction(mVmActivate);
	mVmMenu->addAction(mReloadVms);

	mHelpMenu = menuBar()->addMenu(tr(MENU_HELP));
	mHelpMenu->addAction(mAboutApp);
}

void NewPalacios::createToolBars() {
	mVmToolBar = addToolBar(tr(MENU_FILE));
	mVmToolBar->addAction(mVmNew);

	mVmCtrlToolBar = addToolBar(tr(MENU_VM));
	mVmCtrlToolBar->addAction(mVmStart);
	mVmCtrlToolBar->addAction(mVmStop);
	mVmCtrlToolBar->addAction(mVmPause);
	mVmCtrlToolBar->addAction(mVmActivate);
	mVmCtrlToolBar->addAction(mReloadVms);
}

void NewPalacios::createStatusBar() {
	statusBar()->showMessage(tr(STATUS_BAR_MSG_READY));
}

void NewPalacios::createDockWindows() {
	QDockWidget* dockVmList = new QDockWidget(tr(TITLE_DOCK_VM_LIST), this);
	       
        // Setup VM instance tree view
        mVmTreeView = new QTreeWidget(dockVmList);
        mVmTreeView->setColumnCount(1);
        mVmTreeView->headerItem()->setHidden(true);
        // Header for active VMs
        QTreeWidgetItem* activeVms = new QTreeWidgetItem(mVmTreeView);
        activeVms->setText(0, tr(LABEL_ACTIVE_INVENTORY));
        mVmTreeView->addTopLevelItem(activeVms);
        
        // Header for inactive VMs in the inventory
        QTreeWidgetItem* inactiveVms = new QTreeWidgetItem(mVmTreeView);
        inactiveVms->setText(0, tr(LABEL_INACTIVE_INVENTORY));
        mVmTreeView->addTopLevelItem(inactiveVms);
        
        // Header for active VMs not in inventory
        QTreeWidgetItem* activeNotInventoryVms = new QTreeWidgetItem(mVmTreeView);
        activeNotInventoryVms->setText(0, tr(LABEL_ACTIVE_NOT_INVENTORY));
        mVmTreeView->addTopLevelItem(activeNotInventoryVms);
        
        mVmTreeView->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(mVmTreeView, SIGNAL(customContextMenuRequested(const QPoint &)),
                    this, SLOT(vmContextMenu(const QPoint &)));
        connect(mVmTreeView, SIGNAL(itemClicked(QTreeWidgetItem*, int)), this,
                    SLOT(vmItemClickListener(QTreeWidgetItem*, int)));

        dockVmList->setAllowedAreas(Qt::LeftDockWidgetArea);
	dockVmList->setFeatures(QDockWidget::DockWidgetClosable);
	dockVmList->setWidget(mVmTreeView);
	addDockWidget(Qt::LeftDockWidgetArea, dockVmList);
	mViewMenu->addAction(dockVmList->toggleViewAction());
	
	// Telemetry dock
	QDockWidget* dockTelemetry = new QDockWidget(tr(TITLE_DOCK_TELEMETRY), this);
	mVmTelemetryView = new QTextEdit(dockTelemetry);
	mVmTelemetryView->setReadOnly(true);
	dockTelemetry->setAllowedAreas(Qt::BottomDockWidgetArea);
	dockTelemetry->setFeatures(QDockWidget::NoDockWidgetFeatures);
	dockTelemetry->setWidget(mVmTelemetryView);
	addDockWidget(Qt::BottomDockWidgetArea, dockTelemetry);
	connect(dockTelemetry, SIGNAL(visibilityChanged(bool)), this, SLOT(updateTelemetry(bool)));
}

void NewPalacios::updateTelemetry(bool visible) {
	if (visible) {
		mTelemProc = new QProcess();
		mTelemProc->setProcessChannelMode(QProcess::MergedChannels);
		QStringList args;
		args << "-c" << "tail -f /var/log/messages";
		
		// Slots used for debugging
                //connect(mTelemProc, SIGNAL(started()), this, SLOT(processStarted()));
                //connect(mTelemProc, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(processExit(int, QProcess::ExitStatus)));
                //connect(mTelemProc, SIGNAL(error(QProcess::ProcessError)), this, SLOT(processError(QProcess::ProcessError)));
                
                // Connect output of dmesg to text widget
                connect(mTelemProc, SIGNAL(readyReadStandardOutput()), this, SLOT(updateTelemetryView()));
                
                mTelemProc->start("sh", args);
                if (!mTelemProc->waitForStarted()) {
                	if (mVmTelemetryView != NULL) {
                		mVmTelemetryView->setText(tr(ERROR_TELEMETRY));
                	}
                }
        } else {
                /*if (mTelemProc != NULL) {
	                mTelemProc->close();
        	        mTelemProc->terminate();
                        delete mTelemProc;
                }*/
        }
}

void NewPalacios::updateTelemetryView() {
	if (mVmTelemetryView != NULL && mTelemProc != NULL) {
		mVmTelemetryView->setText(mTelemProc->readAllStandardOutput());
	}
}

void NewPalacios::createWizard() {
	mVmWizard = new NewVmWizard();
}

// Listener for VM item clicks
void NewPalacios::vmItemClickListener(QTreeWidgetItem* item, int col) {
        if (item->text(0).compare(LABEL_ACTIVE_INVENTORY) == 0
                || item->text(0).compare(LABEL_INACTIVE_INVENTORY) == 0
                || item->text(0).compare(LABEL_ACTIVE_NOT_INVENTORY) == 0) {
                // If user clicks on the main headers
                // do nothing
                isHeaderClicked = true;
                return;
        }
        
        isHeaderClicked = false;

	QString vmName = item->text(col);
        mVmName = vmName;
        
        // Locate the VM clicked in the list. This is inefficient because each time we need 
        // to search the list. If a better way is found. replace here
	VmInfo* vm = NULL;
	for (int i = 0; i < mVmList.size(); i++) {
		if (vmName.compare(mVmList[i]->getVmName()) == 0) {
                        mVmPos = i;
			vm = mVmList[i];
			break;
		}
	}
        // Load the details of the seleted VM
	if (vm != NULL) {
            mVmInfoWidget->updateInfoView(vm);
        }
}

void NewPalacios::vmContextMenu(const QPoint &pos) {
	//QPoint globalPos = mVmListView->mapToGlobal(pos);
	QPoint globalPos = mVmTreeView->mapToGlobal(pos);
        
        QTreeWidgetItem* item = mVmTreeView->itemAt(pos);
        if (item->text(0).compare(LABEL_ACTIVE_INVENTORY) == 0
                || item->text(0).compare(LABEL_INACTIVE_INVENTORY) == 0
                || item->text(0).compare(LABEL_ACTIVE_NOT_INVENTORY) == 0) {
	        // Clicked on the headers
                // do nothing
                isHeaderClicked = true;
                return;
        }
        
        isHeaderClicked = false;
        
        // Update VM pos, the global index locator for VMs
        QString vmName = item->text(0);
        // Update global VM name
        mVmName = vmName;
        for (int i=0; i<mVmList.size(); i++) {
                if (vmName.compare(mVmList[i]->getVmName()) == 0) {
                        mVmPos = i;
                        break;
                }
        }
        
        QMenu *menu = new QMenu();
        if (mVmList[mVmPos]->getCategory() == VmInfo::INACTIVE_INVENTORY
        	|| mVmList[mVmPos]->getCategory() == VmInfo::ACTIVE_NOT_INVENTORY) {
        	
                menu->addAction(new QAction(tr(VM_MENU_ACTIVATE), this));
        } else {
                if (mVmList[mVmPos]->getState() == VmInfo::RUNNING) {
                        menu->addAction(new QAction(tr(VM_MENU_PAUSE), this));

                } else if (mVmList[mVmPos]->getState() == VmInfo::PAUSED) {
                        menu->addAction(new QAction(tr(VM_MENU_RESTART), this));
        
                } else if (mVmList[mVmPos]->getState() == VmInfo::STOPPED) {
                        menu->addAction(new QAction(tr(VM_MENU_START), this));
	
                }
 
                menu->addAction(new QAction(tr(VM_MENU_STOP), this));
       }
        
        menu->addAction(new QAction(tr(VM_MENU_REMOVE), this));
        
        QAction* selectedAction = menu->exec(globalPos);
        if (selectedAction == NULL) {
        	// User did not select any option
                return;
        }
	
        QString actionItem = selectedAction->text();
	if (actionItem.compare(tr(VM_MENU_START)) == 0
                    || actionItem.compare(tr(VM_MENU_RESTART)) == 0) {
                // If VM was stopped or paused
                selectVmMode();

	} else if (actionItem.compare(tr(VM_MENU_PAUSE)) == 0) {
                // Pause VM
                pauseVm();

        } else if (actionItem.compare(tr(VM_MENU_RESTART)) == 0) {
                // Restart VM
                restartVm();
        
        } else if (actionItem.compare(tr(VM_MENU_STOP)) == 0) {
		// Stop the VM if possible
		stopVm();

	} else if (actionItem.compare(tr(VM_MENU_REMOVE)) == 0) {
                bool isVmRunning = false;
                for (int i=0; i < mRunningVms.size(); i++) {
                        if (mVmName.compare(mRunningVms[i]->getVmName()) == 0) {
                                isVmRunning = true;
                                break;
                        }
                }
                
		QMessageBox vmDeleteWarningBox;

		if (isVmRunning) {
			vmDeleteWarningBox.setText(DELETE_RUNNING_VM_ERROR);
			vmDeleteWarningBox.setIcon(QMessageBox::Critical);
		} else {
			vmDeleteWarningBox.setText(VM_DELETE_WARNING_MESSAGE);
			vmDeleteWarningBox.setIcon(QMessageBox::Warning);
		}

		vmDeleteWarningBox.setStandardButtons(
				QMessageBox::Ok | QMessageBox::Cancel);
		vmDeleteWarningBox.setDefaultButton(QMessageBox::Cancel);
		int retVal = vmDeleteWarningBox.exec();

		switch (retVal) {
		case QMessageBox::Ok:
			if (!isVmRunning) {
				deleteVm(mVmName);
			}
			break;
		case QMessageBox::Cancel:
			break;
		}
		
	} else if (actionItem.compare(tr(VM_MENU_ACTIVATE)) == 0) {
                activateVm();
                return;
        }

}

void NewPalacios::aboutPalacios() {
	QMessageBox::about(this, tr(HELP_MENU_ABOUT), tr(ABOUT_PALACIOS));
}

// This function checks for the necessary setup
// required by palacios to run. The setup for palacios
// includes:
// 1. Check for v3vee.ko module
// 2. Check if palacios has been allocated memory
void NewPalacios::checkPalacios() {
	int err = 0;
        QMessageBox setupError;
        setupError.setStandardButtons(QMessageBox::Ok);
        setupError.setIcon(QMessageBox::Critical);
        
        // v3vee.ko check
        QProcess* v3 = new QProcess();
        v3->setProcessChannelMode(QProcess::MergedChannels);
        v3->start("lsmod");
        v3->waitForFinished();
        QByteArray reply = v3->readAllStandardOutput();

        if (reply.isEmpty() || !reply.contains("v3vee")) {
       		setupError.setText(tr(ERROR_SETUP_MODULE_INSTALL));
                err = -1;
        }
        
        QFile file("/proc/v3vee/v3-mem");
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                setupError.setText(tr(ERROR_SETUP_MODULE_INSTALL_FAILED));
                err = -1;
        }

        QTextStream in(&file);

        while(in.atEnd()) {
                QString line = in.readLine();
                if (line.contains("null")) {
                        setupError.setText(tr(ERROR_SETUP_MEMORY));
                        err = -1;
                        break;
                }
        }
        
	if (err != 0) {
		int ret = setupError.exec();
		
		if (ret == QMessageBox::Ok) {
			// Palacios not setup correctly, exit
			this->close();
		}
	}
}

void NewPalacios::closeEvent(QCloseEvent* event) {
        if (mExitAppFromMenu == true) {
            event->accept();
            return;
        }

        bool flag = false;

        for (int i=0; i<mVmList.size(); i++) {
                if (mVmList[i]->getState() == VmInfo::RUNNING) {
                        flag = true;
                        break;
                }
        }

        if (!flag) {
        	if (mTelemProc != NULL) {
	        	mTelemProc->close();
        		mTelemProc->terminate();
                        delete mTelemProc;
        	}
                event->accept();
        } else {
                
                QMessageBox appCloseWarning;
                appCloseWarning.setText(tr("There are still running VMs. It is suggested to close the running VMs tab before exiting. Click cancel to go back"));
                appCloseWarning.setIcon(QMessageBox::Warning);
                appCloseWarning.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);

                int ret = appCloseWarning.exec();
                switch (ret) {
                case QMessageBox::Ok:
                    event->accept();
                    break;
                case QMessageBox::Cancel:
                    event->ignore();
                    break;
                }
        }
}

void NewPalacios::exitApplication() {
        QMessageBox appCloseWarning;
        appCloseWarning.setText(tr("There are still running VMs. It is suggested to close the running VMs tab before exiting. Click cancel to go back"));
        appCloseWarning.setIcon(QMessageBox::Warning);
        appCloseWarning.setStandardButtons(QMessageBox::Ok | QMessageBox::Cancel);

        bool flag = false;
        
        // Check if there are running VMs
        for (int i=0; i<mVmList.size(); i++) {
                if (mVmList[i]->getState() == VmInfo::RUNNING) {
                        flag = true;
                        break;
                }
        }
        
        // If there are no running VMs
        // exit
        if (!flag) {
        	if (mTelemProc != NULL) {
        		mTelemProc->close();
        		mTelemProc->terminate();
                        delete mTelemProc;
        	}
                mExitAppFromMenu = true;
                this->close();
        } else {
                // Inform user about running VMs and ask for consent
                // before closing application
                int ret = appCloseWarning.exec();
                switch (ret) {
                case QMessageBox::Ok:
                    mExitAppFromMenu = true;
                    this->close();
                    break;
                case QMessageBox::Cancel:
                    mExitAppFromMenu = false;
                    break;
                }
       }
}

// Convenience method for showing messages
void NewPalacios::showMessage(QString msg, bool err, bool warning) {
        QMessageBox msgBox;
        msgBox.setText(msg);
        if (err == true) {
                msgBox.setIcon(QMessageBox::Critical);
        } else if (warning == true) {
	        msgBox.setIcon(QMessageBox::Warning);
        }
        
        msgBox.setStandardButtons(QMessageBox::Ok);
        msgBox.exec();
}

/*void NewPalacios::processStarted() {
	//qDebug() << "Process started...";
}

void NewPalacios::processExit(int errorCode, QProcess::ExitStatus exitStatus) {
	if (exitStatus == QProcess::CrashExit) {
		//qDebug() << "The process crashed!!";
	} else {
		//qDebug() << "The process exited normally..";
	}
}

void NewPalacios::processError(QProcess::ProcessError error) {
	//qDebug() << "There was an error in the process execution...";
	
	switch (error) {
	case QProcess::FailedToStart:
		//qDebug() << "Process failed to start...";
		break;
	case QProcess::Crashed:
		//qDebug() << "Process crashed...";
		break;
	case QProcess::Timedout:
		//qDebug() << "Process timed out...";
		break;
	case QProcess::WriteError:
		//qDebug() << "Process had write error...";
		break;
	case QProcess::ReadError:
		//qDebug() << "Process had read error...";
		break;
	case QProcess::UnknownError:
		//qDebug() << "Process unknown error...";
		break;

	}
}*/

void NewPalacios::createVmInstance() {
	if (mVmWizard != NULL) {
		mVmWizard->restart();
		mVmWizard->show();
	}
}

void NewPalacios::selectVmMode() {
        if (isHeaderClicked) {
                return;
        }
        
        if (mVmList[mVmPos]->getCategory() != VmInfo::ACTIVE_INVENTORY) {
                QMessageBox warning;
                showMessage(tr("VM is not active!"), true);
                return;
        }
           

        if (mVmList[mVmPos]->getState() == VmInfo::PAUSED) {
                // If machine is paused, we just restart using the previous selected mode
                restartVm();
        } else {
                // Machine is started fresh
                mVmModeDialog = new VmModeDialog(this);
                connect(mVmModeDialog, SIGNAL(setMode(int, QString)), 
        	        this, SLOT(getVmMode(int, QString)));
                mVmModeDialog->show();
        }
}

void NewPalacios::getVmMode(int mode, QString streamName) {
        mVmMode = mode;
        mStreamName = streamName;
        // Start VM
        startVm();
}

void NewPalacios::startVm() {
        if (mVmList.isEmpty()) {
                // There is no VM in the list
                return;
        }
        
        // Check if we are trying to start an inactive VM
        if (mVmList[mVmPos]->getCategory() == VmInfo::INACTIVE_INVENTORY) {
                showMessage(tr(ERROR_RUN_INACTIVE_INVENTORY), true);
                return;
        }
        
        // Check if we are trying to start a VM not in the inventory
        if (mVmList[mVmPos]->getCategory() == VmInfo::ACTIVE_NOT_INVENTORY) {
                showMessage(tr(ERROR_RUN_ACTIVE_NOT_INVENTORY), true);
                return;
        }

        int pos = 0;
        // Check if the VM is already running        
        for (int i=0; i < mRunningVms.size(); i++) {
                if (mVmName.compare(mRunningVms[i]->getVmName()) == 0 
                        && mVmMode != VmConsoleWidget::STREAM) {
                        // If we are running the VM in stream mode then we
                        // can have multiple streams open simultaneously
                        // For the other modes, only one instance can be running
                        showMessage(tr(ERROR_VM_RUNNING), true);
                        return;
                }
        }
	
	QString v3_devfile = mVmList[mVmPos]->getVmDevFile();
        if (v3_devfile == NULL) {
                showMessage(tr(ERROR_NO_DEVFILE_FOR_LAUNCH), true);
                return;
        }
        
        int vm_status = mVmList[mVmPos]->getState();
        
        QProcess* v3LaunchProc = NULL;
        QStringList args;
        bool flag = false;
        QByteArray message;

        switch (vm_status) {
            case VmInfo::STOPPED:
                // If VM is stopped, launch it <v3_launch>
                v3LaunchProc = new QProcess();
                v3LaunchProc->setProcessChannelMode(QProcess::MergedChannels);
                
                // Connect debug slots
                //connect(v3LaunchProc, SIGNAL(started()), this, SLOT(processStarted()));
                //connect(v3LaunchProc, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(processExit(int, QProcess::ExitStatus)));
                //connect(v3LaunchProc, SIGNAL(error(QProcess::ProcessError)), this, SLOT(processError(QProcess::ProcessError)));
                
                args << v3_devfile;
                v3LaunchProc->start("v3_launch", args);

                flag = v3LaunchProc->waitForFinished();
		message = v3LaunchProc->readAllStandardOutput();

                if (!flag) {
                        showMessage(tr(ERROR_VM_LAUNCH), true);
                        delete v3LaunchProc;
                        return;
                        
                } else if (message.contains("Error opening V3Vee VM device")) {
                        showMessage(tr(ERROR_LAUNCH_VM_DEVICE_NOT_FOUND), true);
                        delete v3LaunchProc;
                        return;
                	
                } 

                break;
            /*case VmInfo::PAUSED:
                // If VM is paused, call <v3_continue>
                restartVm();
                break;*/
            case VmInfo::RUNNING:
                // If VM is running, do nothing
                // We will be just calling v3_cons_sc/v3_stream/vnc to
                // connect to it
                break;
        }
        
        QString name = mVmList[mVmPos]->getVmName();
        // Create a new console instance and set the launch file
        VmXConsoleParent* consoleParent = new VmXConsoleParent(name);
        
        // Add it to list of running VMs
        mRunningVms.append(consoleParent);
        pos = mRunningVms.indexOf(consoleParent);
        
        // Launch in new tab
        // Disable updates in widgets to reduce screen flicker
        // due to layouting in widgets
        consoleParent->setUpdatesEnabled(false);
        mVmControlPanel->setUpdatesEnabled(false);
        if (mVmMode != VmConsoleWidget::STREAM) {
            // If not in stream mode, header will be VM name
            mVmControlPanel->insertTab(mVmControlPanel->count(), consoleParent, name);
        } else {
            // If stream mode, header will be stream name
            mVmControlPanel->insertTab(mVmControlPanel->count(), consoleParent, mStreamName);
        }
        mVmControlPanel->setCurrentWidget(consoleParent);
        
        // Start VM
	consoleParent->showWidget(mVmMode, v3_devfile, mStreamName);
        // Re-enable updates to widgets
        mVmControlPanel->setUpdatesEnabled(true);
        consoleParent->setUpdatesEnabled(true);

        // Signal to tell the background xterm process to quit when console widget is closed
        connect(mRunningVms[pos], SIGNAL(windowClosingWithId(QString)), this,
			SLOT(consoleWindowClosed(QString)));
	// Update VM state
        updateVmState(VmInfo::RUNNING);
        
        for (int i=0; i<mVmTreeView->topLevelItem(VmInfo::ACTIVE_INVENTORY)->childCount(); i++) {
        	QTreeWidgetItem* child = mVmTreeView->topLevelItem(VmInfo::ACTIVE_INVENTORY)->child(i);
        	if (mVmName.compare(child->text(0)) == 0) {
        		child->setIcon(0, QIcon(":/images/images/start_vm.png"));
        		break;
        	}
        }
}

void NewPalacios::updateVmState(int mode) {
        // TODO: Move this to a background thread
        QString vm_name = mVmList[mVmPos]->getVmName();
        
        QFile file("virtual_machines_list.txt");
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                showMessage(tr(ERROR_UPDATE_VM_STATE), true);
                stopVm();
                return;
        }

        QFile temp("temp.txt");
        if (!temp.open(QIODevice::WriteOnly | QIODevice::Text)) {
                showMessage(tr(ERROR_UPDATE_VM_STATE), true);
                stopVm();
                return;
        }

        QTextStream in(&file);
        QTextStream out(&temp);

        while (!in.atEnd()) {
                QString line = in.readLine();
                if (line.compare("\n") == 0) {
                	continue;
                }
                
                QStringList vmDet = line.split(",");
                QString nameStr = vmDet.at(0);

                if (nameStr.compare(vm_name) == 0) {
                        QString configStr = vmDet.at(1);
                        QString devfileStr = vmDet.at(2);
                        QString stateStr = vmDet.at(3);
                        QString imageStr = vmDet.at(4);

                        out << nameStr << "," << configStr << "," << devfileStr << "," << QString::number(mode) <<"," << imageStr << endl;
                } else {
                        out << line << endl;
                }
        }

        out.flush();
        file.remove("virtual_machines_list.txt");
        temp.rename("virtual_machines_list.txt");

        file.close();
        temp.close();

        // Update VM object
        mVmList[mVmPos]->setState(mode);
}

void NewPalacios::stopVm() {
        if (mVmList.isEmpty() 
                || mVmList[mVmPos]->getState() == VmInfo::STOPPED) {
                // If the VM list is empty or the VM is already stopped
                // or if the VM is passive
                // return
                showMessage(tr(ERROR_STOP_VM), false, true);
                return;
        }

        if (mVmList[mVmPos]->getCategory() == VmInfo::INACTIVE_INVENTORY
        	|| mVmList[mVmPos]->getCategory() == VmInfo::ACTIVE_NOT_INVENTORY) {
                // If the VM is inactive or active not inventory
                showMessage(tr(ERROR_VM_NOT_INVENTORY), true);
                return;
        }

	QString name = NULL;
	QMessageBox vmStopWarningBox;
	vmStopWarningBox.setText(VM_STOP_WARNING_MESSAGE);
	vmStopWarningBox.setIcon(QMessageBox::Warning);
	vmStopWarningBox.setStandardButtons(
			QMessageBox::Cancel | QMessageBox::Ok);
	vmStopWarningBox.setDefaultButton(QMessageBox::Cancel);
	int ret = vmStopWarningBox.exec();
        int posInTabWidget = -1;
        QStringList args;
        QProcess* v3StopProc = NULL;

	switch (ret) {
	case QMessageBox::Ok:
                
                v3StopProc = new QProcess();
                v3StopProc->setProcessChannelMode(QProcess::MergedChannels);
                args << mVmList[mVmPos]->getVmDevFile();
                v3StopProc->start("v3_stop", args);
                if (!v3StopProc->waitForFinished()) {
                    showMessage(tr(ERROR_STOP_VM_PATH), true);
                    delete v3StopProc;
                    return;

                } else if (v3StopProc->readAllStandardOutput().contains("Error opening V3Vee VM device")) {
                    showMessage(tr(ERROR_STOP_VM_DEVICE_NOT_FOUND), true);
                    delete v3StopProc;
                    return;
                }

		name = mVmList[mVmPos]->getVmName();
		for (int i=0; i<mRunningVms.length(); i++) {
	     		if (name.compare(mRunningVms[i]->getVmName()) == 0) {
	                        // Remove widget from tab if placed there
	                        posInTabWidget = mVmControlPanel->indexOf(mRunningVms[i]);
                                if (posInTabWidget != -1) {
	                                // Console is present in tab
                                        mVmControlPanel->removeTab(posInTabWidget);
	                        }
                                // Close the console
             			mRunningVms[i]->close();
                                // Remove it from list of running console windows
              			mRunningVms.removeAt(i);
               			break;
               		}
         	}

                // Update VM state
                updateVmState(VmInfo::STOPPED);
                
                // Update icon
                for (int i=0; i<mVmTreeView->topLevelItem(VmInfo::ACTIVE_INVENTORY)->childCount(); i++) {
        		QTreeWidgetItem* child = mVmTreeView->topLevelItem(VmInfo::ACTIVE_INVENTORY)->child(i);
	        	if (mVmName.compare(child->text(0)) == 0) {
        			child->setIcon(0, QIcon(":/images/images/stop_vm.png"));
        			break;
        		}
        	}
                
		break;
	case QMessageBox::Cancel:
		break;
	}
}

void NewPalacios::pauseVm() {
    if (mVmList.isEmpty()) {
       return;
    }
	
    if (mVmList[mVmPos]->getCategory() == VmInfo::INACTIVE_INVENTORY
    	|| mVmList[mVmPos]->getCategory() == VmInfo::ACTIVE_NOT_INVENTORY) {
        showMessage(tr(ERROR_VM_NOT_INVENTORY), false, true);
        return;
    }
	
    QString v3_devfile = mVmList[mVmPos]->getVmDevFile();
    if (v3_devfile == NULL) {
            showMessage(tr("Device file not found"), true);
            return;
    }

    QProcess* v3Pauseproc = new QProcess();
    v3Pauseproc->setProcessChannelMode(QProcess::MergedChannels);
    QStringList args;
    args << v3_devfile;
    v3Pauseproc->start("v3_pause", args);
    if (!v3Pauseproc->waitForFinished()) {
        showMessage(v3Pauseproc->errorString(), true);
    } else if (v3Pauseproc->readAllStandardOutput().contains("Error opening V3Vee VM device")) {
        showMessage(tr(ERROR_PAUSE_VM_DEVICE_NOT_FOUND), true);
    }
        
    // Update VM state
    updateVmState(VmInfo::PAUSED);
    
    // Update icon
    for (int i=0; i<mVmTreeView->topLevelItem(VmInfo::ACTIVE_INVENTORY)->childCount(); i++) {
       	QTreeWidgetItem* child = mVmTreeView->topLevelItem(VmInfo::ACTIVE_INVENTORY)->child(i);
       	if (mVmName.compare(child->text(0)) == 0) {
       		child->setIcon(0, QIcon(":/images/images/pause_vm.png"));
       		break;
       	}
    }
    
    delete v3Pauseproc;
}

void NewPalacios::restartVm() {
    if (mVmList.isEmpty()) {
       return;
    }

    if (mVmList[mVmPos]->getCategory() == VmInfo::INACTIVE_INVENTORY
    	|| mVmList[mVmPos]->getCategory() == VmInfo::ACTIVE_NOT_INVENTORY) {
        showMessage(tr(ERROR_VM_NOT_INVENTORY), false, true);
        return;
    }

    QString v3_devfile = mVmList[mVmPos]->getVmDevFile();
    if (v3_devfile == NULL) {
            showMessage(tr("Device file not found"), true);
            return;
    }

    QProcess* v3Continueproc = new QProcess();
    v3Continueproc->setProcessChannelMode(QProcess::MergedChannels);
    QStringList args;
    args << v3_devfile;
    v3Continueproc->start("v3_continue", args);
    
    if (!v3Continueproc->waitForFinished()) {
        showMessage(v3Continueproc->errorString(), true);
    } else if (v3Continueproc->readAllStandardOutput().contains("Error opening V3Vee VM device")) {
        showMessage(tr(ERROR_RESTART_VM_IOCTL), true);
    }

    // Update icon
    for (int i=0; i<mVmTreeView->topLevelItem(VmInfo::ACTIVE_INVENTORY)->childCount(); i++) {
       	QTreeWidgetItem* child = mVmTreeView->topLevelItem(VmInfo::ACTIVE_INVENTORY)->child(i);
       	if (mVmName.compare(child->text(0)) == 0) {
       		child->setIcon(0, QIcon(":/images/images/start_vm.png"));
       		break;
       	}
    }

    delete v3Continueproc;
}

// Method to create a VM from inactive list
int NewPalacios::createInactiveVm() {
	QString vmImageFile = mVmList[mVmPos]->getImageFile();
	QString vmName = mVmList[mVmPos]->getVmName();
	
	// Create the VM instance
	QProcess* v3CreateProc = new QProcess();
        QStringList args;
        args.clear();
        args << vmImageFile << vmName;
        v3CreateProc->start("v3_create", args);
                
        if (v3CreateProc->waitForFinished()) {
        	if (v3CreateProc->readAllStandardOutput().contains("Error (-1)")) {
        	        // v3_create has failed with IOCTL error
                        showMessage(tr(ERROR_VM_CREATE_IOCTL), true);
                        delete v3CreateProc;
        		return -1;
                }
	} else {
		showMessage(tr(ERROR_VM_CREATE_PATH), true);
                delete v3CreateProc;
                return -1;
	}
	
	// Cleanup
	delete v3CreateProc;
	
	// Check the last line of /proc/v3vee/v3-guests 
        // to see the /dev/v3-vm# of the new VM
        bool isCreated = false;
        QProcess* proc = new QProcess();
        proc->setProcessChannelMode(QProcess::MergedChannels);
        proc->start("cat /proc/v3vee/v3-guests");
                
        if (!proc->waitForFinished()) {
        	showMessage(tr(ERROR_VM_CREATE_PROC), true);
                delete proc;
                return -1;
	}
	
        //QByteArray temp = vmName.toAscii();
        //const char* vmEntry = temp.data();
        
        // Read standard output of process
        QByteArray val = proc->readAllStandardOutput();
        if (!val.isNull()) {
                // The created VM can be defined anywhere in the /proc file
                // we need to go over all entries till we find it
                QList<QByteArray> procs = val.split('\n');
                for (int i=0; i<procs.size(); i++) {
                	QList<QByteArray> temp = procs[i].split('\t');
                        QByteArray a = temp.at(0);
                        QString vmProcName(a);
                        if (vmName.compare(vmProcName) == 0) {
                               	// We have found the VM, get dev file name
                               	QByteArray b = temp.at(1);
                               	QString vmDevFile(b);
                                vmDevFile.remove(QChar('\n'), Qt::CaseInsensitive);
        	                mVmList[mVmPos]->setVmDevFile(vmDevFile);
        	                // Make VM instance active
        	                mVmList[mVmPos]->setCategory(VmInfo::ACTIVE_INVENTORY);
        	                isCreated = true;
        	                break;
                       }
                }
        }
                
        if (!isCreated) {
        	// We did not find an entry in
                // /proc file so there must have
                // been an error in creation
                showMessage(tr(ERROR_VM_CREATE_FAILED), true);
                delete proc;
                return -1;
        }
        
        if (proc != NULL) 
            delete proc;

	return 0;
}

int NewPalacios::updateDb(int cat) {
	QString vm_name = mVmList[mVmPos]->getVmName();
        
        QFile file("virtual_machines_list.txt");
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                showMessage("Error opening DB! Please restart application", true);
                return -1;
        }

        QFile temp("temp.txt");
        if (!temp.open(QIODevice::WriteOnly | QIODevice::Text)) {
                showMessage("Error opening DB! Please restart application", true);
                return -1;
        }
        
        QTextStream in(&file);
        QTextStream out(&temp);
        

        if (cat == VmInfo::INACTIVE_INVENTORY) {
                // If we are updating an inactive entry
                // search the file and update
                while (!in.atEnd()) {
                        QString line = in.readLine();
                        if (line.compare("\n") == 0) {
                        	continue;
                        }
                        QStringList vmDet = line.split(",");
                        QString nameStr = vmDet.at(0);
                        
                        if (vm_name.compare(nameStr) != 0) {
                                // If the name does not match, then copy to new place
                                out << line << endl;
                        } else {
                        	// Update DB state
                        	VmInfo* v = mVmList[mVmPos];
                        	QString updateLine = v->getVmName()
                        		+ "," + v->getVmConfigFile()
                        		+ "," + v->getVmDevFile()
                        		+ "," + QString::number(VmInfo::STOPPED)
                        		+ "," + v->getImageFile();
                        	out << updateLine << endl;
                        }
                }
        } else if (cat == VmInfo::ACTIVE_NOT_INVENTORY) {
        	
        	while (!in.atEnd()) {
                        QString line = in.readLine();
                        if (line.compare("\n") == 0) {
                        	continue;
                        }
                        
                        // Copy all the contents into new file
                        out << line << endl;
                }
                
                // Copy the new VM info
                VmInfo* vv = mVmList[mVmPos];
                QString updateLine = vv->getVmName()
                	+ "," + vv->getVmConfigFile()
                	+ "," + vv->getVmDevFile()
                	+ "," + QString::number(VmInfo::STOPPED)
                	+ "," + vv->getImageFile();
                	out << updateLine << endl;
        }

	out.flush();
        file.remove("virtual_machines_list.txt");
        temp.rename("virtual_machines_list.txt");

        file.close();
        temp.close();
        
        return 0;
}

void NewPalacios::activateVm() {
        // 1. Check VM category
        // 	1a. If VM is active but not in inventory, upgrade category
        // 	1b. If VM is inactive, create VM and upgrade category
	QTreeWidgetItem* item = NULL;
	
        // Remove VM from inactive list
        // The list could be from either the inactive inventory or
        // active not inventory
        int category = mVmList[mVmPos]->getCategory();
        
        if (category == VmInfo::ACTIVE_NOT_INVENTORY) {
        	// The VM exists in the /proc file and 
        	// is already created. We will only update
        	// VM category.
        	
        	// Ask user for Config file and Image file
        	QString configFile = QFileDialog::getOpenFileName(this, tr("Select config file"), ".",
			"XML file (*.xml)");
		if (configFile == NULL) {
			// User pressed cancel
			return;
		}
		QString imageFile = QFileDialog::getOpenFileName(this, tr("Select image file"), ".",
			"Image file (*.img *.bZ)");
		if (imageFile == NULL) {
			// User pressed cancel
			return;
		}
		
		mVmList[mVmPos]->setVmConfigFile(configFile);
		mVmList[mVmPos]->setImageFile(imageFile);
		
                // Remove from active not inventory list
        	for (int i=0; i<mVmTreeView->topLevelItem(VmInfo::ACTIVE_NOT_INVENTORY)->childCount(); i++) {
                	item = mVmTreeView->topLevelItem(VmInfo::ACTIVE_NOT_INVENTORY)->child(i);
                	if (item->text(0).compare(mVmList[mVmPos]->getVmName()) == 0) {
                        	item = mVmTreeView->topLevelItem(VmInfo::ACTIVE_NOT_INVENTORY)->takeChild(i);
                        	break;
                	}
        	}
        	
        	// Add it to active list
        	QTreeWidgetItem* subParent = mVmTreeView->topLevelItem(VmInfo::ACTIVE_INVENTORY);
                subParent->addChild(item);
        	
        	// Update Category
        	mVmList[mVmPos]->setCategory(VmInfo::ACTIVE_INVENTORY);
        	
        	// Update DB with newly created entry
                updateDb(VmInfo::ACTIVE_NOT_INVENTORY);
        	return;
        	
        } else if (category == VmInfo::INACTIVE_INVENTORY) {
        	// The VM is in the inventory but inactive.
        	// This means that we need to create it.
        	int ret = createInactiveVm();
        	
        	if (ret != 0) {
        		// VM was not created due to some problem
        		// TODO: Decide if you want to delete the reference
        		// from the text file or keep it there for a while and
        		// try again
        		return;
        	}
        	
        	// Once VM is created, we need to first remove it from the 
        	// inactive list and add it to active list
        	for (int i=0; i<mVmTreeView->topLevelItem(VmInfo::INACTIVE_INVENTORY)->childCount(); i++) {
                	item = mVmTreeView->topLevelItem(VmInfo::INACTIVE_INVENTORY)->child(i);
                	if (item->text(0).compare(mVmList[mVmPos]->getVmName()) == 0) {
                        	item = mVmTreeView->topLevelItem(VmInfo::INACTIVE_INVENTORY)->takeChild(i);
                        	break;
                	}
        	}
        	
        	// Add it to active list
        	QTreeWidgetItem* subParent = mVmTreeView->topLevelItem(VmInfo::ACTIVE_INVENTORY);
                subParent->addChild(item);
                
                // Update DB with newly created entry
                updateDb(VmInfo::INACTIVE_INVENTORY);
        }
}

void NewPalacios::reloadVms() {
	// Clear all previous entries
	for (int i=0; i<mVmTreeView->topLevelItemCount(); i++) {
		qDeleteAll(mVmTreeView->topLevelItem(i)->takeChildren());
	}
	
	// Call load VM thread
	readExistingVmsFile();
}

// This handler is used in serial mode when user clicks
// the close button of the floating window
void NewPalacios::consoleWindowClosed(QString name) {
        // Remove the console from the list of
        // running consoles
        for (int i=0; i < mRunningVms.length(); i++) {
                if (name.compare(mRunningVms[i]->getVmName()) == 0) {
                        mRunningVms.removeAt(i);
                        break;
                }
        }
}

// This handler is used in v3_cons_sc mode when the user clicks
// the close button on the tab widget
void NewPalacios::vmTabClosed(int index) {
        if (index == 0) {
                // As of now do not remove the info tab
                return;
        }

        // Get the name of the VM tab which we need to close
        // Need to cast the return value of TabWidget->widget as it returns an object
        // of the base class. static_cast is used because the conversion is from base
        // class object to derived class object
        VmXConsoleParent* widget = static_cast<VmXConsoleParent*>(mVmControlPanel->widget(index));
        QString name = widget->getVmName();
        for (int i=0; i<mRunningVms.length(); i++) {
                if (name.compare(mRunningVms[i]->getVmName()) == 0) {
                        // Remove the tab
                        mVmControlPanel->removeTab(index);
                        // Remove the console from running vm instances
                        mRunningVms.removeAt(i);
                        // Send close event to widget
                        widget->close();
                        break;
                }
        }
}

/* This function reads from the vm list file */
void NewPalacios::readExistingVmsFile() {
	mThread = new QThread();
	mLoadVmsThread = new LoadVmsThread();
	mLoadVmsThread->moveToThread(mThread);
	connect(mThread, SIGNAL(started()), mLoadVmsThread, SLOT(loadVms()));
	connect(mLoadVmsThread, SIGNAL(finished()), mThread, SLOT(quit()));
	// Action which listens to completion of initial VM loading
	connect(mLoadVmsThread, SIGNAL(finished()), this,
			SLOT(finishLoadingVmsFromFile()));
	mThread->start();
}

// Update UI with VMs loaded from file
void NewPalacios::finishLoadingVmsFromFile() {
	if (mLoadVmsThread->getStatus() == LoadVmsThread::STATUS_OK) {
		mVmList.clear();
		mVmPos = 0;
		
		int category = -1;
		
		// Load VMs into memory
		mVmList.append(mLoadVmsThread->getVmList());
		
                QTreeWidgetItem* item = NULL;
                for (int i = 0; i < mVmList.size(); i++) {
                        if (mVmList[i]->getCategory() == VmInfo::ACTIVE_INVENTORY) {
                               item = new QTreeWidgetItem();
                               item->setText(0, mVmList[i]->getVmName());
                               
                               category = mVmList[i]->getState();
                               switch (category) {
                               case VmInfo::STOPPED:
	                               	item->setIcon(0, QIcon(":/images/images/stop_vm.png"));
                               		break;
                               case VmInfo::PAUSED:
                             		item->setIcon(0, QIcon(":/images/images/pause_vm.png"));
                               		break;
                               case VmInfo::RUNNING:
	                                item->setIcon(0, QIcon(":/images/images/start_vm.png"));
                               		break;
                               }
                               
                               mVmTreeView->topLevelItem(VmInfo::ACTIVE_INVENTORY)->addChild(item);
                               
                        } else if (mVmList[i]->getCategory() == VmInfo::INACTIVE_INVENTORY) {
                               item = new QTreeWidgetItem();
                               item->setText(0, mVmList[i]->getVmName()); 
                               mVmTreeView->topLevelItem(VmInfo::INACTIVE_INVENTORY)->addChild(item);
                               
                        } else if (mVmList[i]->getCategory() == VmInfo::ACTIVE_NOT_INVENTORY) {
                               item = new QTreeWidgetItem();
                               item->setText(0, mVmList[i]->getVmName()); 
                               mVmTreeView->topLevelItem(VmInfo::ACTIVE_NOT_INVENTORY)->addChild(item);
                        }
		}
                
                mVmControlPanel->insertTab(0, mVmInfoWidget, tr(VM_TAB_TITLE));

                if (mThread != NULL)
                    delete mThread;

	}
}

/* This method is called after completion of the VM wizard
 * It updates the backend vm file with new vm */
void NewPalacios::addNewVm() {
        QString name = mVmWizard->field("guestName").toString();
	QString file = mVmWizard->field("configLoc").toString();
        QFileInfo f(mVmWizard->field("imageLoc").toString());
        QString img = f.fileName();
        
        mThread = new QThread();
	mAddVmThread = new AddVmThread(name, file, img);
	mAddVmThread->moveToThread(mThread);
	connect(mThread, SIGNAL(started()), mAddVmThread, SLOT(addVm()));
	connect(mAddVmThread, SIGNAL(finished()), mThread, SLOT(quit()));
	// Call method on window to update the vm list
	connect(mAddVmThread, SIGNAL(finished()), this, SLOT(updateVmList()));
	mThread->start();
}

/* Update UI with new VM added to list */
void NewPalacios::updateVmList() {
	if(mAddVmThread != NULL && mAddVmThread->getStatus()
                        == AddVmThread::ERROR_V3CREATE_DB) {
                showMessage(tr(ERROR_VM_CREATE_DB), true);

        } else if (mAddVmThread != NULL &&
                        mAddVmThread->getStatus() == AddVmThread::STATUS_OK) {
                // Newly created VM is inactive but in inventory
                showMessage(tr(SUCCESS_VM_ADDED), false);
		mVmList.append(mAddVmThread->getNewVm());
                QTreeWidgetItem* item = new QTreeWidgetItem();
                item->setText(0, mAddVmThread->getName());

                QTreeWidgetItem* subParent = mVmTreeView->topLevelItem(VmInfo::INACTIVE_INVENTORY);
                subParent->addChild(item);
	}

        if (mThread != NULL)
            delete mThread;
}

/* Handler to delete VM instance from list and backend file */
void NewPalacios::deleteVm(QString item) {
        int category = -1;
        QString devfile;

	for (int i = 0; i < mVmList.size(); i++) {
		if (item.compare(mVmList[i]->getVmName()) == 0) {
                        category = mVmList[i]->getCategory();
                        devfile = mVmList[i]->getVmDevFile();
			break;
		}
	}

	mThread = new QThread();
	mDeleteVmThread = new DeleteVmThread(category, item, devfile);
	mDeleteVmThread->moveToThread(mThread);
	connect(mThread, SIGNAL(started()), mDeleteVmThread, SLOT(deleteVm()));
        connect(mDeleteVmThread, SIGNAL(finished()), mThread, SLOT(quit()));
        connect(mDeleteVmThread, SIGNAL(finished()), this, SLOT(handleVmDeletion()));
	mThread->start();
}

void NewPalacios::handleVmDeletion() {
        if (mDeleteVmThread->getStatus()
                        == DeleteVmThread::ERROR_V3FREE_PATH) {
                showMessage(tr(ERROR_VM_DELETE_PATH), true);

        } else if (mDeleteVmThread->getStatus()
                        == DeleteVmThread::ERROR_V3FREE_IOCTL) {
                showMessage(tr(ERROR_VM_DELETE_IOCTL), true);

        } else if (mDeleteVmThread->getStatus()
                        == DeleteVmThread::ERROR_V3FREE_DB) {
                showMessage(tr(ERROR_VM_DELETE_DB), true);

        } else if (mDeleteVmThread->getStatus()
                        == DeleteVmThread::ERROR_V3FREE_INVALID_ARGUMENT) {
                showMessage(tr(ERROR_VM_DELETE_INVALID_ARGUMENT), true);    

        } else if (mDeleteVmThread->getStatus()
                        == DeleteVmThread::STATUS_OK) {
                // Delete VM from list
	        for (int i = 0; i < mVmList.size(); i++) {
		        if (mVmName.compare(mVmList[i]->getVmName()) == 0) {
                                // Deletion is a little tricky
                                // 1. Find the VM to be deleted and get its name
                                // 2. Remove VM from list
                                // 3. Determine the category
                                // 4. Search the appropriate category for the VM
                                // 5. Remove from UI widget and delete
			        int cat = mVmList[i]->getCategory();
                                mVmList.removeAt(i);
                                
                                if (cat == VmInfo::ACTIVE_INVENTORY) {
                                    QTreeWidgetItem* activeItems = mVmTreeView->topLevelItem(VmInfo::ACTIVE_INVENTORY);
                                    for (int j=0; j<activeItems->childCount(); j++) {
                                        QString n = activeItems->child(j)->text(0);
                                        if (mVmName.compare(n) == 0) {
                                                QTreeWidgetItem* achild = activeItems->takeChild(j);
                                                delete achild;
                                                break;
                                        }
                                                
                                    }
                                } else if (cat == VmInfo::INACTIVE_INVENTORY) {
                                   QTreeWidgetItem* inactiveItems = mVmTreeView->topLevelItem(VmInfo::INACTIVE_INVENTORY);
                                   for (int j=0; j<inactiveItems->childCount(); j++) {
                                        QString in = inactiveItems->child(j)->text(0);
                                        if (mVmName.compare(in) == 0) {
                                                QTreeWidgetItem* ichild = inactiveItems->takeChild(j);
                                                delete ichild;
                                                break;
                                        }
                                   }
                                } else if (cat == VmInfo::ACTIVE_NOT_INVENTORY) {
                                	QTreeWidgetItem* activeNotInvItems = mVmTreeView->topLevelItem(VmInfo::ACTIVE_NOT_INVENTORY);
                                   	for (int j=0; j<activeNotInvItems->childCount(); j++) {
                                   	     QString an = activeNotInvItems->child(j)->text(0);
                                   	     if (mVmName.compare(an) == 0) {
                                   	             QTreeWidgetItem* aichild = activeNotInvItems->takeChild(j);
                                   	             delete aichild;
                                   		     break;
                                        	}
                                   	}
                                
                                }
				
				// Clear VM from Info widget view
                                mVmInfoWidget->deleteVm();
			        break;
		        }
	        }

                showMessage(tr(SUCCESS_VM_DELETE), false);
        }

        if (mThread != NULL)
            delete mThread;
}
