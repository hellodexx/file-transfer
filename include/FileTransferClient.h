#ifndef FILETRANSFERCLIENT_H
#define FILETRANSFERCLIENT_H

#include "packet.h"

namespace Dex {

class FileTransferClient {
public:
	FileTransferClient();
	~FileTransferClient();
	void runClient(const char* serverIp, Command cmd, const char* pattern);

private:
	int connectToServer(const char* serverIp);
	int handleCommand(Command cmd, const char* pattern);
	int receiveFile();
	int sendFile(const char* fileName);
	int receiveFileList();

	int serverSocket;
	unsigned totalFiles = 0;
	unsigned fileCount = 0;
};

} // namespace Dex
#endif // FILETRANSFERCLIENT_H
