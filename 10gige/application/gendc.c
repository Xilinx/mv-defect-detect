/******************************************************************************/
/*  GigE Vision Core Firmware                                                 */
/*----------------------------------------------------------------------------*/
/*    File :  gendc.c                                                         */
/*    Date :  2022-03-09                                                      */
/*     Rev :  2.1                                                             */
/*  Author :  JP                                                              */
/*----------------------------------------------------------------------------*/
/*  GigE Vision reference design GenDC front-end IP core functions            */
/*----------------------------------------------------------------------------*/
/*  1.0  |  2019-09-18  |  JP  |  Initial release                             */
/*  1.1  |  2019-10-30  |  JP  |  New test pattern generator                  */
/*  2.0  |  2022-03-04  |  SS  |  Ported for Linux application                */
/*  2.1  |  2022-03-09  |  SS  |  Prepared for kr260                          */
/******************************************************************************/

#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <printf.h>
#include <stdint.h>
#include "libGigE.h"
#include "gendc.h"


// -- Global variables ---------------------------------------------------------

// Software-only registers
uint32_t  gendc_flow_table_len;
uint32_t *gendc_flow_table;

// GenDC descriptor for intensity RGB planar 2D image 1024x256 @ 8bpp
//const uint32_t GENDC_DEFAULT_DESCRIPTOR[] =
//{
//    /* Container */
//    /*   0 */   0x474E4443, 0x01000000,
//    /*   8 */   0x00100000, 0x40000000,
//    /*  16 */   0x01000000, 0x00000000,
//    /*  24 */   0x00000000, 0x00000000,
//    /*  32 */   0x00000C00, 0x00000000,
//    /*  40 */   0x30010000, 0x00000000,
//    /*  48 */   0x30010000, 0x01000000,
//    /*  56 */   0x40000000, 0x00000000,
//    /* Component */
//    /*  64 */   0x00200000, 0x48000000,
//    /*  72 */   0x00000000, 0x00000000,
//    /*  80 */   0x00000000, 0x00000000,
//    /*  88 */   0x00000000, 0x00000000,
//    /*  96 */   0x01000000, 0x00000000,
//    /* 104 */   0x21001802, 0x00000300,
//    /* 112 */   0x88000000, 0x00000000,
//    /* 120 */   0xC0000000, 0x00000000,
//    /* 128 */   0xF8000000, 0x00000000,
//    /* Part 0 */
//    /* 136 */   0x00420000, 0x38000000,
//    /* 144 */   0xC9000801, 0x00000000,
//    /* 152 */   0x30010000, 0x00000000,
//    /* 160 */   0x00000400, 0x00000000,
//    /* 168 */   0x30010000, 0x00000000,
//    /* 176 */   0x00040000, 0x00010000,
//    /* 184 */   0x00000000, 0x00000000,
//    /* Part 1 */
//    /* 192 */   0x00420000, 0x38000000,
//    /* 200 */   0xCD000801, 0x00000000,
//    /* 208 */   0x30010400, 0x00000000,
//    /* 216 */   0x00000400, 0x00000000,
//    /* 224 */   0x30010400, 0x00000000,
//    /* 232 */   0x00040000, 0x00010000,
//    /* 240 */   0x00000000, 0x00000000,
//    /* Part 2 */
//    /* 248 */   0x00420000, 0x38000000,
//    /* 256 */   0xD1000801, 0x00000000,
//    /* 264 */   0x30010800, 0x00000000,
//    /* 272 */   0x00000400, 0x00000000,
//    /* 280 */   0x30010800, 0x00000000,
//    /* 288 */   0x00040000, 0x00010000,
//    /* 296 */   0x00000000, 0x00000000
//};

