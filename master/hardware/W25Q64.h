#ifndef __W25Q64_H
#define __W25Q64_H

#include "W25Q64_Reg.h"
#include "MYSPI.h"

void W25Q64_Init(void);
void W25Q64_ReadID(uint8_t *MID, uint16_t *DID);
void W25Q64_PageProgram(uint32_t Address, uint8_t *DataArray, uint16_t Count);
void W25Q64_SectorErase(uint32_t Address);
void W25Q64_ReadData(uint32_t Address, uint8_t *DataArray, uint32_t Count);
uint8_t W25Q64_SaveValue(uint32_t Address, int16_t Value);
uint8_t W25Q64_LoadValue(uint32_t Address, int16_t *Value);

#endif
