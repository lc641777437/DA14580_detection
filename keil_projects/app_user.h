
#ifndef __APP_USER__H__
#define __APP_USER__H__
#include "stdint.h"

#define KEY_BT_ADDR_LENGTH 6
#define KEY_BT_ADDR_MAX_COUNTER 5
#define TIMES_4_LOSE_KEY 1

extern uint8_t key_bt_addr[KEY_BT_ADDR_MAX_COUNTER][KEY_BT_ADDR_LENGTH + 1];
int da14580_resetTime(void);
int da14580_addTime(void);
int da14580_getTime(void);

int da14580_isHaveKey(void);
int da14580_HaveKey(int is_have_key);

int da14580_sendLockedReq(void);
int da14580_sendUnlockedReq(void);
int da14580_sendBTaddressReq(void);
int DA14580_uartHandler(unsigned char *buf);

#endif/*__APP_USER__H__*/