// GenDC descriptor for single Mono16 2D image 1024x768 @ 8bpp
const uint32_t GENDC_DEFAULT_DESCRIPTOR[] =
{
    /* Container */
    /*   0 */   0x474E4443, 0x01000000,
    /*   8 */   0x00100000, 0x40000000,
    /*  16 */   0x01000000, 0x00000000,
    /*  24 */   0x00000000, 0x00000000,
    /*  32 */   0x00000C00, 0x00000000,
    /*  40 */   0xC8000000, 0x00000000,
    /*  48 */   0xC8000000, 0x01000000,
    /*  56 */   0x40000000, 0x00000000,
    /* Component */
    /*  64 */   0x00200000, 0x48000000,
    /*  72 */   0x00000000, 0x00000000,
    /*  80 */   0x00000000, 0x00000000,
    /*  88 */   0x00000000, 0x00000000,
    /*  96 */   0x01000000, 0x00000000,
    /* 104 */   0x01000801, 0x00000100,
    /* 112 */   0x88000000, 0x00000000,
    /* 120 */   0x00000000, 0x00000000,
    /* 128 */   0x00000000, 0x00000000,
    /* Part 0 */
    /* 136 */   0x00420000, 0x38000000,
    /* 144 */   0x01000801, 0x00000000,
    /* 152 */   0x000C0000, 0x00000000,
    /* 160 */   0x00000C00, 0x00000000,
    /* 168 */   0xC8000000, 0x00000000,
    /* 176 */   0x00040000, 0x00030000,
    /* 184 */   0x00000000, 0x00000000,
    /* 192 */   0x00000000, 0x00000000
};

// GenDC descriptor for dual Mono8 2D image 1024x768 @ 8bpp
//const uint32_t GENDC_DEFAULT_DESCRIPTOR[] =
//{
//    /* Container */
//    /*   0 */   0x474E4443, 0x01000000,
//    /*   8 */   0x00100000, 0x40000000,
//    /*  16 */   0x01000000, 0x00000000,
//    /*  24 */   0x00000000, 0x00000000,
//    /*  32 */   0x00001800, 0x00000000,
//    /*  40 */   0xF8000000, 0x00000000,
//    /*  48 */   0xF8000000, 0x01000000,
//    /*  56 */   0x40000000, 0x00000000,
//    /* Component */
//    /*  64 */   0x00200000, 0x48000000,
//    /*  72 */   0x00000000, 0x00000000,
//    /*  80 */   0x00000000, 0x00000000,
//    /*  88 */   0x00000000, 0x00000000,
//    /*  96 */   0x01000000, 0x00000000,
//    /* 104 */   0x01000801, 0x00000200,
//    /* 112 */   0x88000000, 0x00000000,
//    /* 120 */   0xC0000000, 0x00000000,
//    /* 128 */   0x00000000, 0x00000000,
//    /* Part 0 */
//    /* 136 */   0x00420000, 0x38000000,
//    /* 144 */   0x01000801, 0x00000000,
//    /* 152 */   0x000F8000, 0x00000000,
//    /* 160 */   0x00000C00, 0x00000000,
//    /* 168 */   0xC8000000, 0x00000000,
//    /* 176 */   0x00040000, 0x00030000,
//    /* 184 */   0x00000000, 0x00000000,
//    /* Part 1 */
//    /* 192 */   0x00420000, 0x38000000,
//    /* 200 */   0x01000801, 0x00000000,
//    /* 208 */   0x000F8000, 0x00000000,
//    /* 216 */   0x00000C00, 0x00000000,
//    /* 224 */   0xC8000000, 0x00000000,
//    /* 232 */   0x00040000, 0x00030000,
//    /* 240 */   0x00000000, 0x00000000
//};

