#ifndef UTILS_H
#define UTILS_H
#include <string>
#include <vector>

bool isFilePattern(const char* filename);
std::string getBaseName(const std::string& path);
void splitPathAndPattern(const std::string& filestr, std::string& directory,
                         std::string& pattern);
std::vector<std::string> getMatchingFiles(const std::string& filestr);
bool fileExists(const char *path);
int createDirectory(const char *dir);
#endif // UTILS_H