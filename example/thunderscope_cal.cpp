/**
 * This file is part of libtslitex.
 *
 * Copyright (c) 2020-2021 Florent Kermarrec <florent@enjoy-digital.fr>
 * Copyright (c) 2022 Franck Jullien <franck.jullien@collshade.fr>
 * Copyright (c) 2020 Antmicro <www.antmicro.com>
 * Copyright (c) 2024 John Simons <jammsimons@gmail.com>
 * Copyright (c) 2024 Nate Meyer <nate.devel@gmail.com>
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <inttypes.h>
#include <fcntl.h>
#include <signal.h>
#include <time.h>

#include <chrono>
#include <iostream>
#include <thread>


#ifdef _WIN32
#include <Windows.h>
#endif

#define OPTPARSE_IMPLEMENTATION
#include "optparse.h"

#include "../src/mcp443x.h"
#include "../src/platform.h"

#include "liblitepcie.h"
#include "thunderscope.h"
#include "ts_calibration.h"

#ifdef _WIN32
#define FILE_FLAGS  (FILE_ATTRIBUTE_NORMAL)
#else
#define FILE_FLAGS  (O_RDWR)
#endif

#define FLUSH() { char c; do { c = getchar(); } while(c != '\n' && c != 'EOF');}

/* Variables */
/*-----------*/
tsScopeCalibration_t calibration;

/* Main */
/*------*/
// Functioning returning 
// current time
auto now()
{
    return std::chrono::steady_clock::now();
}

// Function calculating sleep time 
// with 500ms delay
auto awake_time()
{
    using std::chrono::operator"" ms;
    return now() + 500ms;
}

static void cal_step_0(tsHandle_t pTs, uint8_t chanBitmap)
{
    //Set Channels to unity gain, no offset
    tsChannelParam_t param = {0};
    param.active = false;
    param.volt_scale_mV = 700;
    param.volt_offset_mV = 0;
    param.bandwidth = 0;
    param.coupling = TS_COUPLE_AC;
    param.term = TS_TERM_1M;
    
    for(uint32_t i=0; i < TS_NUM_CHANNELS; i++)
    {
        if((chanBitmap >> i) & 0x1)
        {
            thunderscopeChannelConfigSet(pTs, i, &param);
        }
    }
    return;
}

static void cal_step_1(uint8_t chanBitmap)
{
    // Measure +VBIAS
    printf("Measure the +VBIAS Test Point\r\n");

    while(1)
    {
        double vbias = 0;
        printf("Enter value of +VBIAS: ");
        if((scanf("%lf", &vbias) > 0) && (vbias < 5.0))
        {
            printf("Saving value of %i mV for +VBIAS\r\n", (int32_t)(vbias * 1000));
            for(uint32_t i=0; i < TS_NUM_CHANNELS; i++)
            {
                if((chanBitmap >> i) & 0x1)
                {
                    calibration.afeCal[i].bias_mV = (int32_t)(vbias * 1000);
                }
            }
            break;
        }
        printf("Invalid Value for VBIAS\r\n");
    }

    FLUSH();
    printf("<Press ENTER to continue>\r\n");
    while(getchar() != '\n') {;;}

    return;
}

