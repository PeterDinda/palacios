/*
 * vm_console_widget.h
 *
 *  Created on: Oct 5, 2012
 *      Author: abhinav
 */

#ifndef VM_CONSOLE_WIDGET_H_
#define VM_CONSOLE_WIDGET_H_

#include "vnc_module/vncview.h"

#include <QProcess>
#include <QWidget>
#include <QCloseEvent>

class VmConsoleWidget: public QWidget {
Q_OBJECT

public:
        // Different operation modes of VM
        enum {
                STREAM, CONSOLE, VNC
        };
        
	VmConsoleWidget(QWidget* parent = 0);
	virtual ~VmConsoleWidget();
	bool isRunning();

public slots:
	bool start(int mode, QString devfile, QString streamName);
	bool tryTerminate();
	void mainWindowClosing();

protected slots:
	void termProcessExited(int, QProcess::ExitStatus);
        void errorMessage(QProcess::ProcessError);

signals:
	void exited();

protected:
	void closeEvent(QCloseEvent *);
	void resizeEvent(QResizeEvent *);

private:
	int mCols;
	int mRows;
        QString mVmMode;
        // Process for xterm
	QProcess* mTermProcess;
        // VNC
        QProcess* mVncServer;
        VncView* mVncView;

};


#endif /* VM_CONSOLE_WIDGET_H_ */
