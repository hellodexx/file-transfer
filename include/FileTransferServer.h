#ifndef FILETRANSFERSERVER_H
#define FILETRANSFERSERVER_H
#include <string>
#include <vector>

namespace Dex {

class FileTransferServer {
public:
	enum class command {
		PULL,
		LIST,
		INVALID
	};

	FileTransferServer();
	~FileTransferServer();

	void runServer();
private:
	void handleClient(int clientSocket);
	int sendFile(int clientSocket, const char* filename);
	int sendFileList(int clientSocket, std::vector<std::string> files);
	bool isFilePattern(const char* filename);
	std::string getBaseName(const std::string& path);
	void splitPathAndPattern(const std::string& filestr, std::string& directory,
                             std::string& pattern);
	std::vector<std::string> getMatchingFiles(const std::string& filestr);
	bool fileExists(const char *path);

	int serverSocket;
	unsigned noOfFilesToSend = 0;
	unsigned sentFilesCounter = 0;
};

} // namespace Dex

#endif // FILETRANSFERSERVER_H