// GenDC descriptor for two components, one Mono8 2D image 1024x768 @ 8bpp each
//const uint32_t GENDC_DEFAULT_DESCRIPTOR[] =
//{
//    /* Container */
//    /*   0 */   0x474E4443, 0x01000000,
//    /*   8 */   0x00100000, 0x40000000,
//    /*  16 */   0x01000000, 0x00000000,
//    /*  24 */   0x00000000, 0x00000000,
//    /*  32 */   0x00001800, 0x00000000,
//    /*  40 */   0x48010000, 0x00000000,
//    /*  48 */   0x48010000, 0x02000000,
//    /*  56 */   0x48000000, 0x00000000,
//    /*  64 */   0xC8000000, 0x00000000,
//    /* Component 0*/
//    /*  72 */   0x00200000, 0x38000000,
//    /*  80 */   0x00000000, 0x00000000,
//    /*  88 */   0x00000000, 0x00000000,
//    /*  96 */   0x00000000, 0x00000000,
//    /* 104 */   0x01000000, 0x00000000,
//    /* 112 */   0x01000801, 0x00000100,
//    /* 120 */   0x90000000, 0x00000000,
//    /* 128 */   0x00000000, 0x00000000,
//    /* 136 */   0x00000000, 0x00000000,
//    /* Part 0 of Component 0*/
//    /* 144 */   0x00420000, 0x38000000,
//    /* 152 */   0x01000801, 0x00000000,
//    /* 160 */   0x000F8000, 0x00000000,
//    /* 168 */   0x00000C00, 0x00000000,
//    /* 176 */   0xC8000000, 0x00000000,
//    /* 185 */   0x00040000, 0x00030000,
//    /* 192 */   0x00000000, 0x00000000,
//    /* Component 1*/
//    /* 200 */   0x00200000, 0x38000000,
//    /* 208 */   0x00000000, 0x00000000,
//    /* 216 */   0x00000000, 0x00000000,
//    /* 224 */   0x00000000, 0x00000000,
//    /* 232 */   0x01000000, 0x00000000,
//    /* 240 */   0x01000801, 0x00000100,
//    /* 248 */   0x10010000, 0x00000000,
//    /* 256 */   0x00000000, 0x00000000,
//    /* 264 */   0x00000000, 0x00000000,
//    /* Part 0 Component 1 */
//    /* 272 */   0x00420000, 0x38000000,
//    /* 280 */   0x01000801, 0x00000000,
//    /* 288 */   0x000F8000, 0x00000000,
//    /* 296 */   0x00000C00, 0x00000000,
//    /* 304 */   0xC8000000, 0x00000000,
//    /* 312 */   0x00040000, 0x00030000,
//    /* 320 */   0x00000000, 0x00000000
//};

const uint32_t GENDC_DESCRIPTOR_SIZE = sizeof(GENDC_DEFAULT_DESCRIPTOR);

// GenDC flow mapping table for intensity RGB planar/Mono8 2D image
// with all parts in flow 0
uint32_t GENDC_DEFAULT_FLOW_TABLE[] =
{
    /* Header */
    /*   0 */   0x00700000, 0x18000000,
    /*   8 */   0x01000000, 0x01000000,
    /* FlowSize[0] */
    /*  16 */   0x30010C00, 0x00000000
};


const uint32_t GENDC_FLOW_TABLE_SIZE = sizeof(GENDC_DEFAULT_FLOW_TABLE);


// ---- Print GenDC core info --------------------------------------------------
//
void gendc_info(uint8_t device)
{
    uint32_t i, parts = (gendc_config >> 24) & 0xFF;

    printf("ID:            0x%08X\r\n", gige_get_user_register(device, gendc_id));
    printf("Version:       %d.%d.%d\r\n", (gige_get_user_register(device, gendc_version) >> 24) & 0xFF,
                                              (gige_get_user_register(device, gendc_version) >> 16) & 0xFF,
                                               gige_get_user_register(device, gendc_version)        & 0xFFFF);
    printf("Build date:    %04X-%02X-%02X\r\n", (gige_get_user_register(device, gendc_date) >> 16) & 0xFFFF,
                                                    (gige_get_user_register(device, gendc_date) >>  8) & 0xFF,
                                                     gige_get_user_register(device, gendc_date)        & 0xFF);
    printf("Configuration: %d part(s)\r\n", (gige_get_user_register(device, gendc_config) >> 24) & 0xFF);
    printf("               %db input width\r\n", (gige_get_user_register(device, gendc_config) >> 16) & 0xFF);
    printf("               %d words descriptor DPRAM\r\n", gige_get_user_register(device, gendc_config) & 0xFFFF);
    printf("Packet size:   %d bytes\r\n", gige_get_user_register(device, gendc_pktsize));
    printf("control:       0x%08X\r\n", gige_get_user_register(device, gendc_control));
    printf("desc_flags:    0x%08X\r\n", gige_get_user_register(device, gendc_desc_flags));
    printf("desc_offs_h:   0x%08X\r\n", gige_get_user_register(device, gendc_desc_offs_h));
    printf("desc_offs_l:   0x%08X\r\n", gige_get_user_register(device, gendc_desc_offs_l));
    printf("desc_len:      0x%08X\r\n", gige_get_user_register(device, gendc_desc_len));
    for (i = 0; i < parts; i++)
    {
        printf("Part %u:\r\n", (uint32_t)i);
        printf(" - status:     0x%08X\r\n", gige_get_user_register(device, gendc_part_status(i)));
        printf(" - gendc_part_ts_h:    0x%08X\r\n", gige_get_user_register(device, gendc_part_ts_h(i)));
        printf(" - gendc_part_ts_l:    0x%08X\r\n", gige_get_user_register(device, gendc_part_ts_l(i)));
        printf(" - gendc_part_bytes_h: 0x%08X\r\n", gige_get_user_register(device, gendc_part_bytes_h(i)));
        printf(" - gendc_part_bytes_l: 0x%08X\r\n", gige_get_user_register(device, gendc_part_bytes_l(i)));
        printf(" - lines:      %d\r\n", gige_get_user_register(device, gendc_part_lines(i)));
        printf(" - frames:     %d\r\n", gige_get_user_register(device, gendc_part_frames(i)));
        printf(" - FIFO depth: %d\r\n", gige_get_user_register(device, gendc_part_config(i)) & 0xFFFF);
        printf(" - gendc_part_control: 0x%08X\r\n", gige_get_user_register(device, gendc_part_control(i)));
        printf(" - gendc_part_flags:   0x%08X\r\n", gige_get_user_register(device, gendc_part_flags(i)));
        printf(" - gendc_part_offs_h:  0x%08X\r\n", gige_get_user_register(device, gendc_part_offs_h(i)));
        printf(" - gendc_part_offs_l:  0x%08X\r\n", gige_get_user_register(device, gendc_part_offs_l(i)));
    }
    return;
}


