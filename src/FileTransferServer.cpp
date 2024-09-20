#include "FileTransferServer.h"
#include "packet.h"
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
#define CHUNK_SIZE 1024*16

FileTransferServer::FileTransferServer() : serverSocket(-1) {
	LOGD("Starting...");
}

FileTransferServer::~FileTransferServer() {
	LOGD("Destroying socket...");
	if (serverSocket != -1) {
		close(serverSocket);
	}
}

void FileTransferServer::runServer() {
	struct sockaddr_in serverAddr, clientAddr;
	socklen_t addrLen = sizeof(clientAddr);

	// Create socket
	serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (serverSocket == -1) {
		LOGE("Socket creation failed: %s", strerror(errno));
		return;
	}

	// Attach socket to the port
	int opt = 1;
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

		// Handle connection in a thread
		std::thread clientThread(&FileTransferServer::handleClient, this,
		    clientSocket);
		clientThread.detach();
	}

	LOGI("Closing server socket");
	close(serverSocket);
}

void FileTransferServer::handleClient(int clientSocket) {
	std::vector<std::string> files;
	noOfFilesFound = 0;
	sentFilesCount = 0;

	// Receive command + pattern
	InitPkt initPkt{};
	if (recv(clientSocket, &initPkt, sizeof(initPkt), 0) < 0) {
		LOGE("Receive command + pattern failed: %s", strerror(errno));
		close(clientSocket);
		return;
	}
	LOGD("Received command=%d pattern=[%s]", static_cast<int>(initPkt.command),
	      initPkt.pattern);

	std::string patternStr(initPkt.pattern);

	// If no path '/' symbol in pattern then set default path for android
#ifdef __ANDROID__
	if (strchr(initPkt.pattern, '/') == NULL) {
		patternStr.clear();
		patternStr = "/storage/self/primary/DCIM/Camera/";
		patternStr.append(initPkt.pattern);
		LOGD("Prepended pattern=[%s]", patternStr.c_str());
	}
#endif

	if (isFilePattern(patternStr.c_str())) {
		// Find matching files
		LOGI("Finding matching files: %s", patternStr.c_str());
		files = getMatchingFiles(patternStr);
		noOfFilesFound = files.size();
	} else {
		// Find matching file
		LOGI("Finding file: %s", patternStr.c_str());
		if (fileExists(patternStr.c_str())) {
			noOfFilesFound = 1;
		}
	}

	// Send number of files to client
	InitReplyPkt initReplyPkt{};
	initReplyPkt.noOfFiles = noOfFilesFound;
	LOGD("Sending number of file(s): %d", noOfFilesFound);
	if (send(clientSocket, &initReplyPkt, sizeof(initReplyPkt), 0) < 0) {
		LOGE("Sending number of files failed: %s", strerror(errno));
		close(clientSocket);
		return;
	}

	// Close and return if no files are found
	if (noOfFilesFound == 0) {
		LOGE("No file(s) found: %s", patternStr.c_str());
		close(clientSocket);
		return;
	}

	if (initPkt.command == Command::LIST) {
		// Send file list to client
		sendFileList(clientSocket, files);
	} else {
		if (noOfFilesFound == 1) {
			// Send a single file to client
			LOGD("Sending file: %s", patternStr.c_str());
			sendFile(clientSocket, patternStr.c_str());
		} else {
			// Iterate files
			for (const auto& file : files) {
				// Send file to client
				LOGD("Sending file: %s", file.c_str());
				sendFile(clientSocket, file.c_str());
			}
		}
	}

	LOGI("Total files sent: %d", sentFilesCount);
	LOGI("Closing client connection");
	close(clientSocket);
}

int FileTransferServer::sendFile(int clientSocket, const char *filename) {
	// Wait for client start signal
	LOGD("Waiting for start signal");
	StartSignalPkt startSignalPkt{};
	if (recv(clientSocket, &startSignalPkt, sizeof(startSignalPkt), 0) < 0) {
		LOGE("Receive start signal failed: %s", strerror(errno));
		return -1;
	}

	// Retrieve file status
	struct stat file_stat;
	if (stat(filename, &file_stat) != 0) {
		LOGE("Error getting file status");
		return -1;
	}

	// Construct file info packet
	FileInfoPkt fileInfoPkt{};
	std::string filenameStr = filename;
	std::string baseName = getBaseName(filenameStr);
	memset(fileInfoPkt.name, 0, sizeof(fileInfoPkt.name));
	memcpy(fileInfoPkt.name, baseName.c_str(), baseName.length());
	fileInfoPkt.size = file_stat.st_size;
	fileInfoPkt.time = file_stat.st_mtime;

	// Send file info packet
	LOGD("Sending file name=%s size=%d time=%d", fileInfoPkt.name,
	     fileInfoPkt.size, fileInfoPkt.time);
	if (send(clientSocket, &fileInfoPkt, sizeof(fileInfoPkt), 0) !=
	    sizeof(fileInfoPkt)) {
		LOGE("Send file info failed: %s", strerror(errno));
		return -1;
	}

	// Open file for reading
	FILE *file = fopen(filename, "rb");
	if (!file) {
		LOGE("Error opening file");
		return -1;
	}

	// Send file content
	sentFilesCount += 1;
	LOGI("Sending %d/%d %s...", sentFilesCount, noOfFilesFound, filename);
	char buffer[CHUNK_SIZE] = {0};
	size_t bytesRead = 0;
	ssize_t bytesSent = 0;
	while ((bytesRead = fread(buffer, 1, CHUNK_SIZE, file)) > 0) {
		if (bytesRead < CHUNK_SIZE) {
			if (feof(file)) {
				LOGD("End of file reached.\n");
			} else if (ferror(file)) {
				LOGE("Error reading file");
				sentFilesCount -= 1;
			}
		}

		size_t totalBytesSent = 0;
		while (totalBytesSent < bytesRead) {
			if ((bytesSent = send(clientSocket, buffer + totalBytesSent, bytesRead -
			    totalBytesSent, 0)) < 0) {
				LOGE("Send data failed: %s", strerror(errno));
				break;
			}
			totalBytesSent += bytesSent;
		}

		if (totalBytesSent != bytesRead) {
			LOGE("Error bytes sent not equal to bytes read!");
			sentFilesCount -= 1;
			break;
		}
		memset(buffer, 0, sizeof(buffer));
	}

	// Close the file
	fclose(file);

	LOGI("Send file complete %d/%d %s", sentFilesCount, noOfFilesFound, filename);
	return 0;
}

int FileTransferServer::sendFileList(int clientSocket, std::vector<std::string>
    files) {
	// Wait for client start signal
	LOGD("Waiting for start signal");
	StartSignalPkt startSignalPkt{};
	if (recv(clientSocket, &startSignalPkt, sizeof(startSignalPkt), 0) < 0) {
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
		sentFilesCount += 1;
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
