#include "FileTransferServer.h"
#include "Logger.h"
#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <cerrno>
// Libraries for getting file data
#include <sys/stat.h>
#include <fcntl.h>
// Multiple connections
#include <thread>
#include <vector>
// Multiple files
#include <dirent.h>
#include <sys/types.h>
#include <fnmatch.h>

namespace Dex {

#define DEFAULT_PORT 9413
#define MAX_CLIENTS 10
#define FILENAME_SIZE 1024
#define CHUNK_SIZE 1024

FileTransferServer::FileTransferServer() : serverSocket(-1) {
}

FileTransferServer::~FileTransferServer() {
	if (serverSocket != -1) {
		close(serverSocket);
	}
}

void FileTransferServer::runServer() {
	struct sockaddr_in serverAddr, clientAddr;
	socklen_t addrLen = sizeof(clientAddr);
	int opt = 1;
	std::vector<std::thread> clientThreads;

	// Create socket file descriptor
	serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (serverSocket == -1) {
		LOGE("Socket creation failed: %s", strerror(errno));
		return;
	}

	// Attach socket to the port
#ifdef __APPLE__
	if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
		LOGE("Set socket options failed: %s", strerror(errno));
		close(serverSocket);
		return;
	}
#else
	if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
	    sizeof(opt))) {
		LOGE("Set socket options failed: %s", strerror(errno));
		close(serverSocket);
		return;
	}
#endif

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = INADDR_ANY; // Use INADDR_ANY to bind to all interfaces
	serverAddr.sin_port = htons(DEFAULT_PORT);

	// Bind the socket to the network address and port
	if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr))
	    < 0) {
		LOGE("Bind failed: %s", strerror(errno));
		close(serverSocket);
		return;
	}

	// Start listening for connections
	if (listen(serverSocket, MAX_CLIENTS) < 0) {
		LOGE("Listen failed: %s", strerror(errno));
		close(serverSocket);
		return;
	}

	LOGI("Server is listening on port %d", DEFAULT_PORT);

	while (true) {
		// Accept a new connection
		int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr,
		                   &addrLen);
		if (clientSocket < 0) {
			LOGE("Server accept failed: %s", strerror(errno));
			close(serverSocket);
			return;
		}
		LOGI("New client connection");

		std::thread clientThread(&FileTransferServer::handleClient, this, clientSocket);
		clientThread.detach();
	}

	LOGI("Closing server socket");
	close(serverSocket);
}

void FileTransferServer::handleClient(int clientSocket) {
	char filename[FILENAME_SIZE] = {0};
	std::vector<std::string> files;
	noOfFilesToSend = 0;
	sentFilesCounter = 0;
	Dex::FileTransferServer::command cmd;

	// Receive command
	if (recv(clientSocket, &cmd, sizeof(cmd), 0) < 0) {
		LOGE("Receive command failed: %s", strerror(errno));
		close(clientSocket);
		return;
	}
	LOGD("Received command: %d", static_cast<int>(cmd));

	// Receive file name from client
	if (recv(clientSocket, filename, FILENAME_SIZE, 0) < 0) {
		LOGE("Receive file name failed: %s", strerror(errno));
		close(clientSocket);
		return;
	}
	LOGD("Received file name: %s", filename);

	if (isFilePattern(filename)) {
		// Find matching files
		LOGI("Finding matching files: %s", filename);
		std::string filenameStr(filename);
		files = getMatchingFiles(filenameStr);
		noOfFilesToSend = files.size();
	} else {
		LOGI("Finding file: %s", filename);
		if (fileExists(filename)) {
			noOfFilesToSend = 1;
		}
	}

	// Send number of files to client
	LOGD("Sending number of file(s): %d", noOfFilesToSend);
	if (send(clientSocket, &noOfFilesToSend, sizeof(noOfFilesToSend), 0) < 0) {
		LOGE("Sending number of files failed: %s", strerror(errno));
		close(clientSocket);
		return;
	}

	if (noOfFilesToSend == 0) {
		LOGE("No file(s) found: %s", filename);
		close(clientSocket);
		return;
	}

	if (cmd == Dex::FileTransferServer::command::LIST) {
		sendFileList(clientSocket, files);
	} else {
		if (noOfFilesToSend == 1) {
			// Send a single file to client
			LOGD("Sending file: %s", filename);
			sendFile(clientSocket, filename);
		} else {
			// Iterate files
			for (const auto& file : files) {
				// Send file to client
				LOGD("Sending file: %s", file.c_str());
				sendFile(clientSocket, file.c_str());
			}
		}
	}

	LOGI("Total sent files: %d", sentFilesCounter);
	LOGI("Closing client connection");
	close(clientSocket);
}

