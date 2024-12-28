/* SPDX-License-Identifier: BSD-2-Clause
 *
 * This file is part of libtslitex.
 * Provide methods to fetch and store content in flash
 *
 * Copyright (c) 2024 Nate Meyer <nate.devel@gmail.com>
 */

#include "ts_fw_manager.h"
#include "platform.h"
#include "spiflash.h"
#include "util.h"

static const char* ts_fw_parse_bit_header(const char* header, const char** part, uint32_t* bin_len)
{
    const char* position = header;
    uint16_t key_len = 0;

    //First Field ('0FF0...')
    key_len = (uint16_t)(position[0] << 8) + (position[1]);
    if(key_len != 9)
    {
        LOG_ERROR("Invalid bitstream FIELD 1 (%d)", key_len);
        return NULL;
    }
    position += (2+key_len);

    //Second Field ('a')
    key_len = (uint16_t)(position[0] << 8) + (position[1]);
    if(key_len != 1 && position[2] != 'a')
    {
        LOG_ERROR("Invalid bitstream FIELD 2 (%d)", key_len);
        return NULL;
    }
    position += (2+key_len);

    //Third Field (Design Name)
    key_len = (uint16_t)(position[0] << 8) + (position[1]);
    LOG_DEBUG("Parsing Bitstream for project: %s", &position[2]);
    position += (2+key_len);

    //Fourth Field ('b' + Part Name)
    if(position[0] != 'b')
    {
        LOG_ERROR("Invalid bitstream FIELD 4 (%c)", position[0]);
        return NULL;
    }
    key_len = (uint16_t)(position[1] << 8) + (position[2]);
    LOG_DEBUG("Part: %s", &position[3]);
    position += (3+key_len);

    //Fifth Field ('c' + Build Date)
    key_len = (uint16_t)(position[1] << 8) + (position[2]);
    if((position[0] != 'c') || (key_len != 11))
    {
        LOG_ERROR("Invalid bitstream FIELD 5 (%c)", position[0]);
        return NULL;
    }
    LOG_DEBUG("Build Date: %s", &position[3]);
    position += (3+key_len);

    
    //Sixth Field ('d' + Build Time)
    key_len = (uint16_t)(position[1] << 8) + (position[2]);
    if((position[0] != 'd') || (key_len != 9))
    {
        LOG_ERROR("Invalid bitstream FIELD 6 (%c)", position[0]);
        return NULL;
    }
    LOG_DEBUG("Build Time: %s", &position[3]);
    position += (3+key_len);

    
    //Seventh Field ('e' + Bitstream)
    if(position[0] != 'e')
    {
        LOG_ERROR("Invalid bitstream FIELD 6 (%c)", position[0]);
        return NULL;
    }
    *bin_len = (uint32_t)(((uint32_t)position[1] << 24) + ((uint32_t)position[2] << 16) + ((uint32_t)position[3] << 8) + (uint32_t)position[4]);
    LOG_DEBUG("Bitstream Length: %u", *bin_len);
    position += 5;

    return position;
}

int32_t ts_fw_manager_init(file_t fd, ts_fw_manager_t* mngr)
{
    if(mngr == NULL)
    {
        LOG_ERROR("Invalid manager handle");
        return TS_STATUS_ERROR;
    }
    //Initialize SPI Flash
    spiflash_init(fd, &mngr->flash_dev);

    //Set partition Table
    if( mngr->flash_dev.mfg_code == TS_FLASH_256M_MFG)
    {
        mngr->partition_table = &ts_256Mb_layout;
    }
    else if( mngr->flash_dev.mfg_code == TS_FLASH_64M_MFG)
    {
        mngr->partition_table = &ts_64Mb_layout;
    }

    // Lock Factory Partitions
    //TODO

    return TS_STATUS_OK;
}

int32_t ts_fw_manager_user_fw_update(ts_fw_manager_t* mngr, const char* file_stream, uint32_t len)
{
    // Verify Bitstream and Strip Header Info
    uint32_t bin_length = 0;
    const char* part_name = NULL;
    const char* bin_start = ts_fw_parse_bit_header(file_stream, &part_name, &bin_length);
    if(bin_length > len)
    {
        LOG_ERROR("INVALID length in bitstream header (%u > %u)", bin_length, len);
        return TS_STATUS_ERROR;
    }
    //TODO - Verify FPGA Part 

    // Verify File Length Good
    uint32_t user_partition_len = mngr->partition_table->user_bitstream_end - mngr->partition_table->user_bitstream_start;
    if(bin_length > user_partition_len)
    {
        LOG_ERROR("Bad bitstream length.  Supplied length %u is longer than the allowed %u", bin_length, user_partition_len);
        return TS_INVALID_PARAM;
    }

    // Erase User Flash Partition
    if(TS_STATUS_OK != spiflash_erase(&mngr->flash_dev, mngr->partition_table->user_bitstream_start, bin_length))
    {
        LOG_ERROR("Failed to erase user bitstream partition");
        return TS_STATUS_ERROR;
    }

    // Program New Bitstream
    if(bin_length != spiflash_write(&mngr->flash_dev, mngr->partition_table->user_bitstream_start, bin_start, bin_length))
    {
        LOG_ERROR("Failed to write user bitstream partition");
        return TS_STATUS_ERROR;
    }

    return TS_STATUS_OK;
}

int32_t ts_fw_manager_user_cal_get(ts_fw_manager_t* mngr, const char* file_stream, uint32_t max_len)
{
    // Read File from SPI Flash

    return TS_STATUS_OK;
}

int32_t ts_fw_manager_user_cal_update(ts_fw_manager_t* mngr, const char* file_stream, uint32_t len)
{
    // Verify File Good

    // Erase User Flash Partition

    // Program New Bitstream
    return TS_STATUS_OK;
}

int32_t ts_fw_manager_factory_cal_get(ts_fw_manager_t* mngr, const char* file_stream, uint32_t len)
{
    // Read File from SPI Flash

    return TS_STATUS_OK;
}
