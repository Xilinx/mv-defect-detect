/******************************************************************************/
/*  GigE Vision Core Firmware                                                 */
/*  Copyright (c) 2022. Sensor to Image GmbH. All rights reserved.            */
/*----------------------------------------------------------------------------*/
/*    File :  gendc.h                                                         */
/*    Date :  2022-03-09                                                      */
/*     Rev :  2.1                                                             */
/*  Author :  JP                                                              */
/*----------------------------------------------------------------------------*/
/*  GigE Vision reference design GenDC front-end IP core include file         */
/*----------------------------------------------------------------------------*/
/*  1.0  |  2019-09-18  |  JP  |  Initial release                             */
/*  1.1  |  2019-10-30  |  JP  |  New test pattern generator                  */
/*  2.0  |  2022-03-04  |  SS  |  Ported for Linux application                */
/*  2.1  |  2022-03-09  |  SS  |  Prepared for kr260                          */
/******************************************************************************/

#ifndef _GENDC_H_
#define _GENDC_H_


// ---- Number of Parts --------------------------------------------------------
#define PART_NUM                1

// ---- GenDC register map -----------------------------------------------------

// Base address
#define GENDC_REGS              0x8000

// Common status registers
#define gendc_id                (GENDC_REGS + 0x0000)
#define gendc_version           (GENDC_REGS + 0x0004)
#define gendc_date              (GENDC_REGS + 0x0008)
#define gendc_config            (GENDC_REGS + 0x000C)
#define gendc_pktsize           (GENDC_REGS + 0x0010)

// Common control registers
#define gendc_control           (GENDC_REGS + 0x0020)
#define gendc_desc_flags        (GENDC_REGS + 0x0024)
#define gendc_desc_offs_h       (GENDC_REGS + 0x0028)
#define gendc_desc_offs_l       (GENDC_REGS + 0x002C)
#define gendc_desc_len          (GENDC_REGS + 0x0030)
#define gendc_int_mask          (GENDC_REGS + 0x0034)
#define gendc_int_req           (GENDC_REGS + 0x0038)
#define gendc_ts_gen            (GENDC_REGS + 0x003C)

// Part status registers
#define gendc_part_status(n)    (GENDC_REGS + ((uint32_t)(n) * 0x0100) + 0x0050)
#define gendc_part_ts_h(n)      (GENDC_REGS + ((uint32_t)(n) * 0x0100) + 0x0054)
#define gendc_part_ts_l(n)      (GENDC_REGS + ((uint32_t)(n) * 0x0100) + 0x0058)
#define gendc_part_bytes_h(n)   (GENDC_REGS + ((uint32_t)(n) * 0x0100) + 0x005C)
#define gendc_part_bytes_l(n)   (GENDC_REGS + ((uint32_t)(n) * 0x0100) + 0x0060)
#define gendc_part_lines(n)     (GENDC_REGS + ((uint32_t)(n) * 0x0100) + 0x0064)
#define gendc_part_frames(n)    (GENDC_REGS + ((uint32_t)(n) * 0x0100) + 0x0068)
#define gendc_part_config(n)    (GENDC_REGS + ((uint32_t)(n) * 0x0100) + 0x006C)

// Part control registers
#define gendc_part_control(n)   (GENDC_REGS + ((uint32_t)(n) * 0x0100) + 0x0080)
#define gendc_part_flags(n)     (GENDC_REGS + ((uint32_t)(n) * 0x0100) + 0x0084)
#define gendc_part_offs_h(n)    (GENDC_REGS + ((uint32_t)(n) * 0x0100) + 0x0088)
#define gendc_part_offs_l(n)    (GENDC_REGS + ((uint32_t)(n) * 0x0100) + 0x008C)

// Descriptor DPRAM
#define gendc_descriptor        (GENDC_REGS + 0x1000)


// ---- GenDC constants and macros ---------------------------------------------

// Global control bits
#define GENDC_CONTROL_ENABLE                0x00000001
#define GENDC_CONTROL_DESCR_EN              0x00000002
#define GENDC_CONTROL_PRELIM_EN             0x00000004
#define GENDC_CONTROL_UPDATE_ID             0x00000008
#define GENDC_CONTROL_ID_ADDR               0x0FFF0000

// Interrupt registers bits
#define GENDC_INT_OVERFLOW                  0x00000001
#define GENDC_INT_GLOBAL                    0x80000000

// Timestamp generator register bits
#define GENDC_TS_GEN_INC                    0x000000FF
#define GENDC_TS_GEN_STEP                   0x000FF000
#define GENDC_TS_GEN_CLEAR                  0x80000000

// Part status bits
#define GENDC_PART_STATUS_OVERFLOW          0x00000001
#define GENDC_PART_STATUS_LATCHED_OVERFLOW  0x00000002

// Part control bits
#define GENDC_PART_CONTROL_ENABLE           0x00000001
#define GENDC_PART_CONTROL_GENDC_EN         0x00000002
#define GENDC_PART_CONTROL_UPDATE_TS        0x00000004
#define GENDC_PART_CONTROL_TS_ADDR          0x0FFF0000
#define GENDC_PART_CONTROL_CLEAR_STATUS     0x80000000

// Reorder bytes of 32b word
#define swap_endianness(x)  ((((x) & 0xFF000000) >> 24) | (((x) & 0x00FF0000) >> 8) | (((x) & 0x0000FF00) << 8) | (((x) & 0x000000FF) << 24))


// -- GenDC global variables ---------------------------------------------------

// Software-only registers
extern uint32_t  gendc_flow_table_len;
extern uint32_t *gendc_flow_table;


// ---- Function prototypes ----------------------------------------------------

// GenDC functions
void gendc_info(uint8_t device);
void gendc_init(uint8_t device, uint32_t size_x, uint32_t size_y, uint32_t offs_x, uint32_t offs_y, uint32_t pad_x, uint32_t pad_y, uint32_t bpp, uint32_t pixel_fmt);
void gendc_update(uint8_t device, uint32_t size_x,  uint32_t size_y, uint32_t pad_x, uint32_t pad_y, uint32_t bpp, uint32_t gendc_en);
void gendc_update_descriptor(uint8_t device, uint32_t size_x, uint32_t size_y, uint32_t offs_x, uint32_t offs_y, uint32_t pad_x, uint32_t pad_y, uint32_t bpp, uint32_t pixel_fmt);
void gendc_update_flow_table(uint8_t device, uint32_t size_x, uint32_t size_y, uint32_t pad_x, uint32_t pad_y, uint32_t bpp);
uint32_t gendc_part_padding(uint8_t device, uint32_t size_x, uint32_t size_y, uint32_t bpp, uint32_t gendc_en);
uint32_t gendc_fb_len(uint8_t device, uint32_t size_x, uint32_t size_y, uint32_t pad_x, uint32_t pad_y, uint32_t bpp, uint32_t gendc_en);

#endif
