/* SPDX-License-Identifier: BSD-2-Clause
 *
 * This file is part of libtslitex.
 * Control the ZL30260/ZL30250 in the Thunderscope LiteX design
 *
 * Copyright (c) 2024 John Simons <jammsimons@gmail.com>
 * Copyright (c) 2024 Nate Meyer <nate.devel@gmail.com>
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <stddef.h>
#include <time.h>

#include "ts_common.h"
#include "mcp_clkgen.h"
#include "i2c.h"
#include "liblitepcie.h"
#include "util.h"

#define ZL302XX_ADDR_LEN    (2)

int32_t mcp_clkgen_config(i2c_t device, const mcp_clkgen_conf_t* confData, uint32_t confLen)
{
    if(NULL == confData)
    {
        //Error
        return TS_STATUS_ERROR;
    }

    for(uint32_t i = 0; i < confLen; i++)
    {
        switch(confData[i].action)
        {
        case MCP_CLKGEN_DELAY:
        {
            NS_DELAY(confData[i].delay_us * 1000);
            break;
        }
        case MCP_CLKGEN_WRITE_REG:
        {
            if(!i2c_write(device, (uint32_t)(confData[i].addr),
                            &confData[i].value, 1, ZL302XX_ADDR_LEN))
            {
                return TS_STATUS_ERROR;
            }
            break;
        }
        default:
            //Error
            break;    
        }
    }
    return TS_STATUS_OK;
}

void mcp_clkgen_regdump(i2c_t device, const mcp_clkgen_conf_t* confData, uint32_t confLen)
{
    
    if(NULL == confData)
    {
        //Error
        return;
    }

    printf("Confirming Regs:\r\n");
    for(uint32_t i = 0; i < confLen; i++)
    {
        switch(confData[i].action)
        {
        case MCP_CLKGEN_DELAY:
        {
            //skip
            break;
        }
        case MCP_CLKGEN_WRITE_REG:
        {
            uint8_t data[1] = {0};
            
            if(!i2c_read(device, (uint32_t)(confData[i].addr),
                            data, 1, true, ZL302XX_ADDR_LEN))
            {
                LOG_ERROR("MCP CLKGEN REG DUMP Failed to read reg %d", confData[i].addr);
                return;
            }
            
            if(data[1] == confData[i].value)
            {
                printf("\t%04X : %02X\r\n", confData[i].addr, data[1]);
            }
            else
            {
                printf("\t%04X : %02X (expected %02X)\r\n", confData[i].addr, data[1], confData[i].value);
            }

            break;
        }
        default:
            //Error
            break;    
        }
    }
}