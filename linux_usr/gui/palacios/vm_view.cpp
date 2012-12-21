#include <QDebug>
#include <QWidget>
#include <QStringList>
#include <QCloseEvent>
#include <QResizeEvent>

#include "newpalacios.h"

VmXConsoleParent::VmXConsoleParent(QString name, QWidget* parent)
	: QWidget(parent) {
	
	mVmName = name;
        
        QVBoxLayout* l = new QVBoxLayout;
        mConsole = new VmConsoleWidget(this);
       	l->addWidget(mConsole);
       	l->setStretchFactor(mConsole, 1000);
        
       	this->setLayout(l);
       	this->setWindowTitle(name);
        
       	connect(this, SIGNAL(windowClosing()), mConsole,
             	SLOT(mainWindowClosing()));
}

void VmXConsoleParent::showWidget(int mode, QString devfile, QString streamName) {
	mConsole->start(mode, devfile, streamName);
	        
	// Workaround for QProcess error
	mIsConsoleRunning = true;
}

bool VmXConsoleParent::isRunning() {
	// FIXME: For some reason QProcess does not seem to return the correct state
        // Qprocess->state() should give the correct state of the running process
        // but currently there is a problem with geting the right state.
        // For the time being a boolean flag is used to represent the state (running/not running)
	        
        //return mConsole->isRunning();
        return mIsConsoleRunning;
}

QString VmXConsoleParent::getVmName() {
	return mVmName;
}

void VmXConsoleParent::closeEvent(QCloseEvent* event) {
        // Workaround for QProcess error
        /*if (!tryTerminate())
		qDebug() << "Warning: could not terminate process...there could be a leak";
	else
		qDebug() << "Process terminated";*/
		
        mIsConsoleRunning = false;
	emit windowClosingWithId(mVmName);
        emit windowClosing();
	event->accept();
}
