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
#define CHUNK_SIZE 1024

void FileTransferClient::runClient(const char* serverIp, const char* filename) {
	int serverSocket;
	struct sockaddr_in serverAddr;
	noOfFilesToRecv = 0;

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

	LOGI("Connecting to server");
	if (connect(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr))
	    < 0) {
		LOGE("Connection failed: %s", strerror(errno));
		return;
	}

	// Send file name to the server
	LOGI("File name: %s", filename);
	if (send(serverSocket, filename, FILENAME_SIZE, 0) < 0) {
		LOGE("Send file name failed: %s", strerror(errno));
		close(serverSocket);
		return;
	}

	// Receive number of files
	LOGD("Receiving number of files");
	if (recv(serverSocket, &noOfFilesToRecv, sizeof(noOfFilesToRecv), 0) < 0) {
		LOGE("Receive number of file failed: %s", strerror(errno));
		close(serverSocket);
		return;
	}

	if (noOfFilesToRecv == 0) {
		LOGE("No files found with pattern: %s", filename);
		close(serverSocket);
		return;
	}

	LOGD("noOfFilesToRecv: %d", noOfFilesToRecv);

	for (size_t i = 0; i < noOfFilesToRecv; i++) {	
		// Receive the file and save to local
		receiveFile(serverSocket);
	}

	LOGI("Total received files: %d ", recvdFilesCounter);
	LOGI("Closing connection");
	close(serverSocket);
	LOGI("Exiting");
}

int FileTransferClient::receiveFile(int serverSocket) {
	char fileName[100] = {0};

	// Send start signal to server
	LOGD("Sending start signal");
	short startSignal = 0;
	if (send(serverSocket, &startSignal, sizeof(startSignal), 0) < 0) {
		LOGE("Send start signal failed: %s", strerror(errno));
		return -1;
	}

	// Receive file name
	LOGD("Receiving file name");
	if (recv(serverSocket, fileName, 100, 0) < 0) {
		LOGE("Receive file name failed: %s", strerror(errno));
		return -1;
	}

	// Receive file size
	LOGD("Receiving file size");
	long long fileSize = 0;
	if (recv(serverSocket, &fileSize, sizeof(fileSize), 0) < 0) {
		LOGE("Receive file size failed: %s", strerror(errno));
		return -1;
	}
	LOGD("File size: %lld", fileSize);

	// Receive file timestamp
	LOGD("Receiving file time stamp");
	time_t file_time;
	if (recv(serverSocket, &file_time, sizeof(time_t), 0) != sizeof(time_t)) {
		LOGE("Receive file timestamp failed: %s", strerror(errno));
		return -1;
	}

	// Open file for writing
	FILE *file = fopen(fileName, "wb");
	if (!file) {
		LOGE("Error opening file");
		return -1;
	}

	// Receive the content of the file
	LOGD("Receiving file content");
	unsigned char buffer[CHUNK_SIZE] = {0};
	ssize_t bytesRecv = 0;
	long long totalBytes = 0;
	while (true) {
		memset(buffer, 0, CHUNK_SIZE);
		bytesRecv = recv(serverSocket, buffer, CHUNK_SIZE, 0);
		if (bytesRecv < 0) {
			LOGE("Receive file chunk failed: %s", strerror(errno));
			break;
		}

		fwrite(buffer, 1, bytesRecv, file);

		totalBytes += bytesRecv;
		if (totalBytes >= fileSize) {
			break;
		}
	}
	
	// Close the file
	LOGD("Closing file");
	fclose(file);

	// Copy original file timestamp
	LOGD("Copying original time stamp");
	struct utimbuf new_times;
	new_times.actime = file_time; // Use the current access time
	new_times.modtime = file_time; // Set the modification time
	if (utime(fileName, &new_times) == -1) {
		LOGE("Error copying file timestamp: %s", strerror(errno));
		return -1;
	}

	recvdFilesCounter += 1;
	LOGI("File received: %d/%d %s", recvdFilesCounter, noOfFilesToRecv,
	      fileName);

	return 0;
}

} // namespace Dex