#ifndef _LOG_H_
#define _LOG_H_

#include <stdio.h>
#include <cutils/log.h>
#include <android/log.h>

#ifndef ALOGD
	#define ALOGV LOGV
	#define ALOGD LOGD
	#define ALOGI LOGI
	#define ALOGW LOGW
	#define ALOGE LOGE
#endif

#define err_log(num) \
	do	\
	{	\
		ALOGD("Error: %s, %s(Line: %d): %s.\n", __FILE__, __func__, __LINE__, strerror(num));	\
	}while(0)

#define warn_log(msg)	\
	do	\
	{	\
		ALOGD(stderr, "Warn: %s, %s(Line: %d): %s.\n", __FILE__, __func__, __LINE__, msg);	\
	}while(0)

#define msg_log(msg)	\
	do	\
	{	\
		ALOGD(stderr, "Msg: %s, %s(Line: %d): %s.\n", __FILE__, __func__, __LINE__, msg);	\
	}while(0)


#endif	//_LOG_H_