// ---- Initialize the GenDC core and data structures --------------------------
//
// NOTE: Hardcoded for default Mono8
//
void gendc_init(uint8_t device, uint32_t size_x, uint32_t size_y,
                uint32_t offs_x, uint32_t offs_y,
                uint32_t pad_x, uint32_t pad_y, uint32_t bpp, uint32_t pixel_fmt)
{
    uint64_t i;

    // Initialize GenDC timestamp generator
    // NOTE: For GenDC & FB clock frequency 150 MHz increment timestamp counter
    //       every 3 clock cycles by 20 to get timestamp in nanoseconds
    gige_set_user_register(device, gendc_ts_gen,
                           GENDC_TS_GEN_CLEAR |
                           (( 2 << 12) & GENDC_TS_GEN_STEP) |    // Every 3 clock cycles
                           ( 20        & GENDC_TS_GEN_INC));     // Increment by 20

    // Setup GenDC front end (legacy Mono8 mode)
    gendc_update(device, size_x, size_y, pad_x, pad_y, bpp, 0);

    // Initialize default GenDC descriptor and update it
    for (i = 0; i < (GENDC_DESCRIPTOR_SIZE / sizeof(uint32_t)); i++)
    {
        gige_set_user_register(device, gendc_descriptor + i*4, GENDC_DEFAULT_DESCRIPTOR[i]);
    }
    gige_set_user_register(device, gendc_desc_len, GENDC_DESCRIPTOR_SIZE);
    gendc_update_descriptor(device, size_x, size_y, offs_x, offs_y, pad_x, pad_y, bpp, pixel_fmt);

    // Initialize flow mapping table and update it
    gendc_flow_table_len = GENDC_FLOW_TABLE_SIZE;
    gendc_flow_table     = (uint32_t *)GENDC_DEFAULT_FLOW_TABLE;
    gendc_update_flow_table(device, size_x, size_y, pad_x, pad_y, bpp);

    return;
}


