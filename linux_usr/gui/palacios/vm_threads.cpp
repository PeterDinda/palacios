#include <QDebug>

#include "newpalacios.h"

class ProcVm {
public:
	QString name;
	QString devFile;
};

class DbVm {
public:
	QString name;
	QString config;
	QString image;
	int state;
	QString dev;
};

int LoadVmsThread::getStatus() {
	return status;
}

QList<VmInfo*> LoadVmsThread::getVmList() {
	return list;
}

void LoadVmsThread::loadVms() {
        bool doesProcExist = false;
        bool isDbPresent = false;
        
        // Check if /proc file exists
        QFileInfo procInfo("/proc/v3vee/v3-guests");
        if (procInfo.exists()) {
                // Proc file exists
                doesProcExist = true;
        } 
       
        // Read proc file
        //QStringList procVms;
        QList<ProcVm> procVms;
        
        if (doesProcExist) {
                QProcess* p = new QProcess();
                p->setProcessChannelMode(QProcess::MergedChannels);
                QStringList args;
                args << "-c" << "cat /proc/v3vee/v3-guests";
                p->start("sh", args);
                if (!p->waitForFinished()) {
                        // Since we are unable to open the proc file
                        // for whatever some reason, the only way
                        // to identify this state would be to mark /proc
                        // as not existing                        
                        doesProcExist = false;
                        
                } else {
                        QByteArray procFile = p->readAllStandardOutput();
                        if (!procFile.isNull()) {
                            if (procFile.isEmpty()) {
                                doesProcExist = false;

                            } else {
                                QList<QByteArray> procContents = procFile.split('\n');
                                
                                for (int i=0; i<procContents.size(); i++) {
                                        QByteArray b = procContents.at(i);
                                        QString procentry(b);
                                        QStringList vals = procentry.split('\t');
                                                                            
                                        if (vals.size() == 2) {
                                        	ProcVm procVm;
                                                QString val = vals.at(0);
                                                val.remove(QChar('\n'), Qt::CaseInsensitive);
                                        	procVm.name = val;
                                                
                                                val = vals.at(1);
                                                val.remove(QChar('\n'), Qt::CaseInsensitive);
                                        	procVm.devFile = val;
                                        	procVms.append(procVm);
                                        }
                                }
                            }
                        } 
                }
                
                // Delete proc process
                delete p;
        }
        
        // Check if proc is empty
        if (doesProcExist && procVms.size() == 0) {
                // There was nothing in the proc file
                doesProcExist = true;
        }
        
        // Check if DB exists
        QFileInfo dbFile("virtual_machines_list.txt");
        if (dbFile.exists()) {
        	isDbPresent = true;
        }
        
        QFile file("virtual_machines_list.txt");
        // Open DB file
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
                // The DB file is missing
		isDbPresent = false;
	}
	
	// Check if there is any source to load the VM
	if (!doesProcExist && !isDbPresent) {
		// If proc file does not exist or is empty
		// and the DB does not exist then there are 
		// no VMs to load
		// return with STATUS_OK
		status = STATUS_OK;
		emit finished();
		return;	
	}
	
	//QStringList dbVms;
        QList<DbVm> dbVms;

	if (isDbPresent) {
		// Read the DB file
       		QTextStream in(&file);
        	while (!in.atEnd()) {
			QString line = in.readLine();
			if (line.compare("\n") == 0) {
				continue;
			}
		
			QStringList vmDet = line.split(",");
			DbVm dbVm;
			dbVm.name = vmDet.at(0);
			dbVm.config = vmDet.at(1);
			dbVm.dev = vmDet.at(2);
			dbVm.state = vmDet.at(3).toInt();
			dbVm.image = vmDet.at(4);
			
			dbVms.append(dbVm);
		}
	}
	        	
	if (!doesProcExist && isDbPresent) {
		// If /proc does not exist but DB does
		// then all VMs are inactive
		for (int i=0; i<dbVms.size(); i++) {
			VmInfo* vmDb = new VmInfo();
			vmDb->setVmName(dbVms[i].name);
			vmDb->setVmConfigFile(dbVms[i].config);
        	        vmDb->setVmDevFile(dbVms[i].dev);
        	        vmDb->setState(dbVms[i].state);
        	        vmDb->setImageFile(dbVms[i].image);
        	        vmDb->setCategory(VmInfo::INACTIVE_INVENTORY);
        	        // Add VM to loading list
			list.append(vmDb);
		}
				
	} else if (doesProcExist && !isDbPresent) {
		// If /proc exists but DB does not
		for (int i=0; i<procVms.size(); i++) {
			VmInfo* vmProc = new VmInfo();
			vmProc->setVmName(procVms[i].name);
			vmProc->setVmConfigFile("");
        	        vmProc->setVmDevFile(procVms[i].devFile);
        	        vmProc->setState(VmInfo::STOPPED);
        	        vmProc->setImageFile("");
        	        vmProc->setCategory(VmInfo::ACTIVE_NOT_INVENTORY);
        	        // Add VM to loading list
			list.append(vmProc);
		}
		
	} else if (doesProcExist && isDbPresent) {
		// Both files exist
		// Compare entries in /proc and text file, mark non-matching entries in /proc as active not inventory
		QList<int> procIndices;
		QList<int> dbIndices;		
		
                int found = -1;

		for (int i=0; i<procVms.size(); i++) {
			for (int j=0; j<dbVms.size(); j++) {
				if ((procVms[i].devFile.compare(dbVms[j].dev) == 0)
					&& (procVms[i].name.compare(dbVms[j].name) == 0)) {
                                        found = j;
					procIndices.append(i);
					dbIndices.append(j);
					break;
				}		
			}
			
                        if (found >= 0) {
	                        VmInfo* vm = new VmInfo();
			        vm->setVmName(dbVms[found].name);
			        vm->setVmConfigFile(dbVms[found].config);
			        vm->setVmDevFile(dbVms[found].dev);
			        vm->setState(dbVms[found].state);
			        vm->setImageFile(dbVms[found].image);
			        
			        vm->setCategory(VmInfo::ACTIVE_INVENTORY);
			        // Add VM to loading list
				list.append(vm);
                                found = -1;
                        }
		}
		
		// Once we have compared /proc with the DB and marked matching indices
		// we mark the rest of the entries in /proc as active not inventory
		for (int j=0; j<procVms.size(); j++) {
			if (!procIndices.contains(j)) {
				VmInfo* vmProcOnly = new VmInfo();
				vmProcOnly->setVmName(procVms[j].name);
				vmProcOnly->setVmConfigFile("");
				vmProcOnly->setVmDevFile(procVms[j].devFile);
				vmProcOnly->setState(VmInfo::STOPPED);
				vmProcOnly->setImageFile("");
				vmProcOnly->setCategory(VmInfo::ACTIVE_NOT_INVENTORY);
				list.append(vmProcOnly);
			}
		}
		
		// Once we have compared /proc with the DB and marked matching indices
		// we mark the rest of the entries in dbVms as inactive
		for (int j=0; j<dbVms.size(); j++) {
			if (!dbIndices.contains(j)) {
				VmInfo* vmDbOnly = new VmInfo();
				vmDbOnly->setVmName(dbVms[j].name);
				vmDbOnly->setVmConfigFile(dbVms[j].config);
				vmDbOnly->setVmDevFile("");
				vmDbOnly->setState(dbVms[j].state);
				vmDbOnly->setImageFile(dbVms[j].image);
				vmDbOnly->setCategory(VmInfo::INACTIVE_INVENTORY);
				list.append(vmDbOnly);
			}
		}
	}
	
	status = STATUS_OK;
	file.close();

	emit finished();
}

