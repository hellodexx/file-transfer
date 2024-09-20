#include "FileTransferClient.h"
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

void FileTransferClient::runClient(const char* serverIp, const char* filename,
    Command cmd) {
	int serverSocket;
	struct sockaddr_in serverAddr;
	noOfFilesToRecv = 0;

	// Create socket
	if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
		LOGE("Socket creation failed: %s", strerror(errno));
		return;
	}

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(DEFAULT_PORT);

	if (inet_pton(AF_INET, serverIp, &serverAddr.sin_addr) <= 0) {
		LOGE("Invalid address / Address not supported");
		return;
	}

	// Connect to server
	LOGI("Connecting to server");
	if (connect(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr))
	    < 0) {
		LOGE("Connection failed: %s", strerror(errno));
		return;
	}
	LOGI("Connected to server");

	// Send command + pattern to server
	InitPacket initPkt{};
	initPkt.command = cmd;
	memset(initPkt.pattern, 0, sizeof(initPkt.pattern));
	memcpy(initPkt.pattern, filename, strlen(filename));

	LOGI("Sending command=%d filename=%s", static_cast<int>(cmd), filename);
	if (send(serverSocket, &initPkt, sizeof(initPkt), 0) < 0) {
		LOGE("Send command failed: %s", strerror(errno));
		close(serverSocket);
		return;
	}

	// Receive number of files
	InitReplyPkt initReplyPkt{};
	LOGD("Receiving number of files");
	if (recv(serverSocket, &initReplyPkt, sizeof(initReplyPkt), 0) < 0) {
		LOGE("Receive number of file failed: %s", strerror(errno));
		close(serverSocket);
		return;
	}
	noOfFilesToRecv = initReplyPkt.noOfFiles;

	// Close and return if no files are found
	if (initReplyPkt.noOfFiles == 0) {
		LOGE("No files found with pattern: %s", filename);
		close(serverSocket);
		return;
	}
	LOGD("noOfFiles: %d", initReplyPkt.noOfFiles);

	if (cmd == Command::PULL) {
		// Receive files and save to local
		for (size_t i = 0; i < initReplyPkt.noOfFiles; i++) {
			receiveFile(serverSocket);
		}
		LOGI("Total files downloaded: %d ", recvFilesCount);
	} else if (cmd == Command::LIST) {
		receiveFileList(serverSocket);
		LOGI("Total files found: %d ", initReplyPkt.noOfFiles);
	}

	LOGI("Closing connection");
	close(serverSocket);
	LOGI("Exiting");
}

int FileTransferClient::receiveFile(int serverSocket) {
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
	LOGD("File name=%s size=%d time=%d", fileInfoPkt.name, fileInfoPkt.size,
	      fileInfoPkt.time);

	// Open file for writing
	FILE *file = fopen(fileInfoPkt.name, "wb");
	if (!file) {
		LOGE("Error opening file");
		return -1;
	}

	recvFilesCount += 1;
	LOGI("Downloading %d/%d name=%s size=%zu...", recvFilesCount,
	    noOfFilesToRecv, fileInfoPkt.name, fileInfoPkt.size);

	// Receive the content of the file
	LOGD("Receiving file content");
	char buffer[CHUNK_SIZE] = {0};
	ssize_t bytesRecv = 0;
	size_t totalBytesRecv = 0;
	while (true) {
		bytesRecv = recv(serverSocket, buffer, CHUNK_SIZE, 0);

		if (bytesRecv < 0) {
			LOGE("Receive file chunk failed: %s", strerror(errno));
			recvFilesCount -= 1;
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
		recvFilesCount -= 1;
		return -1;
	}

	LOGI("Download file completed %d/%d %s", recvFilesCount, noOfFilesToRecv,
	      fileInfoPkt.name);

	return 0;
}

int FileTransferClient::receiveFileList(int serverSocket) {
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