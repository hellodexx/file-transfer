#ifndef FILETRANSFERCLIENT_H
#define FILETRANSFERCLIENT_H

namespace Dex {

class FileTransferClient {
public:
	void runClient(const char* serverIp, const char* filename);
private:
	void receiveFile(int serverSocket);
	unsigned noOfFilesToRecv = 0;
	unsigned recvdFilesCounter = 0;
};

} // namespace Dex
#endif // FILETRANSFERCLIENT_H