AddVmThread::AddVmThread(QString name, QString conf, QString img) {
	this->status = STATUS_OK;
	this->vmName = name;
	this->vmConfigFile = conf;
        this->vmImageFile = img;
}

int AddVmThread::getStatus() {
	return status;
}

VmInfo* AddVmThread::getNewVm() {
	return vm;
}

QString AddVmThread::getName() {
	return vmName;
}

void AddVmThread::addVm() {
	QFile file("virtual_machines_list.txt");
	if (!file.open(QIODevice::Append | QIODevice::Text)) {
		status = ERROR_V3CREATE_DB;
		emit finished();
		return;
	}
	
	// Add VM to the inventory (database). The VM still needs to be
        // activated to run
        // Create new VM instance
        // The dev file for this VM will be set when it is activated
        vm = new VmInfo();
        vm->setVmName(vmName);
        vm->setVmConfigFile(vmConfigFile);
        vm->setImageFile(vmImageFile);
        vm->setCategory(VmInfo::INACTIVE_INVENTORY);
	
	QTextStream out(&file);
	
        out << vmName;
        out << ",";
        out << vmConfigFile;
        out << ",";
        out << vmDevFile;
        out << ",";
        out << QString::number(VmInfo::STOPPED);
        out << ",";
        out << vmImageFile;
        out << endl;
	
        out.flush();
	file.close();
        
        status = STATUS_OK;

	emit finished();
}