static void cal_step_2(tsHandle_t pTs, uint8_t chanBitmap)
{
    double vtrim[2][TS_NUM_CHANNELS] = {0};
    tsChannelCtrl_t afe_ctrl = {0};
    afe_ctrl.atten = 0;
    // Characterize DPOT
    //Set DAC and DPOT
    afe_ctrl.dac = 1000;
    // afe_ctrl.dac = 1 * 4095 / 5;
    afe_ctrl.dpot = MCP4432_MAX;

    printf("Setting V_DAC to 1V and R_TRIM to 50k\r\n");

    for(uint32_t i=0; i < TS_NUM_CHANNELS; i++)
    {
        if((chanBitmap >> i) & 0x1)
        {
            thunderscopeCalibrationManualCtrl(pTs, i, afe_ctrl);
        }
    }

    // Measure VTRIM
    printf("Measure the VTRIM Test Point(s)\r\n");

    //User input of value and tracking to build calibration
    for(uint32_t i=0; i < TS_NUM_CHANNELS; i++)
    {
        if((chanBitmap >> i) & 0x1)
        {
            while(1)
            {
                double measure = 0;
                printf("Enter value of Channel %d VTRIM: ", i);
                if((scanf("%lf", &measure) > 0) && (measure < 5.0))
                {
                    printf("Storing value of %.03lf V for VTRIM\r\n", measure);
                            vtrim[0][i] = measure;
                    break;
                }
                printf("Invalid Value for VTRIM\r\n");
            }
        }
    }

    FLUSH();
    printf("<Press ENTER to continue>\r\n");
    while(getchar() != '\n') {;;}
    
    afe_ctrl.dpot = MCP4432_MAX/2;

    printf("Setting V_DAC to 1V and R_TRIM to 25k\r\n");

    for(uint32_t i=0; i < TS_NUM_CHANNELS; i++)
    {
        if((chanBitmap >> i) & 0x1)
        {
            thunderscopeCalibrationManualCtrl(pTs, i, afe_ctrl);
        }
    }

    // Measure VTRIM
    printf("Measure the VTRIM Test Point(s)\r\n");

    //User input of value and tracking to build calibration
    for(uint32_t i=0; i < TS_NUM_CHANNELS; i++)
    {
        if((chanBitmap >> i) & 0x1)
        {
            while(1)
            {
                double measure = 0;
                printf("Enter value of Channel %d VTRIM: ", i);
                if((scanf("%lf", &measure) > 0) && (measure < 5.0))
                {
                    printf("Storing value of %.03lf V for VTRIM\r\n", measure);
                            vtrim[1][i] = measure;
                    break;
                }
                printf("Invalid Value for VTRIM\r\n");
            }
        }
    }

    // Calculate DPOT value
    for(uint32_t i=0; i < TS_NUM_CHANNELS; i++)
    {
        if((chanBitmap >> i) & 0x1)
        {
            calibration.afeCal[i].trimRheostat_range = (500.0 * (2 * vtrim[1][i] - vtrim[0][i] - 1.0)) / (vtrim[0][i] - vtrim[1][i]);
            printf("Setting Channel %d Trim DPot to a full range of %d Ohms\r\n", i, calibration.afeCal[i].trimRheostat_range);
        }
    }

    FLUSH();
    printf("<Press ENTER to continue>\r\n");
    while(getchar() != '\n') {;;}
}

static void cal_step_3(tsHandle_t pTs, uint8_t chanBitmap)
{
    // Characterize Input Bias Current
    double vtrim[TS_NUM_CHANNELS] = {0};
    tsChannelCtrl_t afe_ctrl = {0};
    afe_ctrl.atten = 0;


    for(uint32_t i=0; i < TS_NUM_CHANNELS; i++)
    {
        if((chanBitmap >> i) & 0x1)
        {
            //Set DAC and DPOT
            printf("Setting channel %d V_DAC to %dmV and R_TRIM to %d Ohm\r\n", i, calibration.afeCal[i].bias_mV, calibration.afeCal[i].trimRheostat_range);
            afe_ctrl.dac = (uint16_t)(calibration.afeCal[i].bias_mV);
            // afe_ctrl.dac = (int32_t)(calibration.afeCal[i].bias_mV * 4095 / 5);
            afe_ctrl.dpot =  ((((500) * calibration.afeCal[i].trimRheostat_range) / MCP4432_MAX) + MCP4432_RWIPER);
            thunderscopeCalibrationManualCtrl(pTs, i, afe_ctrl);
        }
    }

    // Measure VTRIM
    printf("Measure the VTRIM Test Point(s)\r\n");

    //User input of value and tracking to build calibration
    for(uint32_t i=0; i < TS_NUM_CHANNELS; i++)
    {
        if((chanBitmap >> i) & 0x1)
        {
            while(1)
            {
                double measure = 0;
                printf("Enter value of Channel %d VTRIM: ", i);
                if((scanf("%lf", &measure) > 0) && (measure < 5.0))
                {
                    calibration.afeCal[i].preampInputBias_uA = (int32_t)(((((double)calibration.afeCal[i].bias_mV/1000) - measure)/250.0) * 1000000.0);
                    printf("Saving value of %i uA for Channel %d Preamp Input Bias Current\r\n", calibration.afeCal[i].preampInputBias_uA, i);
                    break;
                }
                printf("Invalid Value for VTRIM\r\n");
            }
        }
    }

    FLUSH();
    printf("<Press ENTER to continue>\r\n");
    while(getchar() != '\n') {;;}
}

static void cal_step_4(tsHandle_t pTs, uint8_t chanBitmap)
{
    // Characterize Preamp Output Offset

}

