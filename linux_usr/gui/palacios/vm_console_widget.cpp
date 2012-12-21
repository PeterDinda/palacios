/*
 * vm_console_widget.cpp
 *
 *  Created on: Oct 4, 2012
 *      Author: abhinav
 */

#include "vm_console_widget.h"

#include <QDebug>
#include <QMessageBox>
#include <QVBoxLayout>
#include <X11/Xlib.h>

VmConsoleWidget::VmConsoleWidget(QWidget* parent) :
		QWidget(parent), mCols(100), mRows(25), mTermProcess(0) {
        mTermProcess = NULL;
        mVncServer = NULL;
}

VmConsoleWidget::~VmConsoleWidget() {

}

bool VmConsoleWidget::tryTerminate() {
	if (mTermProcess == NULL) {
		return true;
	}

	if (mTermProcess != NULL && mTermProcess->state() == QProcess::Running) {
		mTermProcess->terminate();
		bool xwindow_closed = mTermProcess->waitForFinished();
                delete mTermProcess;
                return xwindow_closed;

        } else if (mVncView != NULL) {
                mVncServer->terminate();
                bool vncserver_exited = mVncServer->waitForFinished();
                delete mVncView;
                delete mVncServer;
                return vncserver_exited;
        }

	return true;
}

void VmConsoleWidget::mainWindowClosing() {
	close();
}

void VmConsoleWidget::closeEvent(QCloseEvent* e) {
	if (!tryTerminate())
		qDebug() << "Warning: could not terminate process...there could be a leak";
	else
		qDebug() << "Process terminated";

	e->accept();
}

void VmConsoleWidget::resizeEvent(QResizeEvent* re) {
	QWidget::resizeEvent(re);

	if (mTermProcess == NULL)
		return;

	// Search for xterm window and update its size
	Display *dsp = XOpenDisplay(NULL);
	Window wnd = winId();

	bool childFound = false;
	while (!childFound && mTermProcess->state() == QProcess::Running) {
		Window root, parent, *children;
		uint numwin;
		XQueryTree(dsp, wnd, &root, &parent, &children, &numwin);
		childFound = (children != NULL);

		if (childFound) {
			XResizeWindow(dsp, *children, width(), height());
			XFree(children);
		}
	}

	XCloseDisplay(dsp);
}

bool VmConsoleWidget::isRunning() {
	if (mTermProcess == NULL) {
		return false;
	}
        
        // FIXME: This function is currently very unreliable
        // The QProcess->state() function should return the correct
        // state of the process but it is currently not working as expected
        // Debugging with GDB also does not reveal the problem
        // Maybe the way this method is being called is incorrect
	return (mTermProcess->state() == QProcess::Running) ? true : false;
}