DeleteVmThread::DeleteVmThread(int category, QString name, QString devfile) {
        this->vmCategory = category;
	this->vmToDelete = name;
	this->status = -2;
        this->vmDevfile = devfile;
}

int DeleteVmThread::getStatus() {
	return status;
}

/*void DeleteVmThread::processStarted() {
	//qDebug() << "V3 Free process started";
}

void DeleteVmThread::processExit(int errorCode, QProcess::ExitStatus exitStatus) {
	if (exitStatus == QProcess::CrashExit) {
		//qDebug() << "v3_free process crashed!!";
	} else {
		//qDebug() << "v3_free process exited normally..";
	}
}

void DeleteVmThread::processError(QProcess::ProcessError error) {
	//qDebug() << "There was an error in the v3_free process execution...";
	
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

void DeleteVmThread::deleteVm() {
     
     // Check for the category of VM being deleted
     // If Active or Active not inventory then only
     // invoke v3_free since we have an instance of the 
     // VM in proc. If it is Inactive and we are deleting
     // then we need to just remove it from the DB
     
     // Create variables
     // C++ complains if we create variables inside switch statement
     QProcess* v3Freeproc;
     QStringList args;
     QByteArray procOutput;
     
     switch (vmCategory) {
     case VmInfo::ACTIVE_INVENTORY:
     case VmInfo::ACTIVE_NOT_INVENTORY:
     	if (vmDevfile == NULL) {
        	// Cannot delete VM without dev file
                status = ERROR_V3FREE_INVALID_ARGUMENT;
                emit finished();
                return;
        }
	
       	v3Freeproc = new QProcess();
        v3Freeproc->setProcessChannelMode(QProcess::MergedChannels);
        
        // Connect debug slots
        connect(v3Freeproc, SIGNAL(started()), this, SLOT(processStarted()));
        connect(v3Freeproc, SIGNAL(finished(int, QProcess::ExitStatus)), this, SLOT(processExit(int, QProcess::ExitStatus)));
        connect(v3Freeproc, SIGNAL(error(QProcess::ProcessError)), this, SLOT(processError(QProcess::ProcessError)));
        
        args << vmDevfile;
        v3Freeproc->start("v3_free", args);
        
        if (!v3Freeproc->waitForFinished()) {
        	// Reinsert data into database since v3_free was not successful
                status = ERROR_V3FREE_PATH;
                delete v3Freeproc;
                emit finished();
                return;
        } else {
	        procOutput = v3Freeproc->readAllStandardOutput();

        	if (procOutput.contains("IOCTL error")) {
        		status = ERROR_V3FREE_IOCTL;
        	        delete v3Freeproc;
        	        emit finished();
        	        return;
        	}
        }
                                
        // V3_free success
        if (v3Freeproc != NULL) {
        	delete v3Freeproc;
        }
     	
     	break;
     case VmInfo::INACTIVE_INVENTORY:
     default:
     	break;
     }
     
     QFile file("virtual_machines_list.txt");
     QFile tempFile("temp.txt");

     bool oldFile = file.open(QIODevice::ReadOnly | QIODevice::Text);
     bool newFile = tempFile.open(QIODevice::WriteOnly | QIODevice::Text);

     if (!oldFile || !newFile) {
        status = ERROR_V3FREE_DB;
	emit finished();
        return;
     }

     QTextStream in(&file);
     QTextStream out(&tempFile);

     while (!in.atEnd()) {
        QString line = in.readLine();
	QStringList vm = line.split(",");
        if (vm.at(0).compare(vmToDelete) != 0) {
		out << line << endl;
	}
     }

    out.flush();
    file.remove("virtual_machines_list.txt");
    tempFile.rename("virtual_machines_list.txt");
      
    file.close();
    tempFile.close();
    status = STATUS_OK;
        
    emit finished();
}
