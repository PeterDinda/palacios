#ifndef NEWPALACIOS_H
#define NEWPALACIOS_H

#include <QtGui>
#include <QApplication>
#include <QMainWindow>
#include <QWidget>
#include <QListWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QTextEdit>
#include <QFile>
#include <QTextStream>
#include <QList>
#include <QAction>
#include <QMenu>
#include <QMenuBar>
#include <QToolBar>
#include <QStatusBar>
#include <QProcess>
#include <QObject>
#include <QDomDocument>
#include <QDomNode>
#include <QDomNodeList>
#include <QDomElement>
#include <QDomText>
#include <QDomAttr>
#include <QDomNamedNodeMap>

#include "vm_creation_wizard.h"
#include "vm_console_widget.h"

class VmInfo;
class VmInfoWidget;
class VmXConsoleParent;
class VmVncWidget;
class LoadVmsThread;
class AddVmThread;
class DeleteVmThread;
class VmModeDialog;

class NewPalacios: public QMainWindow {
Q_OBJECT

public:
	NewPalacios(QWidget *parent = 0);
	~NewPalacios();

protected:
        void closeEvent(QCloseEvent*);

private slots:
	void createVmInstance();
	void aboutPalacios();
	void exitApplication();
	void startVm();
        void stopVm();
        void pauseVm();
        void restartVm();
        void activateVm();
        void reloadVms();
        // Item click listener for tree items
        void vmItemClickListener(QTreeWidgetItem*, int);
	void finishLoadingVmsFromFile();
	void addNewVm();
	void updateVmList();
	void deleteVm(QString);
        void handleVmDeletion();
        // Context menu for VMs
        void vmContextMenu(const QPoint &p);
	void consoleWindowClosed(QString);
        void vmTabClosed(int);
        // This slot will be triggered when play/menu start
        // button is pressed. This will launch a dialog which
        // will allow the user to select operating mode
        void selectVmMode();
        // This slot will be triggered from the mode selection
        // dialog when the user has finished selection. 
        void getVmMode(int, QString);
        void updateTelemetry(bool);
        void updateTelemetryView();
        
private slots:
	// These are used for debugging QProcess commands
	//void processStarted();
	//void processExit(int, QProcess::ExitStatus);
	//void processError(QProcess::ProcessError);

signals:
	void vmLoadingComplete();

public:
	void readExistingVmsFile();
	// Check for Palacios setup
        void checkPalacios();

private:
	// Functions to setup different components of the window
	void createCentralWidget();
	void createActions();
	void createMenus();
	void createToolBars();
	void createStatusBar();
	void createDockWindows();
	void createWizard();
	void createEventHandler();
        // Show error message boxes
        void showMessage(QString err, bool error, bool warning=false);
        // Update state of VM
        void updateVmState(int);
        // Create inactive VM
        int createInactiveVm();
        // Update DB
        int updateDb(int);
        
private:
	// Main view of the application.
        QTabWidget* mVmControlPanel;
	VmInfoWidget* mVmInfoWidget;
        // This is used to represent the classes of VM
        QTreeWidget* mVmTreeView;
        QTextEdit* mVmTelemetryView;

        // Progress dialog to inform user about background thread processing
        //QProgressDialog* mProgress;

	// Runs Palacios in terminal mode
        QList<VmXConsoleParent*> mRunningVms;
        
        bool mExitAppFromMenu;
        // Used to identify VMs inside list
        int mVmPos;
        QString mVmName;
        
	// Action variables used to handle events such as menu clicks, button clicks etc.
	QAction* mExitApp;
	QAction* mVmNew;
	QAction* mVmStop;
	QAction* mVmPause;
	QAction* mVmStart;
	QAction* mVmActivate;
        QAction* mReloadVms;
	QAction* mAboutApp;

	// Menu variables
	QMenu* mFileMenu;
	QMenu* mViewMenu;
	QMenu* mVmMenu;
	QMenu* mHelpMenu;

	// Toolbar variables
	QToolBar* mVmToolBar;
	QToolBar* mVmCtrlToolBar;
        
        // This dialog will be used to give option
        // to user to select from three modes of
        // operation
        VmModeDialog* mVmModeDialog;
        int mVmMode;
        QString mStreamName;
        bool isHeaderClicked;

        // Process to read kernel logs
        QProcess* mLogProc;

	// Wizard to help in vm creation
	NewVmWizard* mVmWizard;

	// List of created VMs. Each VM object contains information parsed from
	// the config files provided as part of VM creation
	QList<VmInfo*> mVmList;
        //QList<VM*> mVmList;

	/* We save the information about VMs in a text file. We store the name of the VM instance
	 * and the path of the configuration file used to create the VM. Every time we create/add/delete
	 * a VM instance, this file is edited. This is simplest way as of now to store this information.
	 * If in future we need more information to be stored we could use a database and Qt's
	 * model-view system */
        
        QThread* mThread;

	// Thread to load existing VM intances
	LoadVmsThread* mLoadVmsThread;
	// Thread to add a new VM instance
	AddVmThread* mAddVmThread;
	// Thread to delete a VM instance
	DeleteVmThread* mDeleteVmThread;
	
	// Telemetry process
	QProcess* mTelemProc;
};

