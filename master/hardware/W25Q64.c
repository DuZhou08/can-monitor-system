#include "stm32f4xx.h"
#include "W25Q64.h"

void W25Q64_Init(void)
{
    MYSPI_Init();
}

void W25Q64_ReadID(uint8_t *MID, uint16_t *DID)
{
	MYSPI_Start();
	MYSPI_SwapByte(W25Q64_JEDEC_ID);			//交换发送读取ID的指令
	*MID = MYSPI_SwapByte(W25Q64_DUMMY_BYTE);	//交换接收MID，通过输出参数返回
	*DID = MYSPI_SwapByte(W25Q64_DUMMY_BYTE);	//交换接收DID高8位
	*DID <<= 8;									//高8位移到高位
	*DID |= MYSPI_SwapByte(W25Q64_DUMMY_BYTE);	//或上交换接收DID的低8位，通过输出参数返回
	MYSPI_Stop();
}

void W25Q64_WriteEnable(void)
{
	MYSPI_Start();
	MYSPI_SwapByte(W25Q64_WRITE_ENABLE);		//交换发送写使能的指令
	MYSPI_Stop();
}

void W25Q64_WaitBusy(void)
{
	uint32_t Timeout;
	MYSPI_Start();
	MYSPI_SwapByte(W25Q64_READ_STATUS_REGISTER_1);				//交换发送读状态寄存器1的指令
	Timeout = 1000000;							//给定超时计数时间 (硬件SPI快, 须覆盖扇区擦除最坏 ~400ms)
	while ((MYSPI_SwapByte(W25Q64_DUMMY_BYTE) & 0x01) == 0x01)	//循环等待忙标志位
	{
		Timeout --;
		if (Timeout == 0)
		{

			break;
		}
	}
	MYSPI_Stop();
}

void W25Q64_PageProgram(uint32_t Address, uint8_t *DataArray, uint16_t Count)
{
	uint16_t i;
	
	W25Q64_WriteEnable();						//写使能
	
	MYSPI_Start();
	MYSPI_SwapByte(W25Q64_PAGE_PROGRAM);		//交换发送页编程的指令
	MYSPI_SwapByte(Address >> 16);				//交换发送地址23~16位
	MYSPI_SwapByte(Address >> 8);				//交换发送地址15~8位
	MYSPI_SwapByte(Address);					//交换发送地址7~0位
	for (i = 0; i < Count; i ++)
	{
		MYSPI_SwapByte(DataArray[i]);			//依次在起始地址后写入数据
	}
	MYSPI_Stop();
	
	W25Q64_WaitBusy();							//等待忙
}

void W25Q64_SectorErase(uint32_t Address)
{
	W25Q64_WriteEnable();						//写使能
	
	MYSPI_Start();
	MYSPI_SwapByte(W25Q64_SECTOR_ERASE_4KB);	//交换发送扇区擦除的指令
	MYSPI_SwapByte(Address >> 16);				//交换发送地址23~16位
	MYSPI_SwapByte(Address >> 8);				//交换发送地址15~8位
	MYSPI_SwapByte(Address);					//交换发送地址7~0位
	MYSPI_Stop();
	
	W25Q64_WaitBusy();							//等待忙
}

void W25Q64_ReadData(uint32_t Address, uint8_t *DataArray, uint32_t Count)
{
	uint32_t i;
	MYSPI_Start();
	MYSPI_SwapByte(W25Q64_READ_DATA);			//交换发送读取数据的指令
	MYSPI_SwapByte(Address >> 16);				//交换发送地址23~16位
	MYSPI_SwapByte(Address >> 8);				//交换发送地址15~8位
	MYSPI_SwapByte(Address);					//交换发送地址7~0位
	for (i = 0; i < Count; i ++)
	{
		DataArray[i] = MYSPI_SwapByte(W25Q64_DUMMY_BYTE);	//依次在起始地址后读取数据
	}
	MYSPI_Stop();
}

/*以下为数据存储功能：存储格式 = [magic 2字节][value 2字节][crc16 2字节]，共6字节*/

#define W25Q64_SAVE_MAGIC_0		0x5A
#define W25Q64_SAVE_MAGIC_1		0xA5

static uint16_t W25Q64_Crc16(const uint8_t *DataArray, uint8_t Count)
{
	uint16_t Crc = 0;
	uint8_t i, j;
	for (i = 0; i < Count; i ++)			//遍历每个字节
	{
		Crc ^= (uint16_t)DataArray[i] << 8;	//低字节移到高字节参与运算
		for (j = 0; j < 8; j ++)			//逐位计算
		{
			if (Crc & 0x8000)				//最高位为1时
			{
				Crc = (Crc << 1) ^ 0x1021;	//左移并与生成多项式异或
			}
			else
			{
				Crc <<= 1;					//最高位为0时仅左移
			}
		}
	}
	return Crc;
}

uint8_t W25Q64_SaveValue(uint32_t Address, int16_t Value)
{
	uint8_t DataArray[6];
	uint16_t Crc;

	DataArray[0] = W25Q64_SAVE_MAGIC_0;					//写入magic
	DataArray[1] = W25Q64_SAVE_MAGIC_1;
	DataArray[2] = (uint8_t)(Value & 0xFF);				//写入数值低字节
	DataArray[3] = (uint8_t)((Value >> 8) & 0xFF);		//写入数值高字节
	Crc = W25Q64_Crc16(DataArray, 4);					//计算前4字节的校验值
	DataArray[4] = (uint8_t)(Crc & 0xFF);				//写入校验低字节
	DataArray[5] = (uint8_t)((Crc >> 8) & 0xFF);		//写入校验高字节

	W25Q64_SectorErase(Address);						//扇区必须先擦除才能写入
	W25Q64_PageProgram(Address, DataArray, 6);			//写入数据

	return 1;
}

uint8_t W25Q64_LoadValue(uint32_t Address, int16_t *Value)
{
	uint8_t DataArray[6];
	uint16_t Crc;

	W25Q64_ReadData(Address, DataArray, 6);				//读取6字节数据

	if (DataArray[0] != W25Q64_SAVE_MAGIC_0 || DataArray[1] != W25Q64_SAVE_MAGIC_1)
	{
		return 0;										//magic不匹配：未保存过或数据损坏
	}
	Crc = (uint16_t)((DataArray[5] << 8) | DataArray[4]);	//还原校验值
	if (Crc != W25Q64_Crc16(DataArray, 4))
	{
		return 0;										//校验失败：数据损坏
	}
	*Value = (int16_t)(DataArray[2] | (DataArray[3] << 8));	//还原数值
	return 1;
}

