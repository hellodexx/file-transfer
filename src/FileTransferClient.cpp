#include "FileTransferClient.h"
#include "utils.h"
#include "Logger.h"
#include <iostream>
#include <fstream>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
// Libraries for getting file data
#include <cerrno>
#include <sys/stat.h>
#include <fcntl.h>
#include <utime.h>

namespace Dex {

#define DEFAULT_PORT 9413
#define FILENAME_SIZE 1024
#define CHUNK_SIZE 1024*16

FileTransferClient::FileTransferClient(): serverSocket(-1), totalFiles(0),
    fileCount(0) {
}

FileTransferClient::~FileTransferClient() {
	if (serverSocket != -1) {
		close(serverSocket);
	}
}

void FileTransferClient::runClient(const char* serverIp, Command cmd,
    const char* pattern) {
	// Connect to server
	serverSocket = connectToServer(serverIp);

	// Handle command
	handleCommand(cmd, pattern);

	LOGD("Closing connection");
	close(serverSocket);
	LOGI("Complete");
}

int FileTransferClient::connectToServer(const char* serverIp) {
	int fd;
	struct sockaddr_in serverAddr;

	// Create socket
	if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		LOGE("Socket creation failed: %s", strerror(errno));
		return -1;
	}

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(DEFAULT_PORT);

	if (inet_pton(AF_INET, serverIp, &serverAddr.sin_addr) <= 0) {
		LOGE("Invalid address / Address not supported");
		return -1;
	}

	// Connect to server
	LOGI("Connecting to server");
	if (connect(fd, (struct sockaddr*)&serverAddr, sizeof(serverAddr))
	    < 0) {
		LOGE("Connection to server failed: %s", strerror(errno));
		return -1;
	}
	LOGI("Connected to server");
	return fd;
}

int FileTransferClient::handleCommand(Command cmd, const char *pattern) {
	LOGD("Handle command=%d pattern=%s ", static_cast<int>(cmd), pattern);
	InitPacket initPkt{};
	std::vector<std::string> files;
	totalFiles = 0;

	// Check if local file(s) exist for PUSH command
	if (cmd == Command::PUSH) {
		files = getMatchingFiles(pattern);
		if (files.empty()) {
			LOGI("No files found with pattern=[%s]", pattern);
			return -1;
		}
		totalFiles = files.size();
		initPkt.totalFiles = totalFiles;
		LOGD("totalFiles=[%d]", totalFiles);
	}

	// Send command to server
	LOGI("Sending command=%d pattern=%s totalFiles=%d", static_cast<int>(cmd),
	      pattern, totalFiles);
	initPkt.command = cmd;
	memcpy(initPkt.pattern, pattern, strlen(pattern));

	if (send(serverSocket, &initPkt, sizeof(initPkt), 0) < 0) {
		LOGE("Send command failed: %s", strerror(errno));
		close(serverSocket);
		return -1;
	}

	// Receive init reply
	InitReplyPkt initReplyPkt{};
	LOGD("Receiving init reply");
	if (recv(serverSocket, &initReplyPkt, sizeof(initReplyPkt), 0) < 0) {
		LOGE("Receive init reply failed: %s", strerror(errno));
		close(serverSocket);
		return -1;
	}

	if (cmd == Command::PULL || cmd == Command::LIST) {
		// Close and return if no files are found
		LOGD("initReplyPkt.totalFiles: %d", initReplyPkt.totalFiles);
		totalFiles = initReplyPkt.totalFiles;
		if (totalFiles == 0) {
			LOGE("No files found in server with pattern=%s", pattern);
			close(serverSocket);
			return -1;
		}
	} else if (cmd == Command::PUSH) {
		if (initReplyPkt.proceed == false) {
			LOGE("Error: server did not proceed");
			return -1;
		}
	}

	switch (cmd) {
	case Command::PULL: {
		// Receive file(s) and save to local
		for (size_t i = 0; i < totalFiles; i++) {
			receiveFile();
		}
		LOGI("Total files downloaded: %d ", fileCount);
		break;
	}
	case Command::PUSH: {
		for (const auto& file: files) {
			LOGD("file: %s", file.c_str());
			sendFile(file.c_str());
		}
		break;
	}
	case Command::LIST:
		receiveFileList();
		LOGI("Total files found: %d ", totalFiles);
		break;
	default:
		LOGE("Invalid command=%d", static_cast<int>(cmd));
		return -1;
		break;
	}

	return 0;
}