class VmModeDialog : public QWidget {
Q_OBJECT
public:
    VmModeDialog(QWidget* parent = 0);

private slots:
    void selectMode(bool);
    void okButton();
    void cancelButton();

signals:
    // This signal will be caught in the main window
    // and the mode will be set accordingly
    void setMode(int, QString);

private:
    void setupDialog();

private:
    bool isV3Cons;
    bool isV3Stream;
    bool isV3Vnc;
    int mode;
    QGroupBox* v3_modes;
    QRadioButton* v3_stream;
    QRadioButton* v3_cons;
    QRadioButton* v3_vnc;
    QWidget* v3_stream_info;
    QLineEdit* v3_stream_name;
};

// Class to hold information about a VM instance
class VmInfo {
public:
	VmInfo() {
	}
	
	~VmInfo() {
	}
	
	// Tells us about the state of the VM
        enum {
                STOPPED, PAUSED, RUNNING
        };

        // Tells about the category of VM
        enum {
                ACTIVE_INVENTORY, INACTIVE_INVENTORY, ACTIVE_NOT_INVENTORY
        };
	
	// Return state of VM
        int getState() {
                return mVmState;     
        }
	
	// Ser state of VM
        void setState(int state) {
                this->mVmState = state;
        }

        void setCategory(int cat) {
                this->mVmCategory = cat;
        }

        int getCategory() {
                return mVmCategory;
        }

        void setImageFile(QString img) {
                this->mVmImageFile = img;
        }

        QString getImageFile() {
                return mVmImageFile;
        }
        
        QString getVmName() {
		return mVmName;
	}

	void setVmName(QString name) {
		this->mVmName = name;
	}

        QString getVmDevFile() {
                return mVmDevFile;
        }

        void setVmDevFile(QString name) {
                this->mVmDevFile = name;
        }
        
        QString getVmConfigFile() {
        	return this->mVmConfigFile;
        }
        
        void setVmConfigFile(QString file) {
        	this->mVmConfigFile = file;
        }
        
private:
        int mVmState;
        int mVmCategory;
	QString mVmName;
        QString mVmDevFile;
        QString mVmImageFile;
        QString mVmConfigFile;
};

class VmInfoWidget: public QWidget {
Q_OBJECT
public:
	VmInfoWidget(QWidget* parent = 0);
        ~VmInfoWidget();
	
private:
	void setInfoView();
	void setupUi();

public:
	void parseElement(const QDomElement&, QTreeWidgetItem*);
    	void parseAttribute(const QDomElement&, QTreeWidgetItem*);
    	void parseText(const QDomElement&, QTreeWidgetItem*);
        void updateInfoView(VmInfo* vm);
	void deleteVm();

private:
	QTreeWidget* mVmInfoView;
};

class VmXConsoleParent: public QWidget {
Q_OBJECT
public:
	VmXConsoleParent(QString name, QWidget* parent = 0);
        void showWidget(int mode, QString devfile, QString streamName);
	void showTelemetryInfo();
	bool isRunning();
        QString getVmName();
   	
signals:
	void windowClosingWithId(QString name);
        void windowClosing();

protected:
	void closeEvent(QCloseEvent* event);

private:
	bool mIsConsoleRunning;
	QString mVmName;
    	VmConsoleWidget* mConsole;
};

class LoadVmsThread: public QObject {
Q_OBJECT
public:
	static const int STATUS_OK = 0;
	static const int ERROR_FILE_CANNOT_BE_OPENED = -1;

	int getStatus();
	QList<VmInfo*> getVmList();

signals:
	void finished();

public slots:
	void loadVms();
private:
	int status;
	QList<VmInfo*> list;
};

class AddVmThread: public QObject {
Q_OBJECT
public:
	static const int STATUS_OK = 0;
        static const int ERROR_V3CREATE_PATH = -1;
        static const int ERROR_V3CREATE_IOCTL = -2;
        static const int ERROR_V3CREATE_PROC = -3;
        static const int ERROR_V3CREATE_DEV = -4;
        static const int ERROR_V3CREATE_DB = -5;
        
	AddVmThread(QString name, QString conf, QString img);
	int getStatus();
	QString getName();
	VmInfo* getNewVm();

signals:
	void finished();

public slots:
	void addVm();

private:
	int status;
	QString vmName;
        QString vmDevFile;
        QString vmConfigFile;
        QString vmImageFile;
	VmInfo* vm;
};

class DeleteVmThread: public QObject {
Q_OBJECT
public:
	static const int STATUS_OK = 0;
        static const int ERROR_V3FREE_PATH = -1;
        static const int ERROR_V3FREE_IOCTL = -2;
        static const int ERROR_V3FREE_DB = -3;
        static const int ERROR_V3FREE_INVALID_ARGUMENT = -4;

	DeleteVmThread(int, QString, QString);
	int getStatus();

signals:
        void finished();

public slots:
	void deleteVm();
	
private slots:
	// These are used for debugging QProcess commands
	//void processStarted();
	//void processExit(int, QProcess::ExitStatus);
	//void processError(QProcess::ProcessError);

private:
	int status;
        int vmCategory;
	QString vmToDelete;
        QString vmDevfile;
};

#endif // NEWPALACIOS_H
