/******************************************************************************/
/*  GigE Vision Core Firmware                                                 */
/*----------------------------------------------------------------------------*/
/*    File :  flash.c                                                         */
/*    Date :  2020-11-18                                                      */
/*     Rev :  0.4                                                             */
/*  Author :  RW                                                              */
/*----------------------------------------------------------------------------*/
/*  GigE Vision reference design flash memory functions                       */
/*----------------------------------------------------------------------------*/
/*  0.1  |  2016-05-03  |  RW  |  Initial release                             */
/*  0.2  |  2016-12-01  |  RW  |  swap dword in read_xml function             */
/*  0.3  |  2016-12-14  |  RW  |  Added sector size defines                   */
/*  0.4  |  2020-11-18  |  RW  |  Check file handle when close                */
/******************************************************************************/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <dirent.h>
#include <mtd/mtd-user.h>
#include <mtd/mtd-abi.h>
#include <errno.h>
#include <arpa/inet.h>
#include <string.h>
#include <limits.h>

#include "libGigE.h"
#include "flash.h"

// ---- Global variables -------------------------------------------------------
//
u32 flash_buffer[SECTOR_SIZE_S2I];

FILE *bitstream_file = NULL;
u32 bitstream_size, bitstream_cksum;

FILE *app_file = NULL;
u32 app_size, app_cksum;

FILE *xml_file = NULL;
u32 xml_size, xml_cksum;

FILE *tmp_file = NULL;
static u8 write_flag = 0;

char apppath[PATH_MAX+1];

int make_file_path(char *filename)
{
    char *p;
    int length;

    length = readlink("/proc/self/exe", apppath, sizeof(apppath));

    /* Catch some errors: */
    if (length < 0)
    {
        fprintf(stderr, "Error resolving symlink /proc/self/exe.\n");
        return 2;
    }

    if (length >= PATH_MAX)
    {
        fprintf(stderr, "Path too long. Truncated.\n");
          return 1;
    }

    /* I don't know why, but the string this readlink() function
     * returns is appended with a '@'.
     */
    apppath[length] = '\0';       /* Strip '@' off the end. */

    p = strrchr(apppath, '/');
    if(p)
        *++p = '\0';

    strcat(apppath,filename);

    return 0;
}

int open_xml(void)
{
    if (make_file_path(XML_FILE))
        return 1;

    if ((xml_file = fopen(apppath,"rb" )) == NULL)
        return 2;

    read_alloc_table_index(0x00000018, &xml_size);
    read_alloc_table_index(0x0000001C, &xml_cksum);

    return(0);
}

void close_xml(void)
{
    if(xml_file)
        fclose(xml_file);
}

int read_xml(u32 addr, u32 *value)
{
    int  result,cnt;
    u32 tmp;

    if (addr > xml_size)
    {
        *value = 0;
        return 0;
    }

    if ((xml_size - addr) < 4)
        cnt = xml_size - addr;
    else
        cnt = 4;

    result = fseek( xml_file, addr, SEEK_SET);
    if (result)
        return 2;

  if(fread(&tmp, cnt,1,xml_file) == 0)
    return(3);

    *value = htonl(tmp);
    return(0);
}

int read_alloc_table_index(u32 addr, u32 *value)
{
    int  result;
    FILE *hfile;
    u32 tmp;

    if (addr > 0x0000002C)
    {
        *value = 0;
        return(0);
    }

    if (make_file_path(TABLE_FILE))
        return 4;

    if ((hfile = fopen(apppath,"rb" )) == NULL)
        return 1;

    result = fseek( hfile, addr, SEEK_SET);
    if (result)
        return 2;

    if (fread(&tmp, 4,1,hfile) == 0)
        return 3;

    fclose(hfile);

    *value = htonl(tmp);
    return 0;
}

int write_alloc_table_index(u32 addr, u32 value)
{
    int  result;
    FILE *hfile;
    u32 tmp;

    if (addr > 0x0000002C)
        return 0;

    if (make_file_path(TABLE_FILE))
        return 4;

    if ((hfile = fopen(apppath,"r+b" )) == NULL)
        return 1;

    result = fseek( hfile, addr, SEEK_SET);
    if (result)
        return 2;

    tmp = htonl(value);
    if (fwrite(&tmp, 4,1,hfile) == 0)
        return 3;

    fclose(hfile);

    return 0;
}

int open_application(void)
{
    if (make_file_path(APP_FILE))
        return 4;

    if ((app_file = fopen(apppath,"rb" )) == NULL)
    return 1;

    read_alloc_table_index(0x00000028, &app_size);
    read_alloc_table_index(0x0000002C, &app_cksum);

    return 0;
}

int close_application(void)
{
    if (app_file)
        fclose(app_file);
    return 0;
}

int read_application(u32 addr, u32 *value)
{
    int  result,cnt;
    u32 tmp;

    if (addr > app_size)
    {
        *value = 0;
        return 0;
    }

    if ((app_size - addr) < 4)
        cnt = app_size - addr;
    else
        cnt = 4;

    result = fseek( app_file, addr, SEEK_SET);
    if (result)
        return 2;

    if (fread(&tmp, cnt,1,app_file) == 0)
        return 3;

    *value = tmp;
    return 0;
}

