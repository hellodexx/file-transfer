//
//  DexFtServer.h
//  Dex File Transfer
//
//  Created by Dexter on 11/22/24.
//

#ifndef DexFtServer_h
#define DexFtServer_h
#include <string>
#include <vector>
#include <map>
#import <Photos/Photos.h>

namespace Dex {

class FileTransferServer {
public:
    FileTransferServer();
    ~FileTransferServer();
    void runServer();
    void stopServer();

private:
    void handleClient(int clientSocket);
    int sendFileIOS(int clientSocket, const char* fileName, PHAsset *asset);
    int receiveFileIOS(int clientSocket, const char* directory);
    int sendFileListIOS(int clientSocket, std::map<std::string, PHAsset *> assetsMap);
    void saveFileToPhotosIOS(const char* filePath);
    std::map<std::string, PHAsset *> fetchMediaMatchingPatternMap(const std::string& pattern);
    
    int serverSocket;
    unsigned totalFiles = 0;
    unsigned fileCount = 0;
    std::map<std::string, PHAsset *> matchAssetsMap;
    std::atomic<bool> running;
};

} // namespace Dex2

#endif /* DexFtServer_h */
