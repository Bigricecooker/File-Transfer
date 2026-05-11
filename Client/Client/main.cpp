#include "Client.h"
#include "MainWidget.h"
#include <QtWidgets/QApplication>


#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    static bool wsInit = [] {
        WSADATA wsa;
        return WSAStartup(MAKEWORD(2, 2), &wsa) == 0;
        }();


 //   SOCKET sock = INVALID_SOCKET;
	//MainWidget window(sock, nullptr);
 //   window.show();

    Client window;
    window.show();
    return app.exec();
}
