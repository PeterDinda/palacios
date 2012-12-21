/*
 * newvmwizard.h
 *
 *  Created on: Sep 20, 2012
 *      Author: abhinav
 */

#ifndef NEWVMWIZARD_H_
#define NEWVMWIZARD_H_

#include <QtGui>
#include <QWizard>

class IntroPage;
class GuestImagePage;
class FinalPage;

class NewVmWizard: public QWizard {
Q_OBJECT
public:
	enum {
		Page_Intro, Page_Image_File, Page_Final
	};
	NewVmWizard(QWidget* parent = 0);
	~NewVmWizard();

signals:
	void finishedWizard(QString guestName);

private:
        IntroPage* mIntroPage;
        GuestImagePage* mImagePage;
        FinalPage* mFinalPage;
};

class IntroPage: public QWizardPage {
Q_OBJECT

public:
	IntroPage(QWidget* parent = 0);
        ~IntroPage();
	int nextId() const;

private:
	QLabel* topLabel;
};

class GuestImagePage: public QWizardPage {
Q_OBJECT

public:
	GuestImagePage(QWidget* parent = 0);
        ~GuestImagePage();
        bool validatePage();
	int nextId() const;

private slots:
	void locateConfigFile();
	void locateImageFile();

private:
	QLabel* guestNameLabel;
	QLabel* configLocLabel;
	QLabel* imageLocLabel;

	QLineEdit* guestName;
	QLineEdit* configLoc;
	QLineEdit* imageLoc;

	QPushButton* browseConfig;
	QPushButton* browseImage;

        QProcess* v3Proc;
};

class FinalPage: public QWizardPage {
Q_OBJECT

public:
	FinalPage(QWidget* parent = 0);
        ~FinalPage();
	int nextId() const;

private:
	QLabel* finalLabel;
};

#endif /* NEWVMWIZARD_H_ */
