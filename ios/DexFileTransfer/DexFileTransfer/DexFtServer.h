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
    void foo();

private:
    void handleClient(int clientSocket);
    int sendFile(int clientSocket, const char* filename);
    int sendFileIOS(int clientSocket, const char* fileName, PHAsset *asset);
    int receiveFile(int clientSocket, const char* directory);
    int sendFileList(int clientSocket, std::vector<std::string> files);
    
    int serverSocket;
    unsigned totalFiles = 0;
    unsigned fileCount = 0;
    std::map<std::string, PHAsset *> matchAssetsMap;
};

} // namespace Dex2

#endif /* DexFtServer_h */
