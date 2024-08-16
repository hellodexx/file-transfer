#include <jni.h>
#include <string>
#include "include/FileTransferServer.h"

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_dexfiletransfer_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */) {
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_dexfiletransfer_MainActivity_runDexFileTransferServerJNI(
        JNIEnv* env,
        jobject /* this */) {
    Dex::FileTransferServer ftServer;
    ftServer.runServer();

    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}
