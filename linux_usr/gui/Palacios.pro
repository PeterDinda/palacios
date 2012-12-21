TEMPLATE = app
TARGET = Palacios
QT += core \
    gui \
    xml
CONFIG += debug_and_release
DEFINES += QTONLY

LIBS += -lvncclient -lgnutls -lX11


HEADERS += palacios/vm_console_widget.h \
    palacios/vm_creation_wizard.h \
    palacios/newpalacios.h \
    palacios/defs.h \
    palacios/vnc_module/remoteview.h \
    palacios/vnc_module/vncclientthread.h \
    palacios/vnc_module/vncview.h
SOURCES += palacios/vm_view.cpp \
    palacios/vm_console_widget.cpp \
    palacios/vm_mode_dialog.cpp \
    palacios/vm_info_widget.cpp \
    palacios/vm_creation_wizard.cpp \
    palacios/main.cpp \
    palacios/newpalacios.cpp \
    palacios/vm_threads.cpp \
    palacios/vnc_module/remoteview.cpp \
    palacios/vnc_module/vncclientthread.cpp \
    palacios/vnc_module/vncview.cpp
FORMS += 
RESOURCES += palacios_resources.qrc
