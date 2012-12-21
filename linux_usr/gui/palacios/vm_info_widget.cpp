#include "newpalacios.h"

VmInfoWidget::VmInfoWidget(QWidget* parent) :
		QWidget(parent) {
	setInfoView();
	setupUi();
}

VmInfoWidget::~VmInfoWidget() {
        delete mVmInfoView;
}

void VmInfoWidget::setInfoView() {
	// Create the main tree widget
	mVmInfoView = new QTreeWidget();
    	mVmInfoView->setColumnCount(2);
    	QStringList headerLabels;
    	headerLabels << "Property" << "Value";
    	mVmInfoView->setHeaderLabels(headerLabels);
}

void VmInfoWidget::setupUi() {
	QVBoxLayout* vmDetailsLayout = new QVBoxLayout(this);
	vmDetailsLayout->addWidget(mVmInfoView);
        setLayout(vmDetailsLayout);
}

void VmInfoWidget::parseAttribute(const QDomElement &element,
                              QTreeWidgetItem* parent) {
    QDomNamedNodeMap atts = element.attributes();

    for (unsigned int i=0; i<atts.length(); i++) {
        QTreeWidgetItem* item = new QTreeWidgetItem(parent);

        item->setText(0, "[" + atts.item(i).nodeName() +"]");
        item->setText(1, "[" + atts.item(i).nodeValue() +"]");
    }
}

void VmInfoWidget::parseText(const QDomElement &element,
                              QTreeWidgetItem* parent) {
    QString value = element.text();
    parent->setText(1, value);
}

