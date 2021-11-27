#include <QCoreApplication>
#include "MarkerServer.hpp"

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    MarkerServer server;

    return a.exec();
}
