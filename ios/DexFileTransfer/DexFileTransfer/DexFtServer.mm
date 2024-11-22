//
//  DexFtServer.m
//  Dex File Transfer
//
//  Created by Dexter on 11/22/24.
//

#import <Foundation/Foundation.h>
#import <Photos/Photos.h>
#import <AVFoundation/AVFoundation.h>
#include "DexFtServer.h"
#include <iostream>
#include <map>

#include "utils.h"
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
#include <utime.h>
// Multiple connections
#include <thread>
#include <vector>
// Multiple files
#include <dirent.h>
#include <sys/types.h>
#include <fnmatch.h>

std::map<std::string, PHAsset *> fetchMediaMatchingPatternMap(const std::string& pattern) {
    printf("fetchMediaMatchingPatternMap\n");
    // Map to store file names and corresponding PHAsset objects
    std::map<std::string, PHAsset *> matchingAssets;

    // Ensure the user has granted access to the Photo Library
    PHAuthorizationStatus status = [PHPhotoLibrary authorizationStatus];
    if (status != PHAuthorizationStatusAuthorized) {
        NSLog(@"Photo library access denied.");
        return matchingAssets;
    }

    // Prepare the fetch options
    PHFetchOptions *fetchOptions = [[PHFetchOptions alloc] init];

    // Build a regex pattern
    std::string regexPattern = pattern;
    std::replace(regexPattern.begin(), regexPattern.end(), '*', '.'); // Convert '*' to '.'
    regexPattern += ".*"; // Ensure the wildcard matches the rest of the string
    NSRegularExpression *regex = [NSRegularExpression regularExpressionWithPattern:
                                   [NSString stringWithUTF8String:regexPattern.c_str()]
                                                                          options:0
                                                                            error:nil];

    // Fetch all media assets
    PHFetchResult<PHAsset *> *allAssets = [PHAsset fetchAssetsWithOptions:fetchOptions];

    for (PHAsset *asset in allAssets) {
        // Format the creation date to "yyyymmdd_hhmmss"
        NSDateFormatter *dateFormatter = [[NSDateFormatter alloc] init];
        [dateFormatter setDateFormat:@"yyyyMMdd_HHmmss"];
        NSString *formattedDate = [dateFormatter stringFromDate:asset.creationDate];

        // Match the creation date with the pattern
        NSTextCheckingResult *match = [regex firstMatchInString:formattedDate
                                                        options:0
                                                          range:NSMakeRange(0, formattedDate.length)];
        if (match) {
            // Determine file extension based on asset type
            NSString *fileExtension = (asset.mediaType == PHAssetMediaTypeImage) ? @".jpg" : @".mp4";
            NSString *fileName = [formattedDate stringByAppendingString:fileExtension];

            NSLog(@"Generated File Name: %@", fileName);

            // Save the file name and the asset in the map
            matchingAssets[fileName.UTF8String] = asset;
        }
    }

    return matchingAssets;
}

