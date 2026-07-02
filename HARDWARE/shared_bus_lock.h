#ifndef __SHARED_BUS_LOCK_H
#define __SHARED_BUS_LOCK_H

#include <stdint.h>

uint8_t SharedBus_Lock(uint32_t timeout);
void SharedBus_Unlock(void);

void SharedBus_PrepareForSPL06(void);
void SharedBus_PrepareForNRF24(void);

#endif

