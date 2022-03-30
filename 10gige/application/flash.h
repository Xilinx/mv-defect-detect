/******************************************************************************/
/*  GigE Vision Core Firmware                                                 */
/*----------------------------------------------------------------------------*/
/*    File :  flash.h                                                         */
/*    Date :  2019-08-22                                                      */
/*     Rev :  0.3                                                             */
/*  Author :  RW                                                              */
/*----------------------------------------------------------------------------*/
/*  GigE Vision reference design flash memory include file                    */
/*----------------------------------------------------------------------------*/
/*  0.1  |  2016-05-03  |  RW  |  Initial release                             */
/*  0.2  |  2016-12-14  |  RW  |  Added sector size defines                   */
/*                                Change xml file name to gvrd-mvdk.xml       */
/*  0.3  |  2019-08-22  |  RW  |  Set new xml file name                       */
/******************************************************************************/
#ifndef __FLASH_H
#define __FLASH_H

#include "libGigE.h"

// ---- Global variables -------------------------------------------------------

#define BITSTREAM_FILE "/dev/flash/bitstream"
#define TABLE_FILE "alloc_table.bin"
#define XML_FILE "xgvrd-kr260.xml"
#define APP_FILE "gvrd"

#define BITSTREAM_START         0x00400000
#define BITSTREAM_END           0x00800000

#define ALLOCATION_TABLE_START  0x00E00000
#define ALLOCATION_TABLE_END    0x01000000

#define XML_FILE_START          0x00C00000
#define XML_FILE_END            0x00E00000

#define APPLICATION_START       0x00800000
#define APPLICATION_END         0x00C00000

#define SECTOR_SIZE_S2I         0x40000     // Flash sector size set in GEV Updater
#define SECTOR_SIZE             0x40000     // Flash sector size of equipped QSPI flash


extern u32 flash_buffer[SECTOR_SIZE_S2I];

// ---- Function prototypes ----------------------------------------------------
int make_file_path(char *filename);

int read_alloc_table_index(u32 addr, u32 *value);

u32  flash_read_dword(u32 address);
void flash_write_block(u32 address, u32 *buffer, u32 length);

void flash_init(void);
void flash_close(void);

#endif