void VmInfoWidget::parseElement(const QDomElement &element,
                              QTreeWidgetItem* parent) {

    QTreeWidgetItem* item = new QTreeWidgetItem(parent);

    if (element.tagName().compare("memory") == 0) {
        // Memory tag
        item->setText(0, "memory");
        parseAttribute(element, item);
        parseText(element, item);

    } else if (element.tagName().compare("telemetry") == 0) {
        // Telemetry tag
        item->setText(0, "telemetry");
        parseText(element, item);

    } else if (element.tagName().compare("extensions") == 0) {
        // Extensions tag
        item->setText(0, "extensions");
        QDomNode child = element.firstChild();
        while (!child.isNull()) {
            if (!child.isElement()) {
                child = child.nextSibling();
                continue;
            }

            parseElement(child.toElement(), item);
            child = child.nextSibling();
        }

    } else if (element.tagName().compare("extension") == 0) {
      // Specific Extension
        item->setText(0, "extension");
        parseAttribute(element, item);
        parseText(element, item);

    } else if (element.tagName().compare("paging") == 0) {
        // Paging tag
        item->setText(0, "paging");
        parseAttribute(element, item);
        QDomNode child = element.firstChild();
        while (!child.isNull()) {
            if (!child.isElement()) {
                child = child.nextSibling();
                continue;
            }

            parseElement(child.toElement(), item);
            child = child.nextSibling();
        }

    } else if (element.tagName().compare("strategy") == 0) {
        item->setText(0, "strategy");
        parseText(element, item);

    } else if (element.tagName().compare("large_pages") == 0) {
        item->setText(0, "large_pages");
        parseText(element, item);

    } else if (element.tagName().compare("schedule_hz") == 0) {
        // Schedule Hz
        item->setText(0, "schedule_hz");
        parseText(element, item);

    } else if (element.tagName().compare("cores") == 0) {
        // Cores
        item->setText(0, "cores");
        parseAttribute(element, item);

    } else if (element.tagName().compare("core") == 0) {
        item->setText(0, "core");

    } else if (element.tagName().compare("memmap") == 0) {
        item->setText(0, "memmap");
        QDomNode child = element.firstChild();
        while (!child.isNull()) {

            if (!child.isElement()) {
                child = child.nextSibling();
                continue;
            }

            parseElement(child.toElement(), item);
            child = child.nextSibling();
        }

    } else if (element.tagName().compare("region") == 0) {
        item->setText(0, "region");
        QDomNode child = element.firstChild();
        while (!child.isNull()) {
            if (!child.isElement()) {
                child = child.nextSibling();
                continue;
            }

            parseElement(child.toElement(), item);
            child = child.nextSibling();
        }

    } else if (element.tagName().compare("start") == 0) {
        item->setText(0, "start");
        parseText(element, item);

    } else if (element.tagName().compare("end") == 0) {
        item->setText(0, "end");
        parseText(element, item);

    } else if (element.tagName().compare("host_addr") == 0) {
        item->setText(0, "host_addr");
        parseText(element, item);

    } else if (element.tagName().compare("files") == 0) {
        item->setText(0, "files");
        QDomNode child = element.firstChild();
        while (!child.isNull()) {
            if (!child.isElement()) {
                child = child.nextSibling();
                continue;
            }

            parseElement(child.toElement(), item);
            child = child.nextSibling();
        }

    } else if (element.tagName().compare("file") == 0) {
        item->setText(0, "file");
        parseAttribute(element, item);

    } else if (element.tagName().compare("devices") == 0) {
        // Devices
        item->setText(0, "devices");
        QDomNode child = element.firstChild();
        while (!child.isNull()) {
            if (!child.isElement()) {
                child = child.nextSibling();
                continue;
            }

            parseElement(child.toElement(), item);
            child = child.nextSibling();
        }

    } else if (element.tagName().compare("device") == 0) {
        // Device
        item->setText(0, "device");
        parseAttribute(element, item);
        QDomNode child = element.firstChild();
        while (!child.isNull()) {
            if (!child.isElement()) {
                child = child.nextSibling();
                continue;
            }

            parseElement(child.toElement(), item);
            child = child.nextSibling();
        }

    } else if (element.tagName().compare("storage") == 0) {
        item->setText(0, "storage");
        parseText(element, item);

    } else if (element.tagName().compare("apic") == 0) {
        item->setText(0, "apic");
        parseText(element, item);

    } else if (element.tagName().compare("bus") == 0) {
        // Bus tag, is seen in many device tags

    } else if (element.tagName().compare("vendor_id") == 0) {
        item->setText(0, "vendor_id");
        parseText(element, item);

    } else if (element.tagName().compare("device_id") == 0) {
        item->setText(0, "device_id");
        parseText(element, item);

    } else if (element.tagName().compare("irq") == 0) {
        item->setText(0, "irq");
        parseText(element, item);

    } else if (element.tagName().compare("controller") == 0) {
        // Controller tag, is also seen in some device files
        item->setText(0, "controller");
        parseText(element, item);

    } else if (element.tagName().compare("file") == 0) {
        item->setText(0, "file");
        parseText(element, item);

    } else if (element.tagName().compare("frontend") == 0) {
        item->setText(0, "frontend");
        parseAttribute(element, item);
        QDomNode child = element.firstChild();
        while (!child.isNull()) {
            if (!child.isElement()) {
                child = child.nextSibling();
                continue;
            }

            parseElement(child.toElement(), item);
            child = child.nextSibling();
        }

    } else if (element.tagName().compare("model") == 0) {
        item->setText(0, "model");
        parseText(element, item);

    } else if (element.tagName().compare("type") == 0) {
        item->setText(0, "type");
        parseText(element, item);

    } else if (element.tagName().compare("bus_num") == 0) {
        item->setText(0, "bus_num");
        parseText(element, item);

    } else if (element.tagName().compare("drive_num") == 0) {
        item->setText(0, "drive_num");
        parseText(element, item);

    } else if (element.tagName().compare("path") == 0) {
        item->setText(0, "path");
        parseText(element, item);

    } else if (element.tagName().compare("ip") == 0) {
        item->setText(0, "ip");
        parseText(element, item);

    } else if (element.tagName().compare("port") == 0) {
        item->setText(0, "port");
        parseText(element, item);

    } else if (element.tagName().compare("tag") == 0) {
        item->setText(0, "tag");
        parseText(element, item);

    } else if (element.tagName().compare("size") == 0) {
        item->setText(0, "size");
        parseText(element, item);

    } else if (element.tagName().compare("mac") == 0) {
        item->setText(0, "mac");
        parseText(element, item);

    } else if (element.tagName().compare("name") == 0) {
        item->setText(0, "name");
        parseText(element, item);

    } else if (element.tagName().compare("ports") == 0) {
        item->setText(0, "ports");
        QDomNode child = element.firstChild();
        while (!child.isNull()) {
            if (!child.isElement()) {
                child = child.nextSibling();
                continue;
            }

            parseElement(child.toElement(), item);
            child = child.nextSibling();
        }

    } else if (element.tagName().compare("mode") == 0) {
        item->setText(0, "mode");
        parseText(element, item);

    }
}

void VmInfoWidget::updateInfoView(VmInfo* vm) {
	// Open config file
	QFile file(vm->getVmConfigFile());
    	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        	qDebug() << "File cannot be opened!";
        	return;
    	}

	// Setup DOM parser
    	QDomDocument doc("vm_config");
    	if (!doc.setContent(&file)) {
        	qDebug() << "Error setting content";
        	return;
    	}
	
	file.close();
	
	// Root element
	QTreeWidgetItem* rootItem = new QTreeWidgetItem();
    	QDomElement root = doc.documentElement();
    	
	rootItem->setText(0, root.tagName());
	parseAttribute(root, rootItem);
	mVmInfoView->takeTopLevelItem(0);
    	mVmInfoView->addTopLevelItem(rootItem);

    	// Elements
    	QDomNode n = root.firstChild();
    	while (!n.isNull()) {

        	if (n.isElement()) {
	            	parseElement(n.toElement(), rootItem);
	        }

	        n = n.nextSibling();
    	}

    	mVmInfoView->expandAll();
}

void VmInfoWidget::deleteVm() {
	QTreeWidgetItem* item = mVmInfoView->takeTopLevelItem(0);
	delete item;
}
