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

#define PORT 9413
#define BUFFER_SIZE 1024

void FileTransferServer::runServer() {
	int serverSocket;
	struct sockaddr_in serverAddr, clientAddr;
	socklen_t addrLen = sizeof(clientAddr);
	int opt = 1;
	std::vector<std::thread> clientThreads;

	serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (serverSocket == -1) {
		LOGE("Socket creation failed: %s", strerror(errno));
		return;
	}

#ifdef __APPLE__
	if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
		LOGE("Set socket options failed: %s", strerror(errno));
		close(serverSocket);
	}
#else
	if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt,
	    sizeof(opt))) {
		LOGE("Set socket options failed: %s", strerror(errno));
		close(serverSocket);
	}
#endif

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_addr.s_addr = INADDR_ANY; // Use INADDR_ANY to bind to all
	                                         // interfaces
	serverAddr.sin_port = htons(PORT);

	if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr))
	    < 0) {
		LOGE("Bind failed: %s", strerror(errno));
		close(serverSocket);
		return;
	}

	if (listen(serverSocket, 3) < 0) {
		LOGE("Listen failed: %s", strerror(errno));
		close(serverSocket);
		return;
	}

	LOGI("Server is listening on port %d", PORT);

	while (true) {
		int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr,
		                   &addrLen);
		if (clientSocket < 0) {
			LOGE("Server accept failed: %s", strerror(errno));
			close(serverSocket);
			return;
		}
		LOGI("New client connection");

		noOfFilesToSend = 0;
		sentFilesCounter = 0;

		clientThreads.emplace_back([clientSocket, this]() {
			char filename[BUFFER_SIZE] = {0};

			// Receive file name from client
			recv(clientSocket, filename, BUFFER_SIZE, 0);
			LOGD("File name = %s", filename);

			// Check if multiple files pattern
			if (isMultipleFiles(filename)) {
				std::string filenameStr(filename);

				LOGI("Getting files with pattern: %s", filename);
				// Check matching files and save to list
				std::vector<std::string> files = getMatchingFiles(filenameStr);

				// Send number of files client
				noOfFilesToSend = files.size();

				LOGD("Sending number of files: %d", noOfFilesToSend);
				send(clientSocket, &noOfFilesToSend, sizeof(noOfFilesToSend), 0);

				if (noOfFilesToSend == 0) {
					LOGE("No files found with pattern: %s", filename);
					close(clientSocket);
					return;
				}

				// Iterate files
				for (const auto& file : files) {
					LOGD("file: %s", file.c_str());
					// Send file to client
					sendFile(clientSocket, file.c_str());
				}
			} else {
				LOGD("Single file");
				// Send number of files to send to client
				noOfFilesToSend = 1;
				LOGD("Sending number of files: %d", noOfFilesToSend);
				send(clientSocket, &noOfFilesToSend, sizeof(noOfFilesToSend), 0);

				// Send file to client
				sendFile(clientSocket, filename);
			}

			LOGI("Total sent files: %d", sentFilesCounter);

			LOGI("Closing client connection");
			close(clientSocket);
		});

		// Remove the last added thread
		if (!clientThreads.empty()) {
			clientThreads.back().join(); // Ensure the thread is joined before
			                             // removing
			clientThreads.pop_back(); // Remove the last thread
		}
	}

	// Optionally, join all client threads
	for (auto& thread : clientThreads) {
		if (thread.joinable()) {
			thread.join();
		}
	}

	LOGI("Closing server socket");
	close(serverSocket);
}

void FileTransferServer::sendFile(int clientSocket, const char* filename) {
	std::string filenameStr = filename;
	int bytesSent = 0;

	// Wait for client start signal
	LOGD("Waiting for start signal");
	short startSignal = 0;
	recv(clientSocket, &startSignal, sizeof(startSignal), 0);

	// Open the file
	int fd = open(filename, O_RDONLY);
	if (fd == -1) {
		LOGE("Error opening file: %s", strerror(errno));
		return;
	}

	// Retrieve file information
	struct stat file_stat;
	if (fstat(fd, &file_stat) < 0) {
		LOGE("Error getting file information: %s", strerror(errno));
		close(fd);
		return;
	}

	// Send file name
	std::string baseName = getBaseName(filenameStr);
	LOGD("Base file name: %s", baseName.c_str());
	bytesSent = send(clientSocket, baseName.c_str(), 100, 0);
	LOGD("bytesSent: %d", bytesSent);

	// Send file size
	long long fileSize = file_stat.st_size;
	LOGD("Sending file size: %lld", fileSize);
	bytesSent = send(clientSocket, &fileSize, sizeof(fileSize), 0);
	LOGD("bytesSent: %d", bytesSent);

	// Send file timestamp
	LOGD("Sending file timestamp");
	time_t file_time = file_stat.st_mtime;
	bytesSent = send(clientSocket, &file_time, sizeof(time_t), 0);
	if (bytesSent != sizeof(time_t)) {
		LOGE("Error sending file timestamp");
		close(fd);
		return;
	}
	LOGD("bytesSent: %d", bytesSent);

	// Open file for reading
	FILE *file = fopen(filename, "rb");
	if (!file) {
		LOGE("Error opening file");
		return;
	}
	// Send file content
	LOGI("Sending file content");
	unsigned char buffer[BUFFER_SIZE] = {0};
	size_t bytesRead = 0;

	while (true) {
		// LOGI("while");
		memset(buffer, 0, BUFFER_SIZE);
		bytesRead = fread(buffer, 1, BUFFER_SIZE, file);
		if (bytesRead <= 0)
			break;
		ssize_t bytesSent = send(clientSocket, buffer, bytesRead, 0);
		if (bytesSent == -1) {
			LOGE("Error sending data");
			break;
		}
	}

	// Close the file
	fclose(file);

	sentFilesCounter += 1;
	LOGI("File sent: %d/%d %s", sentFilesCounter, noOfFilesToSend,
	      filename);
}

bool FileTransferServer::isMultipleFiles(const char *filename) {
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

} // namespace Dex