int FileTransferServer::sendFile(int clientSocket, const char *filename) {
	// Wait for client start signal
	LOGD("Waiting for start signal");
	short startSignal = 0;
	if (recv(clientSocket, &startSignal, sizeof(startSignal), 0) < 0) {
		LOGE("Receive start signal failed: %s", strerror(errno));
		return -1;
	}

	// Send base file name
    std::string filenameStr = filename;
	std::string baseName = getBaseName(filenameStr);
	LOGD("Base file name: %s", baseName.c_str());
	if (send(clientSocket, baseName.c_str(), 100, 0) < 0) {
		LOGE("Send file name failed: %s", strerror(errno));
		return -1;
	}

	// Open file descriptor
	int fd = open(filename, O_RDONLY);
	if (fd == -1) {
		LOGE("Error opening file: %s", strerror(errno));
		return -1;
	}

	// Retrieve file information
	struct stat file_stat;
	if (fstat(fd, &file_stat) < 0) {
		LOGE("Error getting file information: %s", strerror(errno));
		close(fd);
		return -1;
	}

	// Send file size
	long long fileSize = file_stat.st_size;
	LOGD("Sending file size: %lld", fileSize);
	if (send(clientSocket, &fileSize, sizeof(fileSize), 0) < 0) {
		LOGE("Send file size failed: %s", strerror(errno));
		return -1;
	}

	// Send file timestamp
	LOGD("Sending file timestamp");
	time_t file_time = file_stat.st_mtime;
	if (send(clientSocket, &file_time, sizeof(time_t), 0) != sizeof(time_t)) {
		LOGE("Send file timestamp failed: %s", strerror(errno));
		return -1;
	}

	// Open file for reading
	FILE *file = fopen(filename, "rb");
	if (!file) {
		LOGE("Error opening file");
		return -1;
	}

	// Send file content
	LOGI("Sending file content");
	unsigned char buffer[CHUNK_SIZE] = {0};
	size_t bytesRead = 0;

	while (true) {
		memset(buffer, 0, CHUNK_SIZE);
		bytesRead = fread(buffer, 1, CHUNK_SIZE, file);
		if (bytesRead <= 0) {
			sentFilesCounter += 1;
			break;
		}

		if (send(clientSocket, buffer, bytesRead, 0) < 0) {
			LOGE("Send data failed: %s", strerror(errno));
			break;
		}
	}

	// Close the file
	fclose(file);

	LOGI("File sent: %d/%d %s", sentFilesCounter, noOfFilesToSend, filename);
	return 0;
}

int FileTransferServer::sendFileList(int clientSocket, std::vector<std::string> files) {
	// Wait for client start signal
	LOGD("Waiting for start signal");
	short startSignal = 0;
	if (recv(clientSocket, &startSignal, sizeof(startSignal), 0) < 0) {
		LOGE("Receive start signal failed: %s", strerror(errno));
		return -1;
	}

	// Iterate files
	for (const auto& file : files) {
		// Send file list to client
		std::string fileStr = file + "\n";
		if (send(clientSocket, fileStr.c_str(), fileStr.length(), 0) < 0) {
			LOGE("Send file list failed: %s", strerror(errno));
			return -1;
		}
		LOGD("Sent: %s", fileStr.c_str());
		sentFilesCounter += 1;
	}

	LOGI("File list sent completed");
	return 0;
}

bool FileTransferServer::isFilePattern(const char *filename) {
	return strchr(filename, '*') != nullptr;
}

std::string FileTransferServer::getBaseName(const std::string &path) {
	// Find the last occurrence of the directory separator
	size_t pos = path.find_last_of("/\\");
	
	// Extract the base name
	if (pos != std::string::npos) {
		return path.substr(pos + 1);
	} else {
		// If no separator is found, return the entire string (assuming it's
		// already a file name)
		return path;
	}
}

void FileTransferServer::splitPathAndPattern(const std::string &filestr,
    std::string &directory, std::string &pattern) {
	size_t last_slash = filestr.find_last_of('/');
	
	if (last_slash != std::string::npos) {
		directory = filestr.substr(0, last_slash);
		pattern = filestr.substr(last_slash + 1);
	} else {
		directory = ".";
		pattern = filestr;
	}
}

std::vector<std::string> FileTransferServer::getMatchingFiles(const std::string
    &filestr) {
	std::string directory, pattern;
	splitPathAndPattern(filestr, directory, pattern);

	std::vector<std::string> matching_files;
	DIR* dir;
	struct dirent* ent;

	if ((dir = opendir(directory.c_str())) != nullptr) {
		while ((ent = readdir(dir)) != nullptr) {
			std::string filename = ent->d_name;
			if (fnmatch(pattern.c_str(), filename.c_str(), 0) == 0) {
				matching_files.push_back(directory + "/" + filename);
			}
		}
		closedir(dir);
	} else {
		LOGE("Could not open directory");
	}

	return matching_files;
}

bool FileTransferServer::fileExists(const char *path) {
	FILE *file = fopen(path, "rb");
	if (file) {
		fclose(file);
		return true;
	} else {
		return false;
	}
}

} // namespace Dex
