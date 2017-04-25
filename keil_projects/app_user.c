#include <string.h>

#include "app_sps_scheduler.h"

#include "app_user.h"

#define DA14580_BTADDRESS_LEN (6)
#define MAX_DA14580_MSG_LEN (128)

static int times = 0;
static int isHaveKey = 0;
uint8_t key_bt_addr[KEY_BT_ADDR_MAX_COUNTER][KEY_BT_ADDR_LENGTH + 1] = {0}; //{bt_flag,bt_addr[6]}  //bt_flag 1 key 0 nokey

typedef struct
{
    short signature;
    char cmd;
    short length;
    unsigned char data[];
}__attribute__((__packed__)) DA14580_MSG;

typedef int (*DA14580_ACTION)(const DA14580_MSG *);

enum
{
    CMD_GET_BTADDRESS    = 0,
    CMD_SEND_BTADDRESS   = 1,
    CMD_GETKEY           = 2,
    CMD_NGETKEY          = 3
};

static int da14580_getMsgHeader(DA14580_MSG *msg, unsigned char cmd, short len)
{
    msg->signature = 0xA5A5;
    msg->cmd = cmd;
    msg->length = len;
    return 0;
}

static int da14580_sendUart(unsigned char* cmdstring, unsigned short len)
{
    app_ble_push(cmdstring, len);
    return 0;
}

int da14580_isHaveKey(void)
{
    return isHaveKey;
}

int da14580_HaveKey(int is_have_key)
{
    return isHaveKey = is_have_key;
}

int da14580_resetTime(void)
{
    return times = 0;
}

int da14580_addTime(void)
{
    return ++times;
}

int da14580_getTime(void)
{
    return times;
}

int da14580_getBTaddressReq(void)
{
    short len = 0;
    unsigned char msg[MAX_DA14580_MSG_LEN] = {0};

    da14580_getMsgHeader((DA14580_MSG *)msg, CMD_GET_BTADDRESS, len);

    da14580_sendUart(msg, sizeof(DA14580_MSG) + len);
    return 0;
}

int da14580_sendGetKeyReq(void)
{
    short len = DA14580_BTADDRESS_LEN;
    unsigned char msg[MAX_DA14580_MSG_LEN] = {0};

    da14580_HaveKey(1);

    da14580_getMsgHeader((DA14580_MSG *)msg, CMD_GETKEY, len);
    da14580_sendUart(msg, sizeof(DA14580_MSG) + len);
    return 0;
}


int da14580_sendNGetKeyReq(void)
{
    short len = DA14580_BTADDRESS_LEN;
    unsigned char msg[MAX_DA14580_MSG_LEN] = {0};

    da14580_getMsgHeader((DA14580_MSG *)msg, CMD_NGETKEY, len);
    da14580_sendUart(msg, sizeof(DA14580_MSG) + len);
    return 0;
}

static int da14580_getBTaddressRsp(const DA14580_MSG* req)
{
    int i = 0;
    short length = req->length;

    for(i = 0;i < length / KEY_BT_ADDR_LENGTH; i++)
    {
        key_bt_addr[i][0] = 1;
        memcpy(key_bt_addr[i] + 1, (unsigned char *)(req->data + i * KEY_BT_ADDR_LENGTH), KEY_BT_ADDR_LENGTH);
    }

    for(;i < KEY_BT_ADDR_MAX_COUNTER; i++)
    {
        memset(key_bt_addr[i], 0, KEY_BT_ADDR_LENGTH + 1);
    }

    return 0;
}

static int da14580_sendBTaddressRsp(const DA14580_MSG* req)
{
    int i = 0;
    short len = 0;
    short length = req->length;
    unsigned char msg[MAX_DA14580_MSG_LEN] = {0};

    for(i = 0;i < length / KEY_BT_ADDR_LENGTH; i++)
    {
        key_bt_addr[i][0] = 1;
        memcpy(key_bt_addr[i] + 1, (unsigned char *)(req->data + i * KEY_BT_ADDR_LENGTH), KEY_BT_ADDR_LENGTH);
    }

    for(;i < KEY_BT_ADDR_MAX_COUNTER; i++)
    {
        memset(key_bt_addr[i], 0, KEY_BT_ADDR_LENGTH + 1);
    }

    da14580_getMsgHeader((DA14580_MSG *)msg, CMD_SEND_BTADDRESS, len);

    da14580_sendUart(msg, sizeof(DA14580_MSG) + len);
    return 0;
}

static int da14580_sendGetKeyRsp(const DA14580_MSG* req)
{
    //TODO: nothing
    return 0;
}

static int da14580_sendNGetKeyRsp(const DA14580_MSG* req)
{
    //TODO: nothing
    return 0;
}

typedef struct
{
    unsigned char cmd;
    DA14580_ACTION  action;
}DA14580_MAP;

DA14580_MAP da14580_map[]=
{
    {CMD_GET_BTADDRESS,      da14580_getBTaddressRsp},
    {CMD_SEND_BTADDRESS,     da14580_sendBTaddressRsp},
    {CMD_GETKEY,             da14580_sendGetKeyRsp},
    {CMD_NGETKEY,            da14580_sendNGetKeyRsp}
};

int DA14580_uartHandler(unsigned char *buf, uint32_t len)
{
    int i = 0;
    int leftLen = len;
    DA14580_MSG *msg = (DA14580_MSG *)buf;
    while(1)
    {
        if(msg->signature != (short)(0xA5A5))
        {
            return -1;
        }
        
        for(i = 0; i < sizeof(da14580_map)/sizeof(da14580_map[0]); i++)
        {
            if(msg->cmd== da14580_map[i].cmd)
            {
                da14580_map[i].action(msg);
                break;
            }
        }
        
        leftLen = leftLen - sizeof(DA14580_MSG) - msg->length;
        msg = (DA14580_MSG *)((const char *)buf + len - leftLen);

    }
}
