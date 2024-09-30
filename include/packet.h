#ifndef PACKET_H
#define PACKET_H
#include <ctime>

enum class Command {
	PULL,
	PUSH,
	LIST,
	INVALID
};

typedef struct InitPkt {
	Command command = Command::INVALID;
	char pattern[512]; // file pattern
	unsigned totalFiles; // Number of local files found used for PUSH command.
} InitPacket ;

typedef struct InitReplyPkt {
	bool proceed;
	unsigned totalFiles;
} initReplyPkt;

typedef struct StartSignalPkt {
	bool start;
} startSignalPkt;

typedef struct FileInfoPkt {
	char name[128];
	size_t size;
	time_t time;
} fileInfoPkt;

#endif // PACKET_H