// NOTE: Hardcoded for legacy Mono8 and GenDC Mono 2D/planar RGB 2D image
//
void gendc_update(uint8_t device, uint32_t size_x, uint32_t size_y,
                  uint32_t pad_x, uint32_t pad_y, uint32_t bpp, uint32_t gendc_en)
{
    uint64_t i;
    uint64_t parts = (gige_get_user_register(device, gendc_config) >> 24) & 0xFF;
    uint32_t len_part = ((((size_x * bpp) / 8) + pad_x) * size_y) + pad_y;
    uint32_t part_contr;

    // Mono8
    if (gendc_en)
    {
        // GenDC descriptor generator
        gige_set_user_register(device, gendc_desc_flags, 0x70800000);     // First and last packet of final descriptor, first packet of flow 0
        gige_set_user_register(device, gendc_desc_offs_h, 0);
        gige_set_user_register(device, gendc_desc_offs_l, 0);
        gige_set_user_register(device, gendc_control, GENDC_CONTROL_DESCR_EN |
                                                      GENDC_CONTROL_UPDATE_ID |
                                                     (GENDC_CONTROL_ID_ADDR & (2 << 16)));

        // Setup individual parts
        for (i = 0; i < parts; i++)
        {
            if (i < PART_NUM)
            {
                uint32_t desc_len = gige_get_user_register(device, gendc_desc_len);
                gige_set_user_register(device, gendc_part_flags(i), 0x00000000);     // No descriptor data, flow 0
                gige_set_user_register(device, gendc_part_offs_h(i), 0);
                gige_set_user_register(device, gendc_part_offs_l(i), desc_len + ((uint32_t)i * len_part));
                gige_set_user_register(device, gendc_part_control(i), GENDC_PART_CONTROL_ENABLE |
                                                                      GENDC_PART_CONTROL_GENDC_EN);
            }
            else
            {
                gige_set_user_register(device, gendc_part_control(i), 0);
            }
        }
        // Dynamic update of component timestamp
        part_contr = gige_get_user_register(device, gendc_part_control(0));
        gige_set_user_register(device, gendc_part_control(0), part_contr |
                                                              GENDC_PART_CONTROL_UPDATE_TS |
                                                              (GENDC_PART_CONTROL_TS_ADDR & (11 << 16)));   //timestamp for component 1 is at offset 11*8byte
        //gendc_part_control(0) |= GENDC_PART_CONTROL_UPDATE_TS |
        //                        (GENDC_PART_CONTROL_TS_ADDR & (12 << 16));                                //timestamp for component1 is at offset 12*8byte
        //gendc_part_control(1) |= GENDC_PART_CONTROL_UPDATE_TS |
        //                        (GENDC_PART_CONTROL_TS_ADDR & (28 << 16));                                //timestamp for component2 is at offset 28*8byte
    }
    // Mono8
    else
    {
        // Disable GenDC descriptor
        gige_set_user_register(device, gendc_control, 0);

        // Enable part 0 pass-through
        gige_set_user_register(device, gendc_part_control(0), GENDC_PART_CONTROL_ENABLE);

        // Disable remaining parts
        for (i = 1; i < parts; i++)
            gige_set_user_register(device, gendc_part_control(i), 0);
    }

    return;
}

