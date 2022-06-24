#include "hisi_temp_api.h"
#include <asm/io.h>




#define TEMP_REG_BASE     0x12030000
#define TEMP_REG_CONFIG         0xb4
#define TEMP_REG_STATUS         0xb8
#define TEMP_REG_RECORD0        0xbc
#define TEMP_REG_RECORD1        0xc0
#define TEMP_REG_RECORD2        0xc4
#define TEMP_REG_RECORD3        0xc8

#define TEMP_REG_SIZE           0xd0


#define WRITE_REG(addr, value)   ((*(volatile u32*)(addr)) = (value))
#define READ_REG(addr)           (*((volatile u32*)(addr)))


static void *temp_reg_base = NULL;




int hisi_get_temp(s32 *temp)
{
    u32 reg;

    reg = READ_REG(temp_reg_base + TEMP_REG_RECORD0) & 0x3ff;
    *temp = (s32)(((s32)reg - 136) * 165 / 793 - 40);

    return 0;
}


int hitemp_init(void)
{
    temp_reg_base = ioremap(TEMP_REG_BASE, TEMP_REG_SIZE);
    if (NULL == temp_reg_base)
        return -1;

    //写入默认配置
    WRITE_REG(temp_reg_base + TEMP_REG_CONFIG, 0xc3200000);
    WRITE_REG(temp_reg_base + TEMP_REG_STATUS, 0xc0);

    return 0;
}

void hitemp_deinit(void)
{
    if (temp_reg_base)
    {
        iounmap(temp_reg_base);
        temp_reg_base = NULL;
    }
}
