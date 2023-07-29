#ifndef _P_COMMON_H_
#define _P_COMMON_H_

static inline void string_trim(char *buffer, int len)
{
        int i;
        for(i = 0; i < len; i++){
                if(buffer[i] == 32){
                        buffer[i] = '\0';
                        break;
                }
        }
}

static void run_cmd(const char *cmd, response_t *res)
{
#define CMD_OUTPUT_MAX_SIZE (64*1024)
        if (!cmd)
                return;

        FILE *fp = popen(cmd, "r");

        if (res) {
                if (!res->data)
                        return;
                fgets(res->data, CMD_OUTPUT_MAX_SIZE, fp);
                res->len = strlen(res->data);
        }

        pclose(fp);
#undef CMD_OUTPUT_MAX_SIZE
}

#endif