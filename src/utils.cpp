#include "utils.h"
#include "Logger.h"
#include <cstring>
#include <dirent.h>
#include <sys/types.h>
#include <fnmatch.h>
#include <sys/stat.h>

bool isFilePattern(const char *filename) {
	return strchr(filename, '*') != nullptr;
}

std::string getBaseName(const std::string &path) {
	// Find the last occurrence of the directory separator
	size_t pos = path.find_last_of("/\\");
	
	// Extract the base name
	if (pos != std::string::npos) {
		return path.substr(pos + 1);
	} else {
		// If no separator is found, return the entire string (assuming it's
		// already a file name)
		return path;
	}
}

void splitPathAndPattern(const std::string &filestr,
    std::string &directory, std::string &pattern) {
	size_t last_slash = filestr.find_last_of('/');
	
	if (last_slash != std::string::npos) {
		directory = filestr.substr(0, last_slash);
		pattern = filestr.substr(last_slash + 1);
	} else {
		directory = ".";
		pattern = filestr;
	}
}

std::vector<std::string> getMatchingFiles(const std::string &filestr) {
	std::string directory, pattern;
	splitPathAndPattern(filestr, directory, pattern);

	std::vector<std::string> matching_files;
	DIR* dir;
	struct dirent* ent;

	if ((dir = opendir(directory.c_str())) != nullptr) {
		while ((ent = readdir(dir)) != nullptr) {
			std::string filename = ent->d_name;
			if (fnmatch(pattern.c_str(), filename.c_str(), 0) == 0) {
				matching_files.push_back(directory + "/" + filename);
			}
		}
		closedir(dir);
	} else {
		LOGE("Could not open directory");
	}

	return matching_files;
}

bool fileExists(const char *path) {
	FILE *file = fopen(path, "rb");
	if (file) {
		fclose(file);
		return true;
	} else {
		return false;
	}
}

int createDirectory(const char *dir) {
	if (mkdir(dir, 0777) == 0) {
		LOGD("Directory created successfully.");
	} else {
		if (errno != EEXIST) {
			LOGE("Failed to create directory. %s", strerror(errno));
			return -1;
		}
	}

	return 0;
}
