
#include "BinderServiceP.h"

#include <binder/IServiceManager.h>
#include <binder/IPCThreadState.h>
#include <cutils/log.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <grp.h>
#include <binder/ProcessState.h>
//#include <private/android_filesystem_config.h>
#include <utils/RefBase.h>
#include <binder/IInterface.h>
#include <binder/Parcel.h>
#include <utils/threads.h>
#include <stdio.h>
#include <stdbool.h>

#define RETURN_INT 1
#define RETURN_BYTEARRAY 2

#define SERVICE_NAME "PServerBinder"

static BinderCallback gCB = NULL;

namespace android
{

class BinderServiceP: public BBinder
{
public:
	static int Instance();
	BinderServiceP();
	virtual ~BinderServiceP();
	virtual status_t onTransact(uint32_t, const Parcel&, Parcel*, uint32_t flag);
};

void *BinderExe(void *arg)
{
	sp<ProcessState> proc(ProcessState::self());
	sp<IServiceManager> sm = defaultServiceManager();
	BinderServiceP::Instance();
	//ProcessState::self()->setThreadPoolMaxThreadCount(4);//android default max thread count 16
	ProcessState::self()->startThreadPool();
	IPCThreadState::self()->joinThreadPool();
	return NULL;
}

BinderServiceP::BinderServiceP()
{

}

BinderServiceP::~BinderServiceP()
{

}

int BinderServiceP::Instance()
{
	int r= 10;
	r = defaultServiceManager()->addService(String16(SERVICE_NAME), new BinderServiceP(),true);
	LOGD("ADDSERVICE RESULT IS %d",r);
    return r;
}

static int readStringArray(const Parcel& data, char* result[]){
	int n = data.readInt32();
	
	if (n >= 0) {
		if (n <= REQUEST_ARGC_MAX_LEN) {
			for (int ix = 0; ix != n; ++ix) {
				String16 s16 = data.readString16();
				String8 s8 = String8(s16);
				const char* s = s8.string();
				size_t size = sizeof(char) * strlen(s) + 1;
				result[ix] = new char[size];
				memset(result[ix], 0, size);
				strcpy(result[ix], s);
			}
			return n;
		} else {
			return -1;
		}
	}
	return 0;
}

static bool parseData(const Parcel& data, request_t *req)
{
	char *argv[REQUEST_ARGC_MAX_LEN + 1] = { 0 };
	
	int argc = readStringArray(data, argv);

	if (argc <= 0)
		return false;
	for (int i = 0; i < argc; i++)
		if (!argv[i])
			return false;

	for (int i = 0; i < argc; i++)
		strcpy(req->argv[i], argv[i]);
		
	req->argc = argc;

	for (int i = 0; i < argc; i++) {
		if (argv[i]) {
			delete argv[i];
			argv[i] = NULL;
		}
	}
	
	return true;
}

status_t BinderServiceP::onTransact(uint32_t code, const Parcel& data, Parcel* reply, uint32_t flag) 
{
	response_t res;
	request_t req;

	memset(&res, 0, sizeof(res));
	memset(&req, 0, sizeof(req));

	res.data = calloc(1, RESPONSE_DATA_MAX_LEN * sizeof(char));
	if (!res.data)
		return NO_MEMORY;

	req.code = code;
	if (!parseData(data, &req))
		return UNKNOWN_ERROR;

	gCB(&req, &res);
	
	reply->writeByteArray(res.len, (const uint8_t *)res.data);
	
	free(res.data);
	res.data = NULL;
	res.len = 0;

	return OK;
}

}//namespace

void BinderMainThread(BinderCallback cb)
{
	pthread_t pid = -1;
	gCB = cb;
	pthread_create(&pid, NULL, android::BinderExe, NULL); 
}

void BinderMainBlock(BinderCallback cb) 
{
	gCB = cb;

	android::BinderExe(NULL);
}