// ---- Update GenDC descriptor ------------------------------------------------
//
// NOTE: Hardcoded for planar RGB8 2D image
//
//void gendc_update_descriptor(uint32_t size_x, uint32_t size_y,
//                             uint32_t offs_x, uint32_t offs_y,
//                             uint32_t pad_x, uint32_t pad_y)
//{
//    uint32_t bpp = 8;
//    uint32_t len_descr = gendc_desc_len;
//    uint32_t len_part = ((((size_x * bpp) / 8) + pad_x) * size_y) + pad_y;
//    uint32_t padding = ((pad_y & 0xFFFF) << 16) | (pad_x & 0xFFFF);
//
//    // Update the descriptor
//    // ... container header
//    gendc_descriptor[ 8] = swap_endianness(len_part * 3);               // Container.DataSize
//    gendc_descriptor[ 9] = 0;
//    // ... component header
//    gendc_descriptor[20] = swap_endianness(offs_x);                     // Component.RegionOffsetX
//    gendc_descriptor[21] = swap_endianness(offs_y);                     // Component.RegionOffsetY
//    // ... part 0 header
//    gendc_descriptor[38] = swap_endianness(len_descr);                  // Part0.FlowOffset
//    gendc_descriptor[39] = 0;
//    gendc_descriptor[40] = swap_endianness(len_part);                   // Part0.DataSize
//    gendc_descriptor[41] = 0;
//    gendc_descriptor[42] = swap_endianness(len_descr);                  // Part0.DataOffset
//    gendc_descriptor[43] = 0;
//    gendc_descriptor[44] = swap_endianness(size_x);                     // Part0.SizeX
//    gendc_descriptor[45] = swap_endianness(size_y);                     // Part0.SizeY
//    gendc_descriptor[46] = swap_endianness(padding);                    // Part0.PaddingX & Part0.PaddingY
//    // ... part 1 header
//    gendc_descriptor[52] = swap_endianness(len_descr + len_part);       // Part1.FlowOffset
//    gendc_descriptor[53] = swap_endianness(0);
//    gendc_descriptor[54] = swap_endianness(len_part);                   // Part1.DataSize
//    gendc_descriptor[55] = swap_endianness(0);
//    gendc_descriptor[56] = swap_endianness(len_descr + len_part);       // Part1.DataOffset
//    gendc_descriptor[57] = swap_endianness(0);
//    gendc_descriptor[58] = swap_endianness(size_x);                     // Part1.SizeX
//    gendc_descriptor[59] = swap_endianness(size_y);                     // Part1.SizeY
//    gendc_descriptor[60] = swap_endianness(padding);                    // Part1.PaddingX & Part1.PaddingY
//    // ... part 2 header
//    gendc_descriptor[66] = swap_endianness(len_descr + (len_part * 2)); // Part2.FlowOffset
//    gendc_descriptor[67] = swap_endianness(0);
//    gendc_descriptor[68] = swap_endianness(len_part);                   // Part2.DataSize
//    gendc_descriptor[69] = swap_endianness(0);
//    gendc_descriptor[70] = swap_endianness(len_descr + (len_part * 2)); // Part2.DataOffset
//    gendc_descriptor[71] = swap_endianness(0);
//    gendc_descriptor[72] = swap_endianness(size_x);                     // Part2.SizeX
//    gendc_descriptor[73] = swap_endianness(size_y);                     // Part2.SizeY
//    gendc_descriptor[74] = swap_endianness(padding);                    // Part2.PaddingX & Part1.PaddingY
//
//    return;
//}

// NOTE: Hardcoded for one Mono8 2D image
//
void gendc_update_descriptor(uint8_t device, uint32_t size_x, uint32_t size_y,
                             uint32_t offs_x, uint32_t offs_y,
                             uint32_t pad_x, uint32_t pad_y, uint32_t bpp, uint32_t pixel_fmt)
{
    uint32_t len_descr = gige_get_user_register(device, gendc_desc_len);
    uint32_t len_part = ((((size_x * bpp) / 8) + pad_x) * size_y) + pad_y;
    uint32_t padding = ((pad_y & 0xFFFF) << 16) | (pad_x & 0xFFFF);

    // Update the descriptor
    // ... container header
    gige_set_user_register(device, gendc_descriptor + 8*4, swap_endianness(len_part));      // Container.DataSize
    gige_set_user_register(device, gendc_descriptor + 9*4, 0);
    // ... component header
    gige_set_user_register(device, gendc_descriptor + 20*4, swap_endianness(offs_x));       // Component.RegionOffsetX
    gige_set_user_register(device, gendc_descriptor + 21*4, swap_endianness(offs_y));       // Component.RegionOffsetY
    gige_set_user_register(device, gendc_descriptor + 26*4, swap_endianness(pixel_fmt));    // Component.PixelFormat

    // ... part 0 header
    gige_set_user_register(device, gendc_descriptor + 36*4, swap_endianness(pixel_fmt));    // Part0.PixelFormat
    gige_set_user_register(device, gendc_descriptor + 38*4, swap_endianness(len_descr));    // Part0.FlowOffset
    gige_set_user_register(device, gendc_descriptor + 39*4, 0);
    gige_set_user_register(device, gendc_descriptor + 40*4, swap_endianness(len_part));     // Part0.DataSize
    gige_set_user_register(device, gendc_descriptor + 41*4, 0);
    gige_set_user_register(device, gendc_descriptor + 42*4, swap_endianness(len_descr));    // Part0.DataOffset
    gige_set_user_register(device, gendc_descriptor + 43*4, 0);
    gige_set_user_register(device, gendc_descriptor + 44*4, swap_endianness(size_x));       // Part0.SizeX
    gige_set_user_register(device, gendc_descriptor + 45*4, swap_endianness(size_y));       // Part0.SizeY
    gige_set_user_register(device, gendc_descriptor + 46*4, swap_endianness(padding));      // Part0.PaddingX & Part0.PaddingY

    return;
}

