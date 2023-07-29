#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <sys/types.h>
#include "BinderServiceP.h"
#include "list.h"
#include "common.h"
#include "service.h"

#define SU_CMD 0
#define GET_CHIP_ID 1

#define FILE_INPUT_MAX_SIZE (64*1024)

//#define DEBUG_PSERVICE
//#define NO_RSINPUT

unsigned char chip_id[32];

static void handle_request(const request_t *req, response_t *res);
static void run_cmd(const char *cmd, response_t *res);

#ifdef DEBUG_PSERVICE
static void do_test(uint32_t code, int argc, const char *argv[])
{
	request_t req;
	response_t res;
	res.data = calloc(1, RESPONSE_DATA_MAX_LEN * sizeof(char));
	
	req.code = code;

	int i;
	for (i = 0; i < argc; i++){
		strcpy(req.argv[i], argv[i]);
		LOGD("msg:%d %s",i, argv[i]);
	}
		
	req.argc = argc;

	handle_request((const request_t *)&req, &res);
	LOGD("msg: %s", res.data);
	free(res.data);
}
#endif

int main(int argc, const char *argv[])
{
	signal(SIGINT, _exit);
    signal(SIGTERM, _exit);

	run_cmd("setenforce 0", NULL);

#ifndef DEBUG_PSERVICE

#ifndef NO_RSINPUT
	extern void device_manager_start();
	device_manager_start();
#endif
	LOGD("start to binderservice");
	BinderMainBlock((BinderCallback)handle_request);
#else
	if (argc > 2)
		do_test(atoi(argv[1]), argc - 2, &argv[2]);
	else if (argc == 2)
		do_test(atoi(argv[1]), argc - 2, &argv[2]);
	else
		
#endif
	
	return 0;
}

static bool vertify_request(const request_t *req)
{
	return true;
}

static bool get_chip_id(char* str)
{
	bool ret = false;
	char* chip_path = "/proc/cpuinfo";
	FILE* chipf= fopen(chip_path,"r");
	
	if(!chipf){
		LOGI("can't find %s\n",chip_path);
		return false;
	}
	
	while(fgets(str, FILE_INPUT_MAX_SIZE, chipf)){
		if(strstr(str, "Serial")){
			strcpy(str, str+10);
			ret = true;
			break;
		}
		memset(str, 0, strlen(str));
	}
	
	fclose(chipf);
	return ret;
}

static void handle_request(const request_t *req, response_t *res)
{
	const char *msg;
	
	if (!vertify_request(req)) {
		msg = "vertify error";
		goto error;
	}

	switch(req->code){
		case SU_CMD:
			run_cmd(req->argv[0], res);
			break;
		case GET_CHIP_ID:
			if(!get_chip_id(res->data))
				goto error;
			//LOGD("GET_CHIP_ID%d,%d:%s",sizeof(void*),sizeof(char*),res->data);
			break;
		default:
			msg = "no cmd";
			goto error;
			break;
	}
	
	goto done;
	
error:
	strcpy(res->data, msg);
	res->len = strlen(res->data);
	res->status = -1;
	return;
done:
	res->len = strlen(res->data);
	res->status = 0;
	return;
	
}

