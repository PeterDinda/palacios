#include "newpalacios.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    NewPalacios w;
    /*int checkSetup = w.checkPalacios();

    if (checkSetup == -1) {
        a.exit();
        return 0;
    } else {
        w.setMinimumSize(820, 640);
        w.showMaximized();
        return a.exec();
    }*/
    
    w.setMinimumSize(820, 640);
    w.showMaximized();
    //w.checkPalacios();
    int eventLoop = a.exec();
    return eventLoop;
}
