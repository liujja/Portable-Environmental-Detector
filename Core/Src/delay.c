#include "delay.h"                    // Device header

void delayus(unsigned long int count)  //大约1us延时函数
{
	unsigned int i;
	for(i=0;i<count;i++)
	{
		__nop();__nop();__nop();__nop();
		__nop();__nop();__nop();__nop();
		__nop();__nop();__nop();__nop();
		__nop();__nop();__nop();__nop();
		__nop();__nop();__nop();__nop();
	}
}

void delayms(int count)  // 大约1ms延时函数
{
		volatile unsigned long int i,j,Delaynum=600;
		for(i=0;i<count;i++)
		 for(j=0;j<Delaynum;j++); 
}
