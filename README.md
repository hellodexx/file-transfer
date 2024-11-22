# Dex File Transfer App
C++ file transfer application for Linux, macOS, Android, iOS, and WSL.

## Supported OS
* Linux Ubuntu
* macOS
* Android
* iOS - (soon)
* Windows WSL

## Prerequisites
* Unix like OS for building
* Android Studio for Android
* xcode for iOS

## Build
### Build for PC
```bash
git clone https://github.com/hellodexx/file-transfer.git
cd file-transfer
make clean
make
```
### Build for Android
1. Build static library for android. 
```bash
make android_libs
```
2. Open and build **android/DexFileTransfer** on Android Studio

## Usages
### Download PC file(s)
1. Run server
```bash
./ft --server or ./ft -s
```
2. Run client
```bash
./ft --client --ip <server_ip> --pull <file_pattern>

Examples:
# Download single file
./ft -c -i 127.0.0.1 -p "~/path/to/file/file.jpg"
# Download files by pattern
./ft -c -i 127.0.0.1 -p "~/path/to/file/*.mp4"
./ft -c -i 127.0.0.1 -p "~/path/to/file/somefile*"
```
### Download Android file(s)
1. Build **android/DexFileTransfer** App and run it on your device.
2. Run client on PC

```bash
./ft --client --ip <server_ip> --pull </storage/self/primary/DCIM/Camera/2024*.jpg>
```
### Download iOS file(s) SOON
