#include <iostream>
#include <cstring>
#include "FileTransferServer.h"
#include "FileTransferClient.h"
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
	std::string serverIp;
	std::string pattern;
	std::string listPattern;
	Dex::FileTransferServer ftServer;
	Dex::FileTransferClient ftClient;

	struct option long_options[] = {
		{"help", no_argument, 0, 'h'},
		{"version", no_argument, 0, 'v'},
		{"server", no_argument, 0, 's'},
		{"client", no_argument, 0, 'c'},
		{"ip", required_argument, 0, 'i'},
		{"pull", required_argument, 0, 'p'},
		{"list", required_argument, 0, 'l'},
		{0, 0, 0, 0} // This marks the end of the array
	};

	while ((opt = getopt_long(argc, argv, "hvsci:p:l:", long_options,
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
				std::cout << "Server mode\n";
				mode = Mode::SERVER;
				break;
			case 'c':
				std::cout << "Client mode\n";
				mode = Mode::CLIENT;
				break;
			case 'i':
				std::cout << "Server ip: " << optarg << "\n";
				serverIp = optarg;
				break;
			case 'p':
				std::cout << "Pull: " << optarg << "\n";
				pattern = optarg;
				break;
			case 'l':
				std::cout << "List: " << optarg << "\n";
				listPattern = optarg;
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
		ftServer.runServer();
	} else if (mode == Mode::CLIENT) {
		if (serverIp.empty()) {
			printf("-i is required!\n");
			printUsage();
		}

		if (pattern.empty() && listPattern.empty()) {
			printf("-p or -l is required");
			printUsage();
		}

		if (!pattern.empty() && !listPattern.empty()) {
			printf("-p or -l conflict");
			printUsage();
		}
	
		if (pattern.size()) {
			ftClient.runClient(serverIp.c_str(), pattern.c_str(),
			                   Dex::FileTransferClient::command::PULL);
		}

		if (listPattern.size()) {
			ftClient.runClient(serverIp.c_str(), listPattern.c_str(),
			                   Dex::FileTransferClient::command::LIST);
		}
	} else {
		printUsage();
	}

	return 0;
}