// NOTE: Hardcoded for 2 Mono8 2D image
//
//void gendc_update_descriptor(uint32_t size_x, uint32_t size_y,
//                             uint32_t offs_x, uint32_t offs_y,
//                             uint32_t pad_x, uint32_t pad_y)
//{
//    uint32_t bpp = 8;
//    uint32_t len_descr = gendc_desc_len;
//    uint32_t len_part = ((((size_x * bpp) / 8) + pad_x) * size_y) + pad_y;
//    uint32_t padding = ((pad_y & 0xFFFF) << 16) | (pad_x & 0xFFFF);
//
//    // Update the descriptor
//    // ... container header
//    gendc_descriptor[ 8] = swap_endianness(len_part * 2);               // Container.DataSize
//    gendc_descriptor[ 9] = 0;
//    // ... component header
//    gendc_descriptor[20] = swap_endianness(offs_x);                     // Component.RegionOffsetX
//    gendc_descriptor[21] = swap_endianness(offs_y);                     // Component.RegionOffsetY
//    // ... part 0 header
//    gendc_descriptor[38] = swap_endianness(len_descr);                  // Part0.FlowOffset
//    gendc_descriptor[39] = 0;
//    gendc_descriptor[40] = swap_endianness(len_part);                   // Part0.DataSize
//    gendc_descriptor[41] = 0;
//    gendc_descriptor[42] = swap_endianness(len_descr);                  // Part0.DataOffset
//    gendc_descriptor[43] = 0;
//    gendc_descriptor[44] = swap_endianness(size_x);                     // Part0.SizeX
//    gendc_descriptor[45] = swap_endianness(size_y);                     // Part0.SizeY
//    gendc_descriptor[46] = swap_endianness(padding);                    // Part0.PaddingX & Part0.PaddingY
//    // ... part 1 header
//    gendc_descriptor[52] = swap_endianness(len_descr + len_part);       // Part1.FlowOffset
//    gendc_descriptor[53] = swap_endianness(0);
//    gendc_descriptor[54] = swap_endianness(len_part);                   // Part1.DataSize
//    gendc_descriptor[55] = swap_endianness(0);
//    gendc_descriptor[56] = swap_endianness(len_descr + len_part);       // Part1.DataOffset
//    gendc_descriptor[57] = swap_endianness(0);
//    gendc_descriptor[58] = swap_endianness(size_x);                     // Part1.SizeX
//    gendc_descriptor[59] = swap_endianness(size_y);                     // Part1.SizeY
//    gendc_descriptor[60] = swap_endianness(padding);                    // Part1.PaddingX & Part1.PaddingY
//
//    return;
//}

