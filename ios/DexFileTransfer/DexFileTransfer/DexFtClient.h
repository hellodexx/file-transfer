//
//  DexFtClient.h
//  DexFileTransfer
//
//  Created by Dexter on 12/3/24.
//

#ifndef DexFtClient_h
#define DexFtClient_h

#include "packet.h"
#include <map>
#include <string>

namespace Dex {

class FileTransferClient {
public:
    FileTransferClient();
    ~FileTransferClient();
    int runClient(const char* serverIp, Command cmd, const char* pattern);

private:
    int connectToServer(const char* serverIp);
    int handleCommand(Command cmd, const char* pattern);
    int receiveFile();
    int sendFile(const char* fileName);
    int sendFileIOS(const char* fileName, PHAsset *asset);
    int receiveFileList();
    std::map<std::string, PHAsset *> fetchMediaMatchingPatternMap(const std::string& pattern);
    
    int serverSocket;
    unsigned totalFiles = 0;
    unsigned fileCount = 0;
    std::map<std::string, PHAsset *> matchAssetsMap;
};

} // namespace Dex

#endif /* DexFtClient_h */
