
#ifndef _SECURIY_H_
#define _SECURIY_H_

#include <android/log.h>
#include <unistd.h>

#define LOG_TAG "pservice"
#define LOGI(...)  __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...)  __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#if 1
#define LOGD(...)  __android_log_print(ANDROID_LOG_DEBUG, LOG_TAG, __VA_ARGS__)
#else
#define LOGD
#endif

#define REQUEST_LICENCE_LEN 128
#define REQUEST_SYMBOL_MAX_LEN 128
#define REQUEST_ARGV_MAX_LEN 128
#define REQUEST_ARGC_MAX_LEN 8

#define RESPONSE_DATA_MAX_LEN 1024

typedef struct
{
	int status;
	int len;
	void *data;
} response_t;

typedef void (*req_callback_t)(response_t *);

typedef struct
{
	uint32_t code;
	char licence[REQUEST_LICENCE_LEN];

	char symbol[REQUEST_SYMBOL_MAX_LEN];
	char argv[REQUEST_ARGC_MAX_LEN][REQUEST_ARGV_MAX_LEN];
	uint32_t argc;
} request_t;

#endif