// NOTE: Hardcoded for 2 components with one Mono8 2D image each
//
//void gendc_update_descriptor(uint32_t size_x, uint32_t size_y,
//                             uint32_t offs_x, uint32_t offs_y,
//                             uint32_t pad_x, uint32_t pad_y)
//{
//    uint32_t bpp = 8;
//    uint32_t len_descr = gendc_desc_len;
//    uint32_t len_part = ((((size_x * bpp) / 8) + pad_x) * size_y) + pad_y;
//    uint32_t padding = ((pad_y & 0xFFFF) << 16) | (pad_x & 0xFFFF);
//
//    // Update the descriptor
//    // ... container header
//    gendc_descriptor[ 8] = swap_endianness(len_part * 2);               // Container.DataSize
//    gendc_descriptor[ 9] = 0;
//    // ... component0 header
//    gendc_descriptor[22] = swap_endianness(offs_x);                     // Component0.RegionOffsetX
//    gendc_descriptor[23] = swap_endianness(offs_y);                     // Component0.RegionOffsetY
//    // ... part 0 header
//    gendc_descriptor[40] = swap_endianness(len_descr);                  // Component0.Part0.FlowOffset
//    gendc_descriptor[41] = 0;
//    gendc_descriptor[42] = swap_endianness(len_part);                   // Component0.Part0.DataSize
//    gendc_descriptor[43] = 0;
//    gendc_descriptor[44] = swap_endianness(len_descr);                  // Component0.Part0.DataOffset
//    gendc_descriptor[45] = 0;
//    gendc_descriptor[46] = swap_endianness(size_x);                     // Component0.Part0.SizeX
//    gendc_descriptor[47] = swap_endianness(size_y);                     // Component0.Part0.SizeY
//    gendc_descriptor[48] = swap_endianness(padding);                    // Component0.Part0.PaddingX & Component0.Part0.PaddingY
//    // ... component1 header
//    gendc_descriptor[54] = swap_endianness(offs_x);                     // Component1.RegionOffsetX
//    gendc_descriptor[55] = swap_endianness(offs_y);                     // Component1.RegionOffsetY
//    // ... part 1 header
//    gendc_descriptor[72] = swap_endianness(len_descr + len_part);       // Component1.Part1.FlowOffset
//    gendc_descriptor[73] = swap_endianness(0);
//    gendc_descriptor[74] = swap_endianness(len_part);                   // Component1.Part1.DataSize
//    gendc_descriptor[75] = swap_endianness(0);
//    gendc_descriptor[76] = swap_endianness(len_descr + len_part);       // Component1.Part1.DataOffset
//    gendc_descriptor[77] = swap_endianness(0);
//    gendc_descriptor[78] = swap_endianness(size_x);                     // Component1.Part1.SizeX
//    gendc_descriptor[79] = swap_endianness(size_y);                     // Component1.Part1.SizeY
//    gendc_descriptor[80] = swap_endianness(padding);                    // Component1.Part1.PaddingX & Component1.Part1.PaddingY
//
//    return;
//}

// ---- Update GenDC flow mapping table ----------------------------------------
//

// NOTE: All parts on one flow
//
void gendc_update_flow_table(uint8_t device, uint32_t size_x, uint32_t size_y,
                             uint32_t pad_x, uint32_t pad_y, uint32_t bpp)
{
    //uint32_t len_part = ((((size_x * bpp) / 8) + pad_x) * size_y) + pad_y;
    uint32_t len_part = (((size_x * bpp) + pad_x) * size_y) + pad_y;

    // FlowSize[0]
    gendc_flow_table[4] = swap_endianness(gige_get_user_register(device, gendc_desc_len) + (PART_NUM * len_part));
    gendc_flow_table[5] = 0;

    return;
}
// ---- Calculate padding of one part ------------------------------------------
//
uint32_t gendc_part_padding(uint8_t device, uint32_t size_x, uint32_t size_y, uint32_t bpp, uint32_t gendc_en)
{
    uint32_t fs, ps, pad;

    // Frame size in bytes
    fs = (size_x * size_y * bpp) / 8;

    // Packet data payload size in bytes
    ps = gige_get_user_register(device, gendc_pktsize) & 0xFFFF;
    if (gendc_en)
        ps -= 16;               // Size of GenDC header in GEV payload packet

    // Padding of the part
    if (fs % ps)
        pad = ps - (fs % ps);
    else
        pad = 0;

    return pad;
}


// ---- Calculate total length of the GenDC container in framebuffer -----------
//
uint32_t gendc_fb_len(uint8_t device, uint32_t size_x, uint32_t size_y,
                      uint32_t pad_x, uint32_t pad_y,
                      uint32_t bpp, uint32_t gendc_en)
{
    uint32_t part_len   = ((((size_x * bpp) / 8) + pad_x) * size_y) + pad_y;
    uint32_t pkt_size   = (gige_get_user_register(device, gendc_pktsize) & 0xFFFF) - (gendc_en ? 16 : 0);
    uint32_t part_total = ((part_len + pkt_size - 1) / pkt_size) * (gige_get_user_register(device, gendc_pktsize) & 0xFFFF);
    uint32_t desc_total = ((gige_get_user_register(device, gendc_desc_len) + pkt_size - 1) / pkt_size) * (gige_get_user_register(device, gendc_pktsize) & 0xFFFF);

    if (gendc_en)
        return desc_total + (PART_NUM * part_len);
    else
        return part_total;
}