#define TS_CAL_STEPS 4
static void do_calibration(file_t fd, uint32_t idx, uint8_t channelBitmap, uint32_t stepNo)
{
    tsHandle_t tsHdl = thunderscopeOpen(idx);
    // tsChannelHdl_t channels;
    // sampleStream_t samp;

    // ts_channel_init(&channels, fd);
    // if(channels == NULL)
    // {
    //     printf("Failed to create channels handle");
    //     return;
    // }

    // samples_init(&samp, 0, 0);

    //Load Starting Config or set Default
    for(uint32_t i=0; i < TS_NUM_CHANNELS; i++)
    {
        if((channelBitmap >> i) & 0x1)
        {
            calibration.afeCal[i].buffer_mV = TS_VBUFFER_NOMINAL_MV;
            calibration.afeCal[i].bias_mV = TS_VBIAS_NOMINAL_MV;
            calibration.afeCal[i].attenuatorGain1M_mdB = TS_ATTENUATION_1M_GAIN_mdB;
            calibration.afeCal[i].attenuatorGain50_mdB = TS_TERMINATION_50OHM_GAIN_mdB;
            calibration.afeCal[i].bufferGain_mdB = TS_BUFFER_GAIN_NOMINAL_mdB;
            calibration.afeCal[i].trimRheostat_range = MCP4432_FULL_SCALE_OHM;
            calibration.afeCal[i].preampLowGainError_mdB = 0;
            calibration.afeCal[i].preampHighGainError_mdB = 0;
            calibration.afeCal[i].preampOutputGainError_mdB = 0;
            calibration.afeCal[i].preampLowOffset_mV = 0;
            calibration.afeCal[i].preampHighOffset_mV = 0;
            calibration.afeCal[i].preampInputBias_uA = TS_PREAMP_INPUT_BIAS_CURRENT_uA;

            thunderscopeCalibrationSet(tsHdl, i, calibration.afeCal[i]);
        }
    }



    //Intentially fall through each case to do all calibration steps
    switch(stepNo)
    {
    default:
    case 0:
        cal_step_0(tsHdl, channelBitmap);
    case 1:
        cal_step_1(channelBitmap);
    case 2:
        cal_step_2(tsHdl, channelBitmap);
    case 3:
        cal_step_3(tsHdl, channelBitmap);
    case 4:
        cal_step_4(tsHdl, channelBitmap);
    }

    //Apply Final Calibration
    for(uint32_t i=0; i < TS_NUM_CHANNELS; i++)
    {
        if((channelBitmap >> i) & 0x1)
        {
            thunderscopeCalibrationSet(tsHdl, i, calibration.afeCal[i]);
        }
    }

    // ts_channel_destroy(channels);
    // samples_teardown(&samp);
    thunderscopeClose(tsHdl);
}

//Capture average

    // uint8_t* sampleBuffer = (uint8_t*)calloc(TS_SAMPLE_BUFFER_SIZE * 10, 1);
    // uint64_t sampleLen = 0;

    // //Setup and Enable Channels
    // tsChannelParam_t chConfig = {0};
    // uint8_t channel = 0;
    // while(channelBitmap > 0)
    // {
    //     if(channelBitmap & 0x1)
    //     {
    //         thunderscopeChannelConfigGet(tsHdl, channel, &chConfig);
    //         chConfig.volt_scale_mV = volt_scale_mV;
    //         chConfig.volt_offset_mV = offset_mV;
    //         chConfig.bandwidth = bandwidth;
    //         chConfig.coupling = ac_couple ? TS_COUPLE_AC : TS_COUPLE_DC;
    //         chConfig.term =  term ? TS_TERM_50 : TS_TERM_1M;
    //         chConfig.active = 1;
    //         // ts_channel_params_set(channels, channel, &chConfig);
    //         thunderscopeChannelConfigSet(tsHdl, channel, &chConfig);
    //         numChan++;
    //     }
    //     channel++;
    //     channelBitmap >>= 1;
    // }

    // uint64_t data_sum = 0;
    // //Start Sample capture
    // // samples_enable_set(&samp, 1);
    // // ts_channel_run(channels, 1);
    // thunderscopeDataEnable(tsHdl, 1);
    
    // if(sampleBuffer != NULL)
    // {
    //     for(uint32_t loop=0; loop < 100; loop++)
    //     {
    //         uint32_t readReq = (TS_SAMPLE_BUFFER_SIZE * 0x8000);
    //         //Collect Samples
    //         // int32_t readRes = samples_get_buffers(&samp, sampleBuffer, readReq);
    //         int32_t readRes = thunderscopeRead(tsHdl, sampleBuffer, readReq);
    //         if(readRes < 0)
    //         {
    //             printf("ERROR: Sample Get Buffers failed with %" PRIi32, readRes);
    //         }
    //         if(readRes != readReq)
    //         {
    //             printf("WARN: Read returned different number of bytes for loop %" PRIu32 ", %" PRIu32 " / %" PRIu32 "\r\n", loop, readRes, readReq);
    //         }
    //         data_sum += readReq;
    //         sampleLen = readRes;
    //     }
    // }

    // //Stop Samples
    // // samples_enable_set(&samp, 0);
    // // ts_channel_run(channels, 0);
    // thunderscopeDataEnable(tsHdl, 0);

    // if(sampleLen > 0)
    // {
    //     uint64_t sample = 0;
    //     uint64_t idx = 0;
    //     while (idx < sampleLen)
    //     {
    //         avg[0][sample] = sampleBuffer[idx++];
    //         if(numChan > 1)
    //         {
    //             avg[1][sample] = sampleBuffer[idx++];
    //             if(numChan > 2)
    //             {
    //                 avg[2][sample] = sampleBuffer[idx++];
    //                 avg[3][sample] = sampleBuffer[idx++];
    //             }
    //         }
    //         sample++;
    //     }
    // }

    
    // //Disable channels
    // for(uint8_t i=0; i < TS_NUM_CHANNELS; i++)
    // {
    //     // ts_channel_params_get(channels, i, &chConfig);
    //     thunderscopeChannelConfigGet(tsHdl, i, &chConfig);
    //     chConfig.active = 0;
    //     // ts_channel_params_set(channels, i, &chConfig);
    //     thunderscopeChannelConfigSet(tsHdl, i, &chConfig);
    // }