int open_bitstream(void)
{
    if (make_file_path(BITSTREAM_FILE))
    return 4;

    if ((bitstream_file = fopen(apppath,"rb" )) == NULL)
        return 1;

    read_alloc_table_index(0x00000000, &bitstream_size);
    read_alloc_table_index(0x00000004, &bitstream_cksum);

    return 0;
}

int close_bitstream(void)
{
    if (bitstream_file)
        fclose(bitstream_file);
    return 0;
}

int read_bitstream(u32 addr, u32 *value)
{
    int  result,cnt;
    u32 tmp;

    if (addr > bitstream_size)
    {
        *value = 0;
        return 0;
    }

    if ((bitstream_size - addr) < 4)
        cnt = bitstream_size - addr;
    else
        cnt = 4;

    result = fseek( bitstream_file, addr, SEEK_SET);
    if (result)
        return 2;

    if (fread(&tmp, cnt,1,bitstream_file) == 0)
        return 3;

    *value = tmp;
    return 0;
}

void update_file(char *filename, int size)
{
    FILE *hin,*hout;
    int ret,cnt = 0;

    if ((hin = fopen("tmp","rb" )) == NULL)
        return;

    if(make_file_path(filename))
        return;

   if ((hout = fopen(apppath,"wb" )) == NULL)
         return;

    do
    {
        ret = fread((void *)flash_buffer, 1,0x20000,hin);
        if((cnt + ret) > size)
            ret = size - cnt;
         fwrite((void *)flash_buffer, 1,ret,hout);
        cnt += ret;
        if(cnt >= size)
            ret = 0;
    } while(ret);
    fclose(hin);
    fclose(hout);
}

// ---- Read double-word from flash memory ---------------------------------
//
// address      = address within the SPI flash memory address space
//
// return value = contents of four consecutive bytes starting at 'address'
//
u32 flash_read_dword(u32 address)
{
  u32 tmp = 0;

    // Bitstream
    if((address >= BITSTREAM_START) && (address < BITSTREAM_END))
    {
//	  read_bitstream(address, &tmp);
    }
    // Allocation Table
    else if((address >= ALLOCATION_TABLE_START) && (address < ALLOCATION_TABLE_END))
    {
      read_alloc_table_index(address - ALLOCATION_TABLE_START, &tmp);
    }
    // Xml File
    else if((address >= XML_FILE_START) && (address < XML_FILE_END))
    {
        read_xml(address - XML_FILE_START, &tmp);
    }
    // Application
    else if((address >= APPLICATION_START) && (address < APPLICATION_END))
    {
        read_application(address - APPLICATION_START, &tmp);
    }
    return(tmp);
}

// ---- Write data block into flash memory ---------------------------------
//
//           The function erases 128 kB memory block and writes new data into it
//           Maximum block length is 131072 bytes
//           The address is adjusted to start at 128 kB block boundary
//
// address = start address within the SPI flash memory
// *buffer = pointer to a dword data buffer
// length  = number of bytes to write
//
void flash_write_block(u32 address, u32 *buffer, u32 length)
{
    int i;

    // Bitstream
    if ((address >= BITSTREAM_START) && (address < BITSTREAM_END))
    {
    /*if(address == 0x00000000)
    {
        if ((tmp_file = fopen("tmp","wb" )) == NULL)
            return;
        write_flag = 1;
    }

    if(fwrite(buffer, length,1,tmp_file) == 0)
        return;
    */
    }
    // Allocation Table
    else if((address >= ALLOCATION_TABLE_START) && (address < ALLOCATION_TABLE_END))
    {
        // update allocation table first 12 entries
        for(i = 0;i < 12;i++)
            write_alloc_table_index(i * 4, buffer[i]);

        // bitstream
        if(write_flag == 1)
        {
            fclose(tmp_file);
            close_bitstream();
            printf("update bitstream\n");
            update_file(BITSTREAM_FILE, buffer[0]);
            unlink("tmp");
            open_bitstream();
        }

        // xml
        if(write_flag == 2)
        {
            fclose(tmp_file);
            close_xml();
            printf("update xml\n");
            update_file(XML_FILE, buffer[6]);
            unlink("tmp");
            open_xml();
        }
        // application
        if(write_flag == 3)
        {
            fclose(tmp_file);
            close_application();
            printf("update application\n");
            update_file(APP_FILE, buffer[10]);
            unlink("tmp");
            open_application();
        }

        write_flag = 0;
    }
    // Xml File
    else if((address >= XML_FILE_START) && (address < XML_FILE_END))
    {
        if(address == XML_FILE_START)
        {
            if ((tmp_file = fopen("tmp","wb" )) == NULL)
                return;
            write_flag = 2;
        }

        if(fwrite(buffer, length,1,tmp_file) == 0)
          return;
    }
    // Application
    else if((address >= APPLICATION_START) && (address < APPLICATION_END))
    {
        if(address == APPLICATION_START)
        {
            if ((tmp_file = fopen("tmp","wb" )) == NULL)
                return;
            write_flag = 3;
        }

        if(fwrite(buffer, length,1,tmp_file) == 0)
            return;
    }

    return;
}

void flash_init(void)
{

    //open_bitstream();
    open_xml();
    open_application();
}

void flash_close(void)
{
    //close_bitstream();
    close_xml();
    close_application();
}
