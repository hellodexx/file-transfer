#ifndef FILETRANSFERCLIENT_H
#define FILETRANSFERCLIENT_H

namespace Dex {

class FileTransferClient {
public:
	enum class command {
		PULL,
		LIST,
		INVALID
	};

	void runClient(const char* serverIp, const char* filename, command cmd);
private:
	int receiveFile(int serverSocket);
	int receiveFileList(int serverSocket);
	unsigned noOfFilesToRecv = 0;
	unsigned recvdFilesCounter = 0;
};

} // namespace Dex
#endif // FILETRANSFERCLIENT_H
