# Dex File Transfer App
File transfer application for Linux, macOS, Android, iOS, and WSL.

## Supported OS
* Linux Ubuntu Debian
* macOS
* Android
* iOS
* Windows WSL

## Prerequisites
* Linux OS for android
* Android Studio for Android
* Mac for iOS
* XCode for iOS

## Build
### Build for Linux
```bash
git clone https://github.com/hellodexx/file-transfer.git
cd file-transfer
make clean
make
```
### Build Android
1. Build static library for android. 
```bash
make android_libs
```
2. Open and build **android/DexFileTransfer** on Android Studio

### Build iOS
1. Open and build **ios/DexFileTransfer** on XCode

## Usages
### Download files from Linux to Linux
1. Run server
```bash
./ft -s
```
2. Run client
```bash
./ft -c --ip <server_ip> --pull <file_pattern>

Examples:
# Download single file
./ft -c -i 127.0.0.1 -p "~/path/to/file/file.jpg"
# Download multiple files by pattern
./ft -c -i 127.0.0.1 -p "~/path/to/file/*.mp4"
./ft -c -i 127.0.0.1 -p "~/path/to/file/filename_prefix*"
```
### Download Android files from Linux
1. Build **android/DexFileTransfer** App and run it on your device.
2. Run client on PC

```bash
./ft -c -ip <server_ip> -p "filename_or_pattern"
# Ex. ./ft -c 192.168.100.101 "2024*"
```
### Download iPhone files from Linux
1. Build **ios/DexFileTransfer** App and run it on your device.
2. Run client on PC

```bash
./ft -c -ip <server_ip> -p "filename_or_pattern"
# Ex. ./ft -c 192.168.100.101 "2024*"
```