namespace Dex {

void FileTransferServer::foo() {
    printf("foo\n");
    std::string searchPattern = "20241116*"; // Example pattern for "yyyymmdd_*"
    std::map<std::string, PHAsset *> result = fetchMediaMatchingPatternMap(searchPattern);
    
    // Loop to all match assets
    for (const auto& [fileName, asset] : result) {
        NSLog(@"Found file: %s, Asset: %@", fileName.c_str(), asset);

        // Fetch resources for the asset
        NSArray<PHAssetResource *> *resources = [PHAssetResource assetResourcesForAsset:asset];
        for (PHAssetResource *resource in resources) {
            // Skip non-primary resources (e.g., auxiliary resources like thumbnails)
            if (resource.type != PHAssetResourceTypePhoto && resource.type != PHAssetResourceTypeVideo) {
                continue;
            }
            
            // Print the original file name
            NSLog(@"Original file name: %@", resource.originalFilename);

            // Retrieve the approximate file size
            NSNumber *fileSize = [resource valueForKey:@"fileSize"];
            if (fileSize) {
                NSLog(@"File size: %@ bytes", fileSize);
            } else {
                NSLog(@"File size not available");
            }

            // Print asset creation date
            if (asset.creationDate) {
                NSLog(@"Creation date: %@", asset.creationDate);

                // Convert creationDate to a timestamp
                NSTimeInterval creationTimestamp = [asset.creationDate timeIntervalSince1970];
                time_t creationTime = static_cast<time_t>(creationTimestamp); // Cast to time_t for comparison
                NSLog(@"Asset creation timestamp: %ld", creationTime);
            }

            // Retrieve additional resource metadata
            NSString *uti = resource.uniformTypeIdentifier;
            NSLog(@"Uniform Type Identifier: %@", uti);

            // Read the file by chunks
            PHAssetResourceManager *manager = [PHAssetResourceManager defaultManager];
            NSMutableData *fileData = [NSMutableData data];

            // Declare readError as __block to modify it inside the block
            __block NSError *readError = nil;

            dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);

            [manager requestDataForAssetResource:resource
                                         options:nil
                               dataReceivedHandler:^(NSData * _Nonnull data) {
                                   [fileData appendData:data];

                                   // Process data in chunks of 1024 bytes
                                   const size_t chunkSize = 1024;
                                   for (size_t i = 0; i < data.length; i += chunkSize) {
//                                       NSData *chunk = [data subdataWithRange:NSMakeRange(i, MIN(chunkSize, data.length - i))];
//                                        NSLog(@"Read chunk: %lu bytes", chunk.length);
                                       // Here, you can process the chunk as needed
                                   }
                               }
                               completionHandler:^(NSError * _Nullable error) {
                                   readError = error;
                                   dispatch_semaphore_signal(semaphore);
                               }];
            dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);

            if (readError) {
                NSLog(@"Error reading file: %@", readError.localizedDescription);
            } else {
                NSLog(@"Finished reading file: %s, Total size: %lu bytes", fileName.c_str(), fileData.length);
            }
        }
    }
}

#define DEFAULT_PORT 9413
#define MAX_CLIENTS 1
#define FILENAME_SIZE 1024
#define CHUNK_SIZE 1024*16

