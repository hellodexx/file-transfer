#ifndef LOGGER_H
#define LOGGER_H

#ifdef DEBUG
#ifdef __ANDROID__
#include <android/log.h>
#define LOG_TAG "DexLog"
#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#include <cstdio>
#define LOGD(fmt, ...) printf("DEBUG: " fmt "\n", ##__VA_ARGS__)
#define LOGI(fmt, ...) printf("INFO: " fmt "\n", ##__VA_ARGS__)
#define LOGE(fmt, ...) fprintf(stderr, "ERROR: " fmt "\n", ##__VA_ARGS__)
#endif
#else // #ifdef DEBUG
#ifdef __ANDROID__
#include <android/log.h>
#define LOG_TAG "DexLog"
#define LOGD(...)
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#include <cstdio>
#define LOGD(fmt, ...)
#define LOGI(fmt, ...) printf("INFO: " fmt "\n", ##__VA_ARGS__)
#define LOGE(fmt, ...) fprintf(stderr, "ERROR: " fmt "\n", ##__VA_ARGS__)
#endif
#endif // #ifdef DEBUG

#endif // LOGGER_H