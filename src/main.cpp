#include "FileTransferServer.h"
#include "FileTransferClient.h"
#include "packet.h"
#include <iostream>
#include <cstring>
// Arg parse
#include <getopt.h>

static std::string binName;

void printUsage() {
	std::cout << "\nUsage:\n";
	std::cout << binName.c_str() << " --server\n";
	std::cout << binName.c_str() << " --client --ip <server_ip> --pull <filename>\n";
	std::cout << "\n";
	std::cout << "Server options:\n";
	std::cout << "  -s, --server\t Run server mode\n";
	std::cout << "Client options:\n";
	std::cout << "  -c, --client\t Run client mode\n";
	std::cout << "  -i, --ip\t IP address of the server\n";
	std::cout << "  -p, --pull\t File pattern to pull\n";
	std::cout << "  -u, --push\t File pattern to push\n";
	std::cout << "  -l, --list\t File pattern to list\n";
	exit(1);
}

enum class Mode {
	SERVER,
	CLIENT,
	INVALID
};

int main(int argc, char* argv[]) {
	Mode mode = Mode::INVALID;
	binName = argv[0];
	int opt;
	int option_index = 0;
	Command cmd = Command::INVALID;
	std::string serverIp;
	std::string pattern;
	Dex::FileTransferServer ftServer;
	Dex::FileTransferClient ftClient;

	struct option long_options[] = {
		{"help", no_argument, 0, 'h'},
		{"version", no_argument, 0, 'v'},
		{"server", no_argument, 0, 's'},
		{"client", no_argument, 0, 'c'},
		{"ip", required_argument, 0, 'i'},
		{"pull", required_argument, 0, 'p'},
		{"push", required_argument, 0, 'u'},
		{"list", required_argument, 0, 'l'},
		{0, 0, 0, 0} // This marks the end of the array
	};

	while ((opt = getopt_long(argc, argv, "hvsci:p:u:l:", long_options,
	       &option_index)) != -1) {
		switch (opt) {
			case 'h':
				std::cout << "Help option\n";
				printUsage();
				break;
			case 'v':
				std::cout << "Version option\n";
				break;
			case 's':
				mode = Mode::SERVER;
				break;
			case 'c':
				mode = Mode::CLIENT;
				break;
			case 'i':
				serverIp = optarg;
				break;
			case 'p':
				pattern = optarg;
				cmd = Command::PULL;
				break;
			case 'u':
				pattern = optarg;
				cmd = Command::PUSH;
				break;
			case 'l':
				pattern = optarg;
				cmd = Command::LIST;
				break;
			case '?':
				// getopt_long already prints an error message
				break;
			default:
				std::cerr << "Unknown option\n";
				printUsage();
				break;
		}
	}

	if (mode == Mode::INVALID) {
		std::cerr << "-s or -c is required!\n";
		printUsage();
	}

	if (mode == Mode::SERVER) {
		std::string localIp = ftServer.getLocalPrivateIP();
		std::cout << "Server listening on " << localIp.c_str() << std::endl;
		ftServer.runServer();
	} else if (mode == Mode::CLIENT) {
		if (serverIp.empty()) {
			printf("-i is required!\n");
			printUsage();
		}

		if (pattern.empty()) {
			printf("-p, -u, -l is required");
			printUsage();
		}

		switch (cmd) {
		case Command::PULL:
			ftClient.runClient(serverIp.c_str(), Command::PULL,
			                   pattern.c_str());
			break;
		case Command::PUSH:
			ftClient.runClient(serverIp.c_str(), Command::PUSH,
			                   pattern.c_str());
			break;
		case Command::LIST:
			ftClient.runClient(serverIp.c_str(), Command::LIST,
			                   pattern.c_str());
			break;
		default:
			printf("Invalid command=%d\n", static_cast<int>(cmd));
			break;
		}
	} else {
		printUsage();
	}

	return 0;
}