FileTransferServer::FileTransferServer() : serverSocket(-1),
    totalFiles(0), running(true) {
    LOGD("Starting server...");
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

    // Create server socket
    if ((serverSocket = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
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
    LOGD("Server is listening on port %d", DEFAULT_PORT);

    running = true;
    while (running) {
        // Accept a new connection
        int clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr,
                           &addrLen);
        if (!running) {
            break; // Exit if the server is stopped during accept
        }

        if (clientSocket < 0) {
            LOGE("Server accept failed: %s", strerror(errno));
            close(serverSocket);
            return;
        }
        
        LOGI("New client connection");

        // Handle connection in a separate thread
        std::thread clientThread(&FileTransferServer::handleClient, this,
            clientSocket);
        clientThread.detach();
    }

    LOGI("Closing server socket");
    close(serverSocket);
}

void FileTransferServer::handleClient(int clientSocket) {
    std::vector<std::string> files;
    totalFiles = 0;
    fileCount = 0;
    Command cmd;
    ssize_t bytesRecv = 0;
    ssize_t bytesSent = 0;

    // Receive command and pattern
    InitPkt initPkt{};
    if ((bytesRecv= recv(clientSocket, &initPkt, sizeof(initPkt), 0)) !=
        sizeof(initPkt)) {
        if (bytesRecv < 0)
            LOGE("Receive command and pattern failed: %s", strerror(errno));
        else
            LOGE("Receive command and pattern failed bytesRecv=%zu", bytesRecv);
        close(clientSocket);
        return;
    }
    cmd = initPkt.command;
    totalFiles = initPkt.totalFiles;
    std::string patternStr(initPkt.pattern);
    LOGI("Received command=%d pattern=[%s] totalFiles=%d", static_cast<int>(cmd),
          patternStr.c_str(), totalFiles);

    // If no path '/' symbol in pattern then set default path for android
#ifdef __ANDROID__
    if (strchr(initPkt.pattern, '/') == NULL) {
        patternStr.clear();
        patternStr = "/storage/self/primary/DCIM/Camera/";
        patternStr.append(initPkt.pattern);
        LOGD("Prepended pattern=[%s]", patternStr.c_str());
    }
#endif

    if (cmd == Command::PULL || cmd == Command::LIST) {
        if (isFilePattern(patternStr.c_str())) {
            // Find matching pattern
            LOGI("Finding matching files: %s...", patternStr.c_str());
//            files = getMatchingFiles(patternStr);
//            files = fetchMediaMatchingPattern(patternStr);
//            totalFiles = files.size();
            matchAssetsMap = fetchMediaMatchingPatternMap(patternStr);
            totalFiles = static_cast<unsigned>(matchAssetsMap.size());
        } else {
            // Find matching file
            LOGI("Finding file: %s", patternStr.c_str());
            if (fileExists(patternStr.c_str())) {
                totalFiles = 1;
            }
        }

        // Send number of found files to client
        InitReplyPkt initReplyPkt{};
        initReplyPkt.proceed = totalFiles > 0 ? true : false;
        initReplyPkt.totalFiles = totalFiles;
        LOGD("Sending number of file(s): %d", totalFiles);
        if ((bytesSent = send(clientSocket, &initReplyPkt,
            sizeof(initReplyPkt), 0)) != sizeof(initReplyPkt)) {
            if (bytesSent < 0)
                LOGE("Sending number of files failed: %s", strerror(errno));
            else
                LOGE("Sending number of files failed bytesSent=%zu", bytesSent);
            close(clientSocket);
            return;
        }

        // Close and return if no files are found
        if (totalFiles == 0) {
            LOGE("No file(s) found: %s", patternStr.c_str());
            close(clientSocket);
            return;
        }
    } else if (cmd == Command::PUSH) {
        // Send init reply to client
        InitReplyPkt initReplyPkt{};
        initReplyPkt.proceed = true;
        LOGD("Sending init reply...");
        if ((bytesSent = send(clientSocket, &initReplyPkt,
            sizeof(initReplyPkt), 0)) != sizeof(initReplyPkt)) {
            if (bytesSent < 0)
                LOGE("Sending init reply failed: %s", strerror(errno));
            else
                LOGE("Sending init reply failed bytesSent=%zu", bytesSent);
            close(clientSocket);
            return;
        }
    }

    switch (cmd) {
    case Command::PULL: // Client will receive file from server
    {
        if (totalFiles == 1 && !isFilePattern(patternStr.c_str())) {
            // Send a single file to client
            LOGD("Sending file: %s", patternStr.c_str());
            sendFile(clientSocket, patternStr.c_str());
        } else {
//            // Send multiple files to client
//            for (const auto& file : files) {
//                LOGD("Sending file=%s", file.c_str());
//                sendFile(clientSocket, file.c_str());
//            }
            
            // Loop to all match assets and send to client
            for (const auto& [fileName, asset] : matchAssetsMap) {
                LOGD("Sending file=%s", fileName.c_str());
                NSLog(@"Found file: %s, Asset: %@", fileName.c_str(), asset);
                sendFileIOS(clientSocket, fileName.c_str(), asset);
            }
            
        }
        LOGI("Total files sent: %d", fileCount);
        break;
    }
    case Command::PUSH: // Client will send file(s) to server
    {
        // Upload
        std::string dirStr;
        // Create default directory for saving the file to receive
#ifdef __ANDROID__
        dirStr = "/storage/self/primary/DCIM/DexFileTransfer";
#else
        dirStr = "DexFileTransfer";
#endif
        if (createDirectory(dirStr.c_str()) != 0) {
            break;
        }

        // Receive file(s)
        for (size_t i = 0; i < totalFiles; i++) {
            receiveFile(clientSocket, dirStr.c_str());
        }
        LOGI("Total files received: %d", fileCount);
        break;
    }
    case Command::LIST: // Client will receive file list from server
    {
        // Send file list to client
        sendFileList(clientSocket, files);
        LOGI("Total files sent: %d", fileCount);
        break;
    }
    default:
        LOGE("Invalid command=%d", static_cast<int>(cmd));
        return;
        break;
    }
    
    LOGI("Closing client connection");
    close(clientSocket);
}

int FileTransferServer::sendFile(int clientSocket, const char *filename) {
    // Wait for client start signal
    LOGD("Waiting for start signal");
    StartSignalPkt startSignalPkt{};
    ssize_t bytesRecv = 0;
    ssize_t bytesSent = 0;

    if ((bytesRecv = recv(clientSocket, &startSignalPkt,
        sizeof(startSignalPkt), 0)) != sizeof(startSignalPkt)) {
        if (bytesRecv < 0)
            LOGE("Receive start signal failed: %s", strerror(errno));
        else
            LOGE("Receive start signal failed bytesRecv=%zu", bytesRecv);
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
    std::string baseName = getBaseName(filename);
    memset(fileInfoPkt.name, 0, sizeof(fileInfoPkt.name));
    memcpy(fileInfoPkt.name, baseName.c_str(), baseName.length());
    fileInfoPkt.size = file_stat.st_size;
    fileInfoPkt.time = file_stat.st_mtime;

    // Send file info packet to client
    LOGD("Sending file name=%s size=%ld time=%ld", fileInfoPkt.name,
         fileInfoPkt.size, fileInfoPkt.time);
    if ((bytesSent = send(clientSocket, &fileInfoPkt, sizeof(fileInfoPkt), 0))
        != sizeof(fileInfoPkt)) {
        if (bytesSent < 0)
            LOGE("Send file info failed: %s", strerror(errno));
        else
            LOGE("Send file info failed bytesSent=%zu", bytesSent);
        return -1;
    }

    // Open file for reading
    FILE *file = fopen(filename, "rb");
    if (!file) {
        LOGE("Error opening file");
        return -1;
    }

    // Send file content
    fileCount += 1;
    LOGI("Sending %d/%d %s...", fileCount, totalFiles, filename);
    char buffer[CHUNK_SIZE] = {0};
    size_t bytesRead = 0;
    size_t totalBytesSent = 0;
    while ((bytesRead = fread(buffer, 1, CHUNK_SIZE, file)) > 0) {
        if (bytesRead < CHUNK_SIZE) {
            if (feof(file)) {
                LOGD("End of file reached.");
            } else if (ferror(file)) {
                LOGE("Error reading file");
                fileCount -= 1;
            }
        }

        totalBytesSent = 0;
        while (totalBytesSent < bytesRead) {
            if ((bytesSent = send(clientSocket, buffer + totalBytesSent,
                bytesRead - totalBytesSent, 0)) < 0) {
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

    LOGI("Send file complete %d/%d %s", fileCount, totalFiles, filename);
    return 0;
}

int FileTransferServer::sendFileIOS(int clientSocket, const char *fileName, PHAsset *asset) {
    // Wait for client start signal
    LOGD("Waiting for start signal");
    StartSignalPkt startSignalPkt{};
    ssize_t bytesRecv = 0;
    __block ssize_t bytesSent = 0;

    if ((bytesRecv = recv(clientSocket, &startSignalPkt,
        sizeof(startSignalPkt), 0)) != sizeof(startSignalPkt)) {
        if (bytesRecv < 0)
            LOGE("Receive start signal failed: %s", strerror(errno));
        else
            LOGE("Receive start signal failed bytesRecv=%zu", bytesRecv);
        return -1;
    }

    // Fetch resources for the asset
    NSArray<PHAssetResource *> *resources = [PHAssetResource assetResourcesForAsset:asset];
    for (PHAssetResource *resource in resources) {
        // Skip non-primary resources (e.g., auxiliary resources like thumbnails)
        if (resource.type != PHAssetResourceTypePhoto && resource.type != PHAssetResourceTypeVideo) {
            continue;
        }
        
        // Print the original file name
        NSLog(@"Original file name: %@", resource.originalFilename);

        // Retrieve the approximate file size
        NSNumber *fileSize = [resource valueForKey:@"fileSize"];
        if (fileSize) {
            NSLog(@"File size: %@ bytes", fileSize);
        } else {
            NSLog(@"File size not available");
        }

        // Print asset creation date
        if (asset.creationDate) {
            NSLog(@"Creation date: %@", asset.creationDate);

            // Convert creationDate to a timestamp
            NSTimeInterval creationTimestamp = [asset.creationDate timeIntervalSince1970];
            time_t creationTime = static_cast<time_t>(creationTimestamp); // Cast to time_t for comparison
            NSLog(@"Asset creation timestamp: %ld", creationTime);

            // Construct file info packet
            FileInfoPkt fileInfoPkt{};
            std::string baseName = getBaseName(fileName);
            memset(fileInfoPkt.name, 0, sizeof(fileInfoPkt.name));
            memcpy(fileInfoPkt.name, baseName.c_str(), baseName.length());
            fileInfoPkt.size = [fileSize unsignedLongValue];;
            fileInfoPkt.time = creationTime;

            // Send file info packet to client
            LOGD("Sending file name=%s size=%ld time=%ld", fileInfoPkt.name,
                 fileInfoPkt.size, fileInfoPkt.time);
            if ((bytesSent = send(clientSocket, &fileInfoPkt, sizeof(fileInfoPkt), 0))
                != sizeof(fileInfoPkt)) {
                if (bytesSent < 0)
                    LOGE("Send file info failed: %s", strerror(errno));
                else
                    LOGE("Send file info failed bytesSent=%zu", bytesSent);
                return -1;
            }
        }

        
        // Retrieve additional resource metadata
        NSString *uti = resource.uniformTypeIdentifier;
        NSLog(@"Uniform Type Identifier: %@", uti);

        // Read the file by chunks
        PHAssetResourceManager *manager = [PHAssetResourceManager defaultManager];
        NSMutableData *fileData = [NSMutableData data];

        // Declare readError as __block to modify it inside the block
        __block NSError *readError = nil;

        dispatch_semaphore_t semaphore = dispatch_semaphore_create(0);

        fileCount += 1;
        LOGI("Sending %d/%d %s...", fileCount, totalFiles, fileName);

        [manager requestDataForAssetResource:resource
                                     options:nil
                           dataReceivedHandler:^(NSData * _Nonnull data) {
                               [fileData appendData:data];

                               // Process data in chunks of 1024 bytes
                               const size_t chunkSize = CHUNK_SIZE;
            
                                char buffer[CHUNK_SIZE] = {0};
                                size_t bytesRead = 0;
                                size_t totalBytesSent = 0;

                               for (size_t i = 0; i < data.length; i += chunkSize) {
                                   NSData *chunk = [data subdataWithRange:NSMakeRange(i, MIN(chunkSize, data.length - i))];
                                   // NSLog(@"Read chunk: %lu bytes", chunk.length);
                                   // Here, you can process the chunk as needed
                                   
                                   // Convert NSData to a C-style buffer (char array)
                                   [chunk getBytes:buffer length:chunk.length];
                                   
                                   bytesRead = chunk.length;
                                   totalBytesSent = 0;
                                   while (totalBytesSent < bytesRead) {
                                       if ((bytesSent = send(clientSocket, buffer + totalBytesSent,
                                           bytesRead - totalBytesSent, 0)) < 0) {
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
                           }
                           completionHandler:^(NSError * _Nullable error) {
                               readError = error;
                               dispatch_semaphore_signal(semaphore);
                           }];
        dispatch_semaphore_wait(semaphore, DISPATCH_TIME_FOREVER);

        if (readError) {
            NSLog(@"Error reading file: %@", readError.localizedDescription);
        } else {
            NSLog(@"Finished reading file: %s, Total size: %lu bytes", fileName, fileData.length);
        }
    }

    LOGI("Send file complete %d/%d %s", fileCount, totalFiles, fileName);
    return 0;
}

int FileTransferServer::receiveFile(int clientSocket, const char *directory) {
    std::string fileNameStr;
    ssize_t bytesSent = 0;
    ssize_t bytesRecv = 0;

    // Send start signal to client
    LOGD("Sending start signal");
    StartSignalPkt startSignalPkt{};
    startSignalPkt.start = true;
    if ((bytesSent = send(clientSocket, &startSignalPkt,
        sizeof(startSignalPkt), 0)) != sizeof(startSignalPkt)) {
        if (bytesSent < 0)
            LOGE("Send start signal failed: %s", strerror(errno));
        else
            LOGE("Send start signal failed bytesSent=%zu", bytesSent);
        return -1;
    }

    // Receive file info packet from client
    FileInfoPkt fileInfoPkt{};
    LOGD("Receiving file info");
    if ((bytesRecv = recv(clientSocket, &fileInfoPkt, sizeof(fileInfoPkt), 0))
        != sizeof(fileInfoPkt)) {
        if (bytesRecv)
            LOGE("Receive file info failed: %s", strerror(errno));
        else
            LOGE("Receive file info failed bytesRecv=%zu", bytesRecv);
        return -1;
    }
    fileNameStr = fileInfoPkt.name;

    // Prepend directory if present
    if (strlen(directory)) {
        fileNameStr.clear();
        fileNameStr = directory;
        fileNameStr.append("/");
        fileNameStr.append(fileInfoPkt.name);
    }

    LOGD("File name=%s size=%ld time=%ld", fileNameStr.c_str(),
        fileInfoPkt.size, fileInfoPkt.time);

    // Open file for writing
    FILE *file = fopen(fileNameStr.c_str(), "wb");
    if (!file) {
        LOGE("Error opening file");
        return -1;
    }

    fileCount += 1;
    LOGI("Receiving file %d/%d name=%s size=%zu...", fileCount,
        totalFiles, fileNameStr.c_str(), fileInfoPkt.size);

    // Receive the content of the file
    LOGD("Receiving file content");
    char buffer[CHUNK_SIZE] = {0};
    bytesRecv = 0;
    size_t totalBytesRecv = 0;
    while (true) {
        if ((bytesRecv = recv(clientSocket, buffer, CHUNK_SIZE, 0)) < 0) {
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
    if (utime(fileNameStr.c_str(), &new_times) == -1) {
        LOGE("Error copying file timestamp: %s", strerror(errno));
        fileCount -= 1;
        return -1;
    }

    LOGI("Receive file completed %d/%d %s", fileCount, totalFiles,
          fileNameStr.c_str());

    return 0;
}

int FileTransferServer::sendFileList(int clientSocket, std::vector<std::string>
    files) {
    ssize_t bytesRecv = 0;

    // Wait for client start signal
    LOGD("Waiting for start signal");
    StartSignalPkt startSignalPkt{};
    if ((bytesRecv = recv(clientSocket, &startSignalPkt,
        sizeof(startSignalPkt), 0)) != sizeof(startSignalPkt)) {
        if (bytesRecv < 0)
            LOGE("Receive start signal failed: %s", strerror(errno));
        else
            LOGE("Receive start signal failed bytesRecv=%zu", bytesRecv);
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
        fileCount += 1;
    }

    LOGI("File list sent completed");
    return 0;
}

void FileTransferServer::stopServer() {
    LOGI("Stopping Server");
    running = false;
    // Trigger the server to exit from the accept call.
    shutdown(serverSocket, SHUT_RDWR);
    close(serverSocket);
}

} // namespace Dex