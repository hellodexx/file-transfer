# Dex File Transfer App
A versatile file transfer application compatible with Android, iOS, Linux, macOS, and WSL. Dex File Transfer simplifies transferring files across devices in the same network, providing a lightweight and efficient solution.

## Supported Platforms
- **Linux**: Ubuntu, Debian
- **macOS**
- **Android**
- **iOS**
- **Windows**: WSL (Windows Subsystem for Linux)

## Prerequisites
To build and use Dex File Transfer, ensure the following requirements are met for your platform:
- **Linux**: GNU Make, GCC
- **Android**: Android Studio and NDK
- **iOS**: macOS with Xcode
- **Windows (WSL)**: A Linux environment with the prerequisites for Linux builds
- All devices must be on the same network for seamless file transfer.

## Building the Application

### Linux
1. Clone the repository:
```bash
git clone https://github.com/hellodexx/file-transfer.git
cd file-transfer
make clean
make
```

### Android
1. Build static libraries for Android:
```bash
make android_libs
```
2. Open the android/DexFileTransfer project in Android Studio.
3. Build and deploy the app using Android Studio.

### iOS
1. Open the ios/DexFileTransfer project in Xcode.
2. Build and deploy the app to your iOS device using Xcode.

## Usage

### Linux to Linux File Transfer
1. Start the Server
Run the server on the host device:
```bash
./ft -s
```
2. Run the Client
Run the client on the device you want to download files to:
```bash
./ft -c -i <server_ip> -p "<file_pattern>"
```
**Examples**  
Download single file
```bash
./ft -c -i 127.0.0.1 -p "~/path/to/file/file.jpg"
```
Download multiple files by pattern
```bash
./ft -c -i 127.0.0.1 -p "~/path/to/file/2024*"
./ft -c -i 127.0.0.1 -p "~/path/to/file/*.jpg"
./ft -c -i 127.0.0.1 -p "~/path/to/file/*.mp4"
```

### Android File Transfer
1. Build and run the **android/DexFileTransfer** app on your Android device.
2. Run the client from your PC:

```bash
./ft -c -i <server_ip> -p "<filename_or_pattern>"
# Example: ./ft -c 192.168.100.101 "2024*"
```
### iPhone File Transfer
1.	Build and run the **ios/DexFileTransfer** app on your iOS device.
2.	Run the client from your PC:
```bash
./ft -c -i <server_ip> -p "<filename_or_pattern>"
# Example: ./ft -c 192.168.100.101 "2024*"
```
