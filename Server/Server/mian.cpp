#include <winsock2.h>
#include <string>
#include "Server.h"

#pragma comment(lib, "ws2_32.lib") 


int main(int argc, char* argv[])
{
	int port = 8080; // 默认端口
 if (argc > 1) {
		port = std::stoi(argv[1]);
	}

	Server server;
	server.SetListenPort(static_cast<unsigned short>(port));
	server.Start();
	return 0;
}