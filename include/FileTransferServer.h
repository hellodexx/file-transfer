#ifndef FILETRANSFERSERVER_H
#define FILETRANSFERSERVER_H
#include <string>
#include <vector>

namespace Dex {

class FileTransferServer {
public:
	FileTransferServer();
	~FileTransferServer();
	void runServer();

private:
	void handleClient(int clientSocket);
	int sendFile(int clientSocket, const char* filename);
	int receiveFile(int clientSocket, const char* directory);
	int sendFileList(int clientSocket, std::vector<std::string> files);

	int serverSocket;
	unsigned totalFiles = 0;
	unsigned fileCount = 0;
};

} // namespace Dex

#endif // FILETRANSFERSERVER_H