static void print_help(void)
{
    printf("TS Calibration Util Usage:\r\n");
    printf("\t -d <device>      Device Index\r\n");
    printf("\t -c <channels>    Channel bitmap\r\n");
    printf("\t -s <step>        Skip to step #\r\n");
}

/* Main */
/*------*/

int main(int argc, char** argv)
{
    const char* cmd = argv[0];
    unsigned char fpga_identifier[256];
    char devicePath[TS_IDENT_STR_LEN];


    file_t fd;
    uint32_t idx = 0;
    int i;
    uint8_t channelBitmap = 0x01;
    uint32_t stepNo = 0;

    struct optparse_long argList[] = {
        {"dev",      'd', OPTPARSE_REQUIRED},
        {"chan",     'c', OPTPARSE_REQUIRED},
        {"step",     's', OPTPARSE_REQUIRED},
        {0}
    };

    auto argCount = 1;
    char *arg = argv[argCount];
    int option;
    struct optparse options;

    (void)argc;
    optparse_init(&options, argv);
    while ((option = optparse_long(&options, argList, NULL)) != -1)
    {
        switch (option) {
        case 'c':
            channelBitmap = strtol(options.optarg, NULL, 0);
            argCount+=2;
            break;
        case 'd':
            idx = strtol(options.optarg, NULL, 0);
            argCount+=2;
            break;
        case 's':
            stepNo = strtol(options.optarg, NULL, 0);
            argCount+=2;
            break;
        case '?':
            fprintf(stderr, "%s: %s\n", argv[0], options.errmsg);
            print_help();
            exit(EXIT_SUCCESS);
        }
    }
    arg = argv[argCount];

    snprintf(devicePath, TS_IDENT_STR_LEN, LITEPCIE_CTRL_NAME(%d), idx);
    printf("Opening Device %s\n", devicePath);
    fd = litepcie_open((const char*)devicePath, FILE_FLAGS);
    if(fd == INVALID_HANDLE_VALUE) {
        fprintf(stderr, "Could not init driver\n");
        exit(1);
    }


    printf("\x1b[1m[> FPGA/SoC Information:\x1b[0m\n");
    printf("------------------------\n");

    for (i = 0; i < 256; i++)
    {
        fpga_identifier[i] = litepcie_readl(fd, CSR_IDENTIFIER_MEM_BASE + 4 * i);
    }
    printf("FPGA Identifier:  %s.\n", fpga_identifier);

#ifdef CSR_DNA_BASE
    printf("FPGA DNA:         0x%08x%08x\n",
        litepcie_readl(fd, CSR_DNA_ID_ADDR + 4 * 0),
        litepcie_readl(fd, CSR_DNA_ID_ADDR + 4 * 1));
#endif
#ifdef CSR_XADC_BASE
    printf("FPGA Temperature: %0.1f �C\n",
        (double)litepcie_readl(fd, CSR_XADC_TEMPERATURE_ADDR) * 503.975 / 4096 - 273.15);
    printf("FPGA VCC-INT:     %0.2f V\n",
        (double)litepcie_readl(fd, CSR_XADC_VCCINT_ADDR) / 4096 * 3);
    printf("FPGA VCC-AUX:     %0.2f V\n",
        (double)litepcie_readl(fd, CSR_XADC_VCCAUX_ADDR) / 4096 * 3);
    printf("FPGA VCC-BRAM:    %0.2f V\n",
        (double)litepcie_readl(fd, CSR_XADC_VCCBRAM_ADDR) / 4096 * 3);
#endif

    // Walk the user through calibration steps
    do_calibration(fd, idx, channelBitmap, stepNo);

    /* Close LitePCIe device. */
    litepcie_close(fd);

    return 0;
}
