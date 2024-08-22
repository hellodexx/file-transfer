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

#define PORT 9413
#define BUFFER_SIZE 1024

void FileTransferClient::runClient(const char* serverIp, const char* filename) {
	int serverSocket;
	struct sockaddr_in serverAddr;
	noOfFilesToRecv = 0;

	serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (serverSocket < 0) {
		LOGE("Socket creation failed: %s", strerror(errno));
		return;
	}

	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(PORT);

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
	LOGI("Filename=%s", filename);
	send(serverSocket, filename, BUFFER_SIZE, 0);

	// Receive number of files
	LOGD("Receiving number of files");
	int bytesRecv = recv(serverSocket, &noOfFilesToRecv, sizeof(noOfFilesToRecv), 0);
	if (bytesRecv <= 0) {
		LOGE("Error receiving file name: %s", strerror(errno));
		return;
	}
	LOGD("noOfFilesToRecv: %d", noOfFilesToRecv);

	if (noOfFilesToRecv == 0) {
		LOGE("No files found with pattern: %s", filename);
		return;
	}

	for (size_t i = 0; i < noOfFilesToRecv; i++) {	
		// Receive the file and save to local
		receiveFile(serverSocket);
	}

	LOGI("Total received files: %d ", recvdFilesCounter);
	LOGI("Closing connection");
	close(serverSocket);
	LOGI("Exiting");
}

void FileTransferClient::receiveFile(int serverSocket) {
	char fileName[100] = {0};

	// Send start signal to server
	LOGD("Sending start signal");
	short startSignal = 0;
	send(serverSocket, &startSignal, sizeof(startSignal), 0);

	// Receive file name
	LOGD("Receiving file name");
	int bytesRecv = recv(serverSocket, fileName, 100, 0);
	if (bytesRecv <= 0) {
		LOGE("Error receiving file name");
		return;
	}
	LOGD("File name: %s", fileName);

	// Receive file size
	LOGD("Receiving file size");
	long long fileSize = 0;
	bytesRecv = recv(serverSocket, &fileSize, sizeof(fileSize), 0);
	if (bytesRecv <= 0) {
		LOGE("Error receiving file size");
		return;
	}
	LOGD("File size: %lld", fileSize);

	// Receive file timestamp
	LOGD("Receiving file time stamp");
	time_t file_time;
	if (recv(serverSocket, &file_time, sizeof(time_t), 0) != sizeof(time_t)) {
		LOGE("Error receiving file timestamp");
		return;
	}

	// Open a file for writing
	FILE *file = fopen(fileName, "wb");
	if (!file) {
		LOGE("Error opening file");
		return;
	}

	// Receive the content of the file
	LOGD("Receiving file content");
	unsigned char buffer[BUFFER_SIZE] = {0};
	ssize_t bytesRecvd = 0;
	long long totalBytes = 0;
	while (true) {
		memset(buffer, 0, BUFFER_SIZE);
		bytesRecvd = recv(serverSocket, buffer, BUFFER_SIZE, 0);
		totalBytes += bytesRecvd;
		if (bytesRecvd <= 0) {
			LOGE("Error on read");
			break;
		}

		fwrite(buffer, 1, bytesRecvd, file);

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
	}

	recvdFilesCounter += 1;
	LOGI("File received: %d/%d %s", recvdFilesCounter, noOfFilesToRecv,
	      fileName);
}

} // namespace Dex