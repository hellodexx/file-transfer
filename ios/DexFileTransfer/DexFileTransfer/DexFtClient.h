//
//  DexFtClient.h
//  DexFileTransfer
//
//  Created by Dexter on 11/27/24.
//

#ifndef DexFtClient_h
#define DexFtClient_h

#include "packet.h"
#include <vector>

namespace Dex {

class FileTransferClient {
public:
    FileTransferClient();
    ~FileTransferClient();
    void runClient(const char* serverIp, Command cmd, const char* pattern);
    void runClientIOS(const char* serverIp, Command cmd, const char* pattern, std::vector<std::string> fileNames);
    void foo(); // For testing purposes only
    bool findFileInCameraRoll(const std::string &fileName, PHAsset **foundAsset);

private:
    int connectToServer(const char* serverIp);
    int handleCommand(Command cmd, const char* pattern);
    int handleCommandIOS(Command cmd, const char* pattern, const std::vector<std::string> &fileNames);
    int receiveFile();
    int sendFile(const char* fileName);
    int sendFileIOS(const char* fileName);
    int receiveFileList();

    int serverSocket;
    unsigned totalFiles = 0;
    unsigned fileCount = 0;
};

} // namespace Dex

#endif /* DexFtClient_h */
