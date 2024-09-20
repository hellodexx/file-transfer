#ifndef PACKET_H
#define PACKET_H
#include <ctime>

enum class Command {
	PULL,
	LIST,
	INVALID
};

typedef struct InitPkt {
	Command command = Command::INVALID;
	char pattern[512];
} InitPacket ;

typedef struct InitReplyPkt {
	unsigned noOfFiles;
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
