#ifndef FILETRANSFERCLIENT_H
#define FILETRANSFERCLIENT_H

#include "packet.h"

namespace Dex {

class FileTransferClient {
public:
	void runClient(const char* serverIp, const char* filename, Command cmd);
private:
	int receiveFile(int serverSocket);
	int receiveFileList(int serverSocket);
	unsigned noOfFilesToRecv = 0;
	unsigned recvFilesCount = 0;
};

} // namespace Dex
#endif // FILETRANSFERCLIENT_H
