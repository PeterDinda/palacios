/*
 * newvmwizard.cpp
 *
 *  Created on: Sep 20, 2012
 *      Author: abhinav
 */

#include <QMessageBox>
#include "vm_creation_wizard.h"

NewVmWizard::NewVmWizard(QWidget* parent) :
		QWizard(parent) {
        
        setPage(Page_Intro, new IntroPage);
	setPage(Page_Image_File, new GuestImagePage);
	setPage(Page_Final, new FinalPage);
        setStartId(Page_Intro);
        
        setOptions(QWizard::NoBackButtonOnLastPage);
	setWindowTitle(tr("New Virtual Machine"));
}

NewVmWizard::~NewVmWizard() {
}

IntroPage::IntroPage(QWidget* parent) :
		QWizardPage(parent) {
	setTitle(tr("Introduction"));
	setPixmap(QWizard::WatermarkPixmap, QPixmap(":/images/images/palacios.png"));

	topLabel = new QLabel(
			tr("This wizard will help you to create a new virtual machine"));
	topLabel->setWordWrap(true);

	QVBoxLayout *layout = new QVBoxLayout;
	layout->addWidget(topLabel);
	setLayout(layout);
}

IntroPage::~IntroPage() {
        delete topLabel;
}

int IntroPage::nextId() const {
	return NewVmWizard::Page_Image_File;
}

GuestImagePage::GuestImagePage(QWidget* parent) :
		QWizardPage(parent) {
	setTitle(tr("VM Creation"));
	setSubTitle(
			tr(
					"Please fill all the fields. Make sure all file paths are correct"));

	guestNameLabel = new QLabel(tr("Enter a name for the guest"));
	guestName = new QLineEdit();
	guestNameLabel->setBuddy(guestName);

	configLocLabel = new QLabel(tr("Enter path of config file"));
	configLoc = new QLineEdit();
	configLocLabel->setBuddy(configLoc);

	imageLocLabel = new QLabel(tr("Enter path of image file"));
	imageLoc = new QLineEdit();
	imageLocLabel->setBuddy(imageLoc);

	browseConfig = new QPushButton(tr("Browse"));
	browseImage = new QPushButton(tr("Browse"));

	registerField("guestName*", guestName);
	registerField("configLoc*", configLoc);
	registerField("imageLoc*", imageLoc);
	//registerField("memory*", memory);

	connect(browseConfig, SIGNAL(clicked()), this, SLOT(locateConfigFile()));
	connect(browseImage, SIGNAL(clicked()), this, SLOT(locateImageFile()));

	QGridLayout* layout = new QGridLayout();
	layout->addWidget(guestNameLabel, 0, 0);
	layout->addWidget(guestName, 0, 1);
	layout->addWidget(configLocLabel, 1, 0);
	layout->addWidget(configLoc, 1, 1);
	layout->addWidget(browseConfig, 1, 2);
	layout->addWidget(imageLocLabel, 2, 0);
	layout->addWidget(imageLoc, 2, 1);
	layout->addWidget(browseImage, 2, 2);

	setLayout(layout);
}

GuestImagePage::~GuestImagePage() {
        delete guestNameLabel;
        delete guestName;
        delete configLocLabel;
        delete configLoc;
        delete imageLocLabel;
        delete imageLoc;
        delete browseConfig;
        delete browseImage;
}

void GuestImagePage::locateConfigFile() {
	QString configFile = QFileDialog::getOpenFileName(this, tr("Open file"), ".",
			"XML file (*.xml)");
	configLoc->setText(configFile);
}

void GuestImagePage::locateImageFile() {
	QString imageFile = QFileDialog::getOpenFileName(this, tr("Open file"), ".",
			"Image file (*.img *.bZ)");
	imageLoc->setText(imageFile);
}

int GuestImagePage::nextId() const {
	return NewVmWizard::Page_Final;
}

bool GuestImagePage::validatePage() {        
        v3Proc = new QProcess();
        v3Proc->setProcessChannelMode(QProcess::MergedChannels);
        v3Proc->start("cat /proc/v3vee/v3-guests");
        v3Proc->waitForFinished();
        qDebug() << "Reply from proc to check for new VM creation: " << v3Proc->readAllStandardOutput();
        QString name = field("guestName").toString();
        QByteArray temp = name.toLocal8Bit();
        const char* vmEntry = temp.data();
        bool exists = false;

        QByteArray val = v3Proc->readAllStandardOutput();
        if (!val.isNull()) {
                QList<QByteArray> procLine = val.split('\n');
                if (procLine.size() > 0) {
                        for (int i=0; i<procLine.size(); i++) {
                                if (procLine[i].contains(vmEntry)) {
                                        exists = true;
                                        break;
                                }
                        }
                }
        }

        if (exists) {
		QMessageBox warning;
		warning.setText("A Virtual Machine with this name already exists! Please use another name");
		warning.setIcon(QMessageBox::Critical);
		warning.setStandardButtons(
				QMessageBox::Ok);
		warning.exec();
                return false;
        }
        
        delete v3Proc;
        return true;
}

FinalPage::FinalPage(QWidget* parent) :
		QWizardPage(parent) {
        this->setCommitPage(true);
        this->setFinalPage(true);
	setTitle(tr("Status"));
	setPixmap(QWizard::WatermarkPixmap, QPixmap(":/images/images/palacios.png"));

	finalLabel = new QLabel("VM Creation Successful");

	QVBoxLayout* layout = new QVBoxLayout();
	layout->addWidget(finalLabel);

	setLayout(layout);
}

FinalPage::~FinalPage() {
        delete finalLabel;
}

int FinalPage::nextId() const {
        return -1;
}