bool VmConsoleWidget::start(int mode, QString devfile, QString streamName) {
	qDebug() << "VmConsoleWidget : start() [" << streamName << "]";
        
        // Start VM in correct mode
        if (mode == VmConsoleWidget::STREAM) {
                mVmMode = "v3_stream";
        } else if (mode == VmConsoleWidget::CONSOLE) {
                mVmMode = "v3_cons_sc";
        } else {
                mVmMode = "v3_vncclient";
        }
        
        qDebug() << "Vm Mode: " << mVmMode;
        qDebug() << "Vm dev file: " << devfile;

	QStringList args;
        
        if (streamName.compare("") != 0) {
            // This is v3_stream mode
	    args << "-sb" << "-geometry" << QString("%1x%2").arg(mCols).arg(mRows) << "-j"
		<< "-into" << QString::number(winId()) << "-e" << mVmMode << devfile << streamName;
                
                // Start the xterm process
	        qDebug() << "Starting terminal with arguments '" << args.join(" ");
                mTermProcess = new QProcess();

                // Connect signals and slots
	        connect(mTermProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, 
			SLOT(termProcessExited(int, QProcess::ExitStatus)));

	        connect(mTermProcess, SIGNAL(error(QProcess::ProcessError)), this, 
			SLOT(errorMessage(QProcess::ProcessError)));

	        mTermProcess->start("/usr/bin/xterm", args);

        } else {
            // This is non-stream mode
            
            if (mVmMode.compare("v3_cons_sc") == 0) {
                // Console mode
                args << "-sb" << "-geometry" << QString("%1x%2").arg(mCols).arg(mRows) << "-j"
	                << "-into" << QString::number(winId()) << "-e" << mVmMode << devfile;


	        qDebug() << "Starting terminal with arguments '" << args.join(" ");
                mTermProcess = new QProcess();

                // Connect signals and slots
	        connect(mTermProcess, SIGNAL(finished(int, QProcess::ExitStatus)), this, 
			SLOT(termProcessExited(int, QProcess::ExitStatus)));

        	connect(mTermProcess, SIGNAL(error(QProcess::ProcessError)), this, 
			SLOT(errorMessage(QProcess::ProcessError)));
	
                mTermProcess->start("/usr/bin/xterm", args);
            
            } else {
                // VNC mode
                mVncServer = new QProcess();
                mVncServer->setProcessChannelMode(QProcess::MergedChannels);

	        connect(mVncServer, SIGNAL(finished(int, QProcess::ExitStatus)), this, 
			SLOT(termProcessExited(int, QProcess::ExitStatus)));

        	connect(mVncServer, SIGNAL(error(QProcess::ProcessError)), this, 
			SLOT(errorMessage(QProcess::ProcessError)));
                
                QStringList args;
                args << "--port=5951" << "--password=test123" << devfile;
                qDebug() << "arguments: " << args.join(" ");
                mVncServer->start("v3_vncserver", args);

                if (!mVncServer->waitForFinished()) {
                        qDebug() << "Error with VNC server";
                        QMessageBox msg;
                        msg.setText("Error with VNC server!");
                        msg.setStandardButtons(QMessageBox::Ok);
                        msg.setIcon(QMessageBox::Critical);
                        msg.exec();
                        return false;
                }
                
                //args << "-sb" << "-geometry" << QString("%1x%2").arg(mCols).arg(mRows) << "-j"
	        //        << "-into" << QString::number(winId()) << "-e" << "vncviewer localhost:5951";
                
                QVBoxLayout* vncBox = new QVBoxLayout();
                mVncView = new VncView(this, QUrl("vnc://localhost:5951"), KConfigGroup());
                //mVncView = new VncView(this, QUrl("vnc://localhost:5901"), KConfigGroup());
                vncBox->addWidget(mVncView);
                this->setLayout(vncBox);
                
                mVncView->show();
                mVncView->start();
            }
        }
        
        // Flag to indicate success and failure
        bool status = false;

        if (mVmMode.compare("v3_vncclient") == 0) {
                // This is VNC mode
                status = true;       
                
        } else {
            int success;

            // This is v3_stream or v3_cons_sc mode
            if (mTermProcess != NULL && mTermProcess->waitForStarted()) {
	            success = 0;
		    qDebug() << "Process started";
	    } else {
		    success = -1;
                    qDebug() << "Process did not start";
            }
            
	    if (success == 0) {
	        /* Wait for the xterm window to be opened and resize it
                 * to our own widget size.
        	 */
        	Display *dsp = XOpenDisplay(NULL);
	        Window wnd = winId();

        	bool childFound = false;
        	while (!childFound && mTermProcess->state() == QProcess::Running) {
        		Window root, parent, *children;
        		uint numwin;
        		XQueryTree(dsp, wnd, &root, &parent, &children, &numwin);
        		childFound = (children != NULL);
        
                        if (childFound) {
	        		XResizeWindow(dsp, *children, width(), height());
        		}
	       	}

	        XCloseDisplay(dsp);

        	if (!childFound)
	       		success = -2;
	    }
        
            if (success < 0) {
	       	qDebug() << (success == -1 ? "Starting the process failed" 
                        : "Process started, but exited before opening a terminal");

	        if (success < -1)
		        tryTerminate();
	    }
        
	    status = (success == 0);
        }

        return status;
}

void VmConsoleWidget::termProcessExited(int a, QProcess::ExitStatus b) {
	qDebug() << "Term Process Exited: " << a << " :  " << b;

	if (mTermProcess != NULL) {
            delete mTermProcess;
        }

        if (mVncServer != NULL) {
            delete mVncServer;
        }

	mTermProcess = NULL;
        mVncServer = NULL;

	emit exited();
}

void VmConsoleWidget::errorMessage(QProcess::ProcessError err) {
        qDebug() << "Process error: " << err;
}
