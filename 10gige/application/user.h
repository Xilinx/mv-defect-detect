/******************************************************************************/
/*  GigE Vision Core Firmware                                                 */
/*  Copyright (c) 2022. Sensor to Image GmbH. All rights reserved.            */
/*----------------------------------------------------------------------------*/
/*    File :  user.h                                                          */
/*    Date :  2022-03-09                                                      */
/*     Rev :  0.6                                                             */
/*  Author :  JP                                                              */
/*----------------------------------------------------------------------------*/
/*  GigE Vision reference design application-specific include file            */
/*----------------------------------------------------------------------------*/
/*  0.1  |  2008-03-12  |  JP  |  Initial release                             */
/*  0.2  |  2018-07-20  |  JP  |  Extended chunk mode support                 */
/*  0.3  |  2020-08-14  |  PD  |  Core and Library Update                     */
/*  0.4  |  2021-09-14  |  SS  |  Use new videotpg and update libgege         */
/*  0.5  |  2021-10-20  |  RW  |  Ported to linux                             */
/*  0.6  |  2022-03-09  |  SS  |  Prepared for kr260                          */
/******************************************************************************/

#ifndef _USER_H_
#define _USER_H_

// ---- Video processor interface ----------------------------------------------

// Video processor base address

// Stream channel packet size margins
// The values represent net GVSP payload length without IP/UDP/GVSP headers!
#define SCPS_MIN                512
#define SCPS_MAX                8192
#define SCPS_INC                8

// Special GEV memory areas address map
#define MAP_SCEBA               0x20000000
#define MAP_GENDC_DESCRIPTOR    0x20010000
#define MAP_GENDC_FLOW_TABLE    0x20020000
#define MAP_EEPROM              0xFBFF0000
#define MAP_EEPROM_LEN          0x00002000
#define MAP_FLASH               0xFE000000

// Bitmasks for update in get_user_reg
#define UPDATE_LEADER           0x00000001
#define UPDATE_TRAILER          0x00000002
#define UPDATE_GENDC            0x00000004
#define UPDATE_TPG              0x00000008
#define UPDATE_DESCRIPTOR       0x00000010
#define UPDATE_FLOW_TABLE       0x00000020
#define UPDATE_FRAMEBUF         0x00000040
#define UPDATE_ALL              0xFFFFFFFF

// Video registers
#define video_pixfmt        0x0018

// ---- Global variables -------------------------------------------------------

extern u32 video_gendc_en;
extern u32 video_gendc_mode;

// ---- Function prototypes ----------------------------------------------------

u32 get_user_reg(u8 device, u32 address, u16 *status);
void set_user_reg(u8 device, u32 address, u32 value, u16 *status);
void user_init(u8 device);
void user_callback(u8 device);

u32 gige_event(u8 device, u32 id, u32 param, void *data);

#endif
