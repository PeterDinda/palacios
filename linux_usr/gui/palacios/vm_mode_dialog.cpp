#include <QDebug>

#include "newpalacios.h"

VmModeDialog::VmModeDialog(QWidget* parent) {
        setupDialog();
}

void VmModeDialog::setupDialog() {
        isV3Cons = false;
        isV3Stream = false;
        isV3Vnc = false;
        
        // Group box to hold radio buttons
        v3_modes = new QGroupBox(tr("Select VM mode"));
        // Widget to give information about stream name
        // for v3_stream
        v3_stream_info = new QWidget(v3_modes);

        v3_stream = new QRadioButton(tr("Stream Mode"));
        v3_cons = new QRadioButton(tr("Console Mode"));
        v3_vnc = new QRadioButton(tr("VNC mode"));
        
        // Setup stream info widget
        QLabel* v3_stream_label = new QLabel(tr("Stream name"));
        v3_stream_name = new QLineEdit();
        
        QGridLayout* grid = new QGridLayout();
        grid->addWidget(v3_stream_label, 0, 0);
        grid->addWidget(v3_stream_name, 0, 1);
        v3_stream_info->setLayout(grid);
        v3_stream_info->setVisible(false);
        
        // Setup group box
        QVBoxLayout* box = new QVBoxLayout();
        box->addWidget(v3_cons);
        box->addWidget(v3_stream);
        box->addWidget(v3_stream_info);
        box->addWidget(v3_vnc);
        v3_modes->setLayout(box);
        
        // Setup main layout for dialog
        QVBoxLayout* mainLayout = new QVBoxLayout();
        mainLayout->addWidget(v3_modes);
        QHBoxLayout* actionLayout = new QHBoxLayout();
        QPushButton* ok = new QPushButton(tr("OK"));
        QPushButton* cancel = new QPushButton(tr("Cancel"));
        actionLayout->addWidget(ok);
        actionLayout->addWidget(cancel);
        mainLayout->addLayout(actionLayout);
        
        setLayout(mainLayout);
        resize(300, 200);
        move(200, 200);
        setWindowTitle("Select VM mode");

        // Connect signals and slots
        connect(v3_cons, SIGNAL(toggled(bool)), this, SLOT(selectMode(bool)));
        connect(v3_stream, SIGNAL(toggled(bool)), this, SLOT(selectMode(bool)));
        connect(v3_vnc, SIGNAL(toggled(bool)), this, SLOT(selectMode(bool)));
        connect(ok, SIGNAL(clicked()), this, SLOT(okButton()));
        connect(cancel, SIGNAL(clicked()), this, SLOT(cancelButton()));
}

void VmModeDialog::okButton() {
        if (isV3Cons == false
                && isV3Stream == false
                && isV3Vnc == false) {
            
            // Do not emit anything
            close();
            return;
        }

        QString name = "";
        
        if (isV3Stream) {
                name = v3_stream_name->text();
        }
        
        emit setMode(mode, name);
        close();
}

void VmModeDialog::cancelButton() {
        close();
}

void VmModeDialog::selectMode(bool checked) {
        if (checked == true) {
                if (v3_cons->isChecked()) {
                        // If console is checked, then set
                        // mode to v3_cons
                        mode = VmConsoleWidget::CONSOLE;
                        isV3Cons = true;
                        isV3Stream = false;
                        isV3Vnc = false;
                        v3_stream_info->setVisible(false);

                } else if (v3_stream->isChecked()) {
                        mode = VmConsoleWidget::STREAM;
                        isV3Cons = false;
                        isV3Stream = true;
                        isV3Vnc = false;
                        v3_stream_info->setVisible(true);

                } else if (v3_vnc->isChecked()) {
                        mode = VmConsoleWidget::VNC;
                        isV3Cons = false;
                        isV3Stream = false;
                        isV3Vnc = true;
                        v3_stream_info->setVisible(false);

                } else {
                        isV3Cons = false;
                        isV3Stream = false;
                        isV3Vnc = false;
                }
        }
}