int FileTransferClient::receiveFile() {
	// Send start signal to server
	LOGD("Sending start signal");
	StartSignalPkt startSignalPkt{};
	startSignalPkt.start = true;
	if (send(serverSocket, &startSignalPkt, sizeof(startSignalPkt), 0) < 0) {
		LOGE("Send start signal failed: %s", strerror(errno));
		return -1;
	}

	// Receive file info packet
	FileInfoPkt fileInfoPkt{};
	LOGD("Receiving file info");
	if (recv(serverSocket, &fileInfoPkt, sizeof(fileInfoPkt), 0) < 0) {
		LOGE("Receive file info failed: %s", strerror(errno));
		return -1;
	}
	LOGD("File name=%s size=%ld time=%ld", fileInfoPkt.name, fileInfoPkt.size,
	      fileInfoPkt.time);

	// Open file for writing
	FILE *file = fopen(fileInfoPkt.name, "wb");
	if (!file) {
		LOGE("Error opening file");
		return -1;
	}

	fileCount += 1;
	LOGI("Downloading %d/%d name=%s size=%zu...", fileCount,
	    totalFiles, fileInfoPkt.name, fileInfoPkt.size);

	// Receive the content of the file
	LOGD("Receiving file content");
	char buffer[CHUNK_SIZE] = {0};
	ssize_t bytesRecv = 0;
	size_t totalBytesRecv = 0;
	while (true) {
		bytesRecv = recv(serverSocket, buffer, CHUNK_SIZE, 0);

		if (bytesRecv < 0) {
			LOGE("Receive file chunk failed: %s", strerror(errno));
			fileCount -= 1;
			break;
		}

		fwrite(buffer, 1, bytesRecv, file);
		totalBytesRecv += bytesRecv;

		if (totalBytesRecv >= fileInfoPkt.size) {
			break;
		}
		memset(buffer, 0, CHUNK_SIZE);
	}
	
	// Close the file
	LOGD("Closing file");
	fclose(file);

	// Copy original file timestamp
	LOGD("Copying original time stamp");
	struct utimbuf new_times;
	new_times.actime = fileInfoPkt.time; // Use the current access time
	new_times.modtime = fileInfoPkt.time; // Set the modification time
	if (utime(fileInfoPkt.name, &new_times) == -1) {
		LOGE("Error copying file timestamp: %s", strerror(errno));
		fileCount -= 1;
		return -1;
	}

	LOGI("Download file completed %d/%d %s", fileCount, totalFiles,
	      fileInfoPkt.name);

	return 0;
}

int FileTransferClient::sendFile(const char* fileName) {
	// Wait for server start signal
	LOGD("Waiting for start signal");
	StartSignalPkt startSignalPkt{};
	if (recv(serverSocket, &startSignalPkt, sizeof(startSignalPkt), 0) < 0) {
		LOGE("Receive start signal failed: %s", strerror(errno));
		return -1;
	}

	if (!startSignalPkt.start) {
		LOGE("Error: server start signal=%d", startSignalPkt.start);
		return -1;
	}

	// Retrieve file status
	struct stat file_stat;
	if (stat(fileName, &file_stat) != 0) {
		LOGE("Error getting file status");
		return -1;
	}

	// Construct file info packet
	FileInfoPkt fileInfoPkt{};
	std::string fileBaseName = getBaseName(fileName);
	memcpy(fileInfoPkt.name, fileBaseName.c_str(), fileBaseName.length());
	fileInfoPkt.size = file_stat.st_size;
	fileInfoPkt.time = file_stat.st_mtime;

	// Send file info packet
	LOGD("Sending file name=%s size=%ld time=%ld...", fileInfoPkt.name,
	     fileInfoPkt.size, fileInfoPkt.time);
	if (send(serverSocket, &fileInfoPkt, sizeof(fileInfoPkt), 0) !=
	    sizeof(fileInfoPkt)) {
		LOGE("Send file info failed: %s", strerror(errno));
		return -1;
	}

	// Open file for reading
	FILE *file = fopen(fileName, "rb");
	if (!file) {
		LOGE("Error opening file");
		return -1;
	}

	// Send file content
	fileCount += 1;
	LOGI("Sending file %d/%d %s...", fileCount, totalFiles, fileName);
	char buffer[CHUNK_SIZE] = {0};
	size_t bytesRead = 0;
	ssize_t bytesSent = 0;
	while ((bytesRead = fread(buffer, 1, CHUNK_SIZE, file)) > 0) {
		if (bytesRead < CHUNK_SIZE) {
			if (feof(file)) {
				LOGD("End of file reached.");
			} else if (ferror(file)) {
				LOGE("Error reading file");
				fileCount -= 1;
			}
		}

		size_t totalBytesSent = 0;
		while (totalBytesSent < bytesRead) {
			if ((bytesSent = send(serverSocket, buffer + totalBytesSent, bytesRead -
			    totalBytesSent, 0)) < 0) {
				LOGE("Send data failed: %s", strerror(errno));
				break;
			}
			totalBytesSent += bytesSent;
		}

		if (totalBytesSent != bytesRead) {
			LOGE("Error bytes sent not equal to bytes read!");
			fileCount -= 1;
			break;
		}
		memset(buffer, 0, sizeof(buffer));
	}

	// Close the file
	fclose(file);

	LOGI("Send file complete %d/%d %s", fileCount, totalFiles, fileName);
	return 0;
}

int FileTransferClient::receiveFileList() {
	char buffer[CHUNK_SIZE] = {0};

	// Send start signal to server
	LOGD("Sending start signal");
	StartSignalPkt startSignalPkt{};
	startSignalPkt.start = true;
	if (send(serverSocket, &startSignalPkt, sizeof(startSignalPkt), 0) < 0) {
		LOGE("Send start signal failed: %s", strerror(errno));
		return -1;
	}

	// Receive file list
	LOGD("Receiving file list");
	while (true) {
		memset(buffer, 0, sizeof(buffer));
		size_t bytesRecv;
		if ((bytesRecv = recv(serverSocket, buffer, CHUNK_SIZE, 0)) < 0) {
			break;
		}
		if (bytesRecv) {
			LOGI("Receive: %s", buffer);
		} else {
			break;
		}
	}

	return 0;
}

} // namespace Dex