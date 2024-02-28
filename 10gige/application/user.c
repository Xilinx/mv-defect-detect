/******************************************************************************/
/*  GigE Vision Core Firmware                                                 */
/*  Copyright (c) 2022. Sensor to Image GmbH. All rights reserved.            */
/*----------------------------------------------------------------------------*/
/*    File :  user.c                                                          */
/*    Date :  2022-03-09                                                      */
/*     Rev :  0.11                                                            */
/*  Author :  JP                                                              */
/*----------------------------------------------------------------------------*/
/*  GigE Vision reference design application-specific functions               */
/*----------------------------------------------------------------------------*/
/*  0.1  |  2008-03-12  |  JP  |  Initial release                             */
/*  0.2  |  2011-10-13  |  JP  |  Acquisition mode made R/W                   */
/*  0.3  |  2013-01-07  |  JP  |  Networking functions moved to new file      */
/*  0.4  |  2013-02-26  |  JP  |  Fixed the action_trigger() function         */
/*  0.5  |  2016-03-18  |  JP  |  Added fan control PWM to user_init()        */
/*  0.6  |  2016-09-21  |  JP  |  New generic libgige user callback function  */
/*  0.7  |  2018-07-20  |  JP  |  Extended chunk mode support                 */
/*  0.8  |  2020-10-19  |  PD  |  Add TestEventGenerate and EventTestTimestamp*/
/*                             |  libgige update -> gige_send_message update  */
/*  0.9  |  2021-09-14  |  SS  |  Use new videotpg and update libgege         */
/*  0.10 |  2021-10-20  |  RW  |  Ported to linux                             */
/*  0.11 |  2022-03-09  |  SS  |  Prepared for kr260                          */
/******************************************************************************/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdint.h>

#include "libGigE.h"
#include "user.h"
#include "flash.h"
#include "framebuffer.h"
#include "gendc.h"

// ---- Global variables ----------------------------------------------
u32 video_width;
u32 video_height;
u32 video_offs_x;
u32 video_offs_y;

u32 video_max_width;
u32 video_max_height;
u32 video_acq_mode;
u32 video_gendc_en;
u32 video_gendc_mode;

u64 event_test_timestamp = 0;

u32 video_tpg;

// --- Local variables ---------------------------------------------------------

u32 video_bpp;

// --- Local function prototypes -----------------------------------------------

static void  updatebpp(uint32_t pixfmt);


// ---- Read GigE Vision user-space bootstrap register -------------------------
//
//           This function must be always implemented!
//           It is called by the libGigE
//
// address = address of the register within GigE Vision manufacturer-specific
//           register address space (0x0000A000 - 0xFFFFFFFF)
// status  = return status value, should be one of following:
//           GEV_STATUS_SUCCESS         - read passed correctly
//           GEV_STATUS_INVALID_ADDRESS - register at 'address' does not exist
//           GEV_STATUS_LOCAL_PROBLEM   - problem while getting register value
//           GEV_STATUS_ERROR           - unspecified error
//
// return value = content of the register at 'address'
//
u32 get_user_reg(u8 fdevice, u32 address, u16 *status)
{
    //printf("get_reg: %x\n", address);
    float framerate;
    u32 *temp = NULL;
    u32 bpp;
    u32 tmp;

    *status = GEV_STATUS_SUCCESS;
    switch (address)
    {
        case 0x0000A000:    return gige_get_user_register(fdevice, gendc_control) & GENDC_CONTROL_ENABLE;
        case 0x0000A004:    return video_width;
        case 0x0000A008:    return video_height;
        case 0x0000A00C:    return fdevice, video_offs_x;
        case 0x0000A010:    return video_offs_y;
        case 0x0000A014:    bpp   = ((gige_get_user_register(fdevice, video_pixfmt) & 0x00FF0000) >> 16);
                            return (((video_width * bpp) /8) + framebuf_pad_x);
        case 0x0000A018:    return gige_get_user_register(fdevice, video_pixfmt);
        case 0x0000A01C:    return video_max_width;
        case 0x0000A020:    return video_max_height;
        //case 0x0000A024:    return framebuf_bpb;
        case 0x0000A028:    return video_acq_mode;
        case 0x0000A038:    return video_tpg;
        case 0x0000A0F0:    return video_gendc_mode;
        case 0x0000A0F4:    return (video_gendc_en ? 1 : 0);
        case 0x0000A10C:   // EventTestTimestamp high value
                            tmp = (u32)(event_test_timestamp << 32);
                            return(tmp);
        case 0x0000A110:    // EventTestTimestamp low value
                            tmp = (u32)(event_test_timestamp & 0xFFFFFFFF);
                            return(tmp);
        // Packet size margins
        case 0x10000000:    return SCPS_MIN + (gige_get_gev_version(fdevice) == 2 ? 48 : 36);
        case 0x10000004:    return SCPS_MAX + (gige_get_gev_version(fdevice) == 2 ? 48 : 36);
        case 0x10000008:    return SCPS_INC;
        // Authentication and evaluation status
        case 0x1000000C:    return (u32)gige_get_auth_status(fdevice);
        case 0x10000010:    return (u32)gige_get_license_checksum(fdevice);
        // Stream channel extended bootstrap registers (GEV 2.2)
        case MAP_SCEBA + 0x00:  return MAP_GENDC_DESCRIPTOR;
        case MAP_SCEBA + 0x04:  return gige_get_user_register(fdevice, gendc_desc_len);
        case MAP_SCEBA + 0x08:  return MAP_GENDC_FLOW_TABLE;
        case MAP_SCEBA + 0x0C:  return gige_get_user_register(fdevice, gendc_flow_table_len);
        // Special memory areas
        default:            // GenDC Descriptor
                            if ((address >= MAP_GENDC_DESCRIPTOR) && (address < (MAP_GENDC_DESCRIPTOR + gendc_desc_len)))
                            {
                                // Get map GenDC to register access (user space)
                                u32 gendc_offs = address - MAP_GENDC_DESCRIPTOR;
                                return  gige_get_user_register(fdevice, gendc_descriptor + gendc_offs);
                            }
                            // GenDC flow mapping table
                            if ((address >= MAP_GENDC_FLOW_TABLE) && (address < (MAP_GENDC_FLOW_TABLE + gendc_flow_table_len)))
                            {
                                return gendc_flow_table[(address - MAP_GENDC_FLOW_TABLE)/4];
                            }
                            // Configuration EEPROM
                            if ((address >= 0xFBFF0000) && (address < 0xFBFF2000))
                            {
                                gige_eeprom_read_dword(fdevice, (u16)(address - 0xFBFF0000), &tmp);
                                return(tmp);
                            }
                            // flash memory
                            if (address >= MAP_FLASH)
                                return flash_read_dword(address - MAP_FLASH);
                            break;
    }

    *status = GEV_STATUS_INVALID_ADDRESS;
    return 0;
}

// ---- Write GigE Vision user-space bootstrap register ------------------------
//
//           This function must be always implemented!
//           It is called by the libGigE
//
// address = address of the register within GigE Vision manufacturer-specific
//           register address space (0x0000A000 - 0xFFFFFFFF)
// value   = data to be written
// status  = return status value, should be one of following:
//           GEV_STATUS_SUCCESS         - write passed correctly
//           GEV_STATUS_INVALID_ADDRESS - register at 'address' does not exist
//           GEV_STATUS_WRITE_PROTECT   - register at 'address' is read-only
//           GEV_STATUS_LOCAL_PROBLEM   - problem while setting register value
//           GEV_STATUS_ERROR           - unspecified error
//
void set_user_reg(u8 fdevice, u32 address, u32 value, u16 *status)
{
    int update = 0;

    *status = GEV_STATUS_SUCCESS;

    switch (address)
    {
        case 0x0000A000:    gige_set_user_register(fdevice, gendc_control,
                                (gige_get_user_register(fdevice, gendc_control) & ~GENDC_CONTROL_ENABLE) |
                                (value & GENDC_CONTROL_ENABLE));
                            gige_set_acquisition_status(fdevice, 0, value & GENDC_CONTROL_ENABLE);
                            break;
        case 0x0000A004:    if ((value > 0) && (value <= video_max_width))
                            {
                                video_width = value;
                                update = UPDATE_LEADER | UPDATE_GENDC | UPDATE_FRAMEBUF |
                                         UPDATE_DESCRIPTOR | UPDATE_FLOW_TABLE;
                            }
                            else
                                *status = GEV_STATUS_INVALID_PARAMETER;
                            break;
        case 0x0000A008:    if ((value > 0) && (value <= video_max_height))
                            {
                                video_height = value;
                                update = UPDATE_LEADER | UPDATE_GENDC | UPDATE_TRAILER |
                                         UPDATE_DESCRIPTOR | UPDATE_FLOW_TABLE | UPDATE_FRAMEBUF;
                            }
                            else
                                *status = GEV_STATUS_INVALID_PARAMETER;
                            break;
        case 0x0000A00C:    video_offs_x = 0;
                            update = UPDATE_LEADER | UPDATE_DESCRIPTOR;
                            break;
        case 0x0000A010:    video_offs_y = value;
                            update = UPDATE_LEADER | UPDATE_DESCRIPTOR;
                            break;

        case 0x0000A014:    *status = GEV_STATUS_WRITE_PROTECT;
                            break;

        case 0x0000A018:    gige_set_user_register(fdevice, video_pixfmt, value);
                            updatebpp(value);
                            update = UPDATE_LEADER | UPDATE_TRAILER | UPDATE_DESCRIPTOR |
                                     UPDATE_GENDC | UPDATE_FLOW_TABLE | UPDATE_FRAMEBUF;
                            break;
        case 0x0000A01C:
        case 0x0000A020:    *status = GEV_STATUS_WRITE_PROTECT;
                            break;
        case 0x0000A028:    video_acq_mode = value;
                            break;
        case 0x0000A0F0:    if (video_gendc_en && (value >= 1) && (value <= 2))
                                video_gendc_mode = value;
                            break;
        case 0x0000A0F4:    *status = GEV_STATUS_WRITE_PROTECT;
                            break;
        case 0x0000A108:    // Test Event
                            gige_send_message(fdevice, 0x9001, 0, 0, NULL, (u64*)&event_test_timestamp);
                            break;
        case 0x0000A10C:    // EventTestTimestamp high
        case 0x0000A110:    // EventTestTimestamp low
                            *status = GEV_STATUS_WRITE_PROTECT;
                            break;
        // Packet size margins
        case 0x10000000:
        case 0x10000004:
        case 0x10000008:    *status = GEV_STATUS_WRITE_PROTECT;
                            break;
        // Stream channel extended bootstrap registers (GEV 2.2)
        case MAP_SCEBA + 0x00:
        case MAP_SCEBA + 0x04:
        case MAP_SCEBA + 0x08:
        case MAP_SCEBA + 0x0C:  *status = GEV_STATUS_WRITE_PROTECT;
                                break;
        // Special memory areas
        default:            // GenDC descriptor
                            if ((address >= MAP_GENDC_DESCRIPTOR) && (address < (MAP_GENDC_DESCRIPTOR + gendc_desc_len)))
                            {
                                *status = GEV_STATUS_WRITE_PROTECT;
                                break;
                            }
                            // GenDC flow mapping table
                            if ((address >= MAP_GENDC_FLOW_TABLE) && (address < (MAP_GENDC_FLOW_TABLE + gendc_flow_table_len)))
                            {
                                *status = GEV_STATUS_WRITE_PROTECT;
                                break;
                            }
                            // Configuration EEPROM
                            if ((address >= 0xFBFF0000) && (address < 0xFBFF2000))
                            {
                                gige_eeprom_write_dword(fdevice, (u16)(address - 0xFBFF0000), value);
                                break;
                            }
                            // flash memory
                            if (address >= 0xFE000000)
                            {
                                if (address < (0xFE000000 + SECTOR_SIZE_S2I))    	// SECTOR_SIZE_S2I kB write buffer
                                {
                                	flash_buffer[(address - 0xFE000000) / 4] = value;
                                }
                                else
                                {
                                    if (address == (0xFE000000 + SECTOR_SIZE_S2I))	// Write address
                                        flash_write_block(value, (u32 *)flash_buffer, SECTOR_SIZE_S2I);
                                    else                                        // Read-only space
                                        *status = GEV_STATUS_WRITE_PROTECT;
                                }
                                break;
                            }
                            // Undefined address space
                            *status = GEV_STATUS_INVALID_ADDRESS;
                            break;
    }

    // Adjust padding and total bytes per block
    framebuf_padding(fdevice, gige_get_user_register(fdevice, video_pixfmt), video_width, video_height);

    // Update leader/trailer
    if (update & UPDATE_LEADER)
        framebuf_img_leader(fdevice, gige_get_user_register(fdevice, video_pixfmt),
                            video_width, video_height, video_offs_x, video_offs_y,
                            framebuf_pad_x, framebuf_pad_y, video_gendc_en,
                            gige_get_user_register(fdevice, gendc_desc_len), video_bpp);

    if (update & UPDATE_TRAILER)
        framebuf_img_trailer(fdevice, video_height, video_gendc_en);

    // Update GenDC
    if (update & UPDATE_GENDC)
        gendc_update(fdevice, video_width, video_height, framebuf_pad_x, framebuf_pad_y, video_bpp, video_gendc_en);
    if (update & UPDATE_DESCRIPTOR)
        gendc_update_descriptor(fdevice, video_width, video_height, video_offs_x, video_offs_y, framebuf_pad_x,
                                framebuf_pad_y, video_bpp, gige_get_user_register(fdevice, video_pixfmt));
    if (update & UPDATE_FLOW_TABLE)
        gendc_update_flow_table(fdevice, video_width, video_height, framebuf_pad_x, framebuf_pad_y, video_bpp);
    if (update & UPDATE_FRAMEBUF)
        framebuf_update(fdevice, video_width, video_height, framebuf_pad_x, framebuf_pad_y, video_bpp, video_gendc_en);

    gige_set_scmbs(fdevice, 0, framebuf_bpb);
}

// ---- Process the action signals ---------------------------------------------
//
//   This function must be always implemented!
//   It is called by the gige_callback() from libgige
//   The function is not allowed to block execution!
//
//   sig = array of four 32b bit fields identifying particular triggered signals
//
void action_trigger(u8 device, u32 *sig)
{
    int i;

    for (i = 0; i < 128; i++)
    {
        if (sig[i / 32] & (0x80000000 >> (i % 32)))
            printf("ACTION_CMD: triggered signal %d\r\n", i);
    }
}

// ---- Initialize custom part of the device -----------------------------------
//
//   This function should initialize the rest of the device which is not under
//   control of the GIgE library
//
//   color = zero if assembled MT9V022 sensor is grayscale version, nonzero
//           for color (Bayer pattern) variant
//
void user_init(u8 fdevice)
{
    // Set the default video parameters
    video_width      = 2472;
    video_height     = 2128;
    video_offs_x     = 0;
    video_offs_y     = 0;
    gige_set_user_register(fdevice, video_pixfmt, GVSP_PIX_MONO8);
    video_max_width  = 2472;
    video_max_height = 2128;
    video_acq_mode   = ACQ_MODE_CONTINUOUS;
    updatebpp(gige_get_user_register(fdevice, video_pixfmt));
    video_gendc_en   = 0;
    video_gendc_mode = 0;

    // Setup GVSP leader and trailer packets
    framebuf_padding(fdevice, gige_get_user_register(fdevice, video_pixfmt),
                     video_width, video_height);
    framebuf_img_leader(fdevice, gige_get_user_register(fdevice, video_pixfmt),
                        video_width, video_height, video_offs_x, video_offs_y,
                        framebuf_pad_x, framebuf_pad_y, 0, 0, video_bpp);
    framebuf_img_trailer(fdevice, video_height, 0);
    framebuf_update(fdevice, video_width, video_height, framebuf_pad_x, framebuf_pad_y, video_bpp, video_gendc_en);

    // Setup GenDC
    gendc_init(fdevice, video_width, video_height, video_offs_x, video_offs_y, framebuf_pad_x, framebuf_pad_y,
               video_bpp, gige_get_user_register(fdevice, video_pixfmt));

    gige_set_scmbs(fdevice, 0, framebuf_bpb);
    gige_set_acquisition_status(fdevice, 0, 0);
}

// ---- User callback function -------------------------------------------------
//
//           This function must be always implemented!
//           It is called by the libGigE
//
//   This function should be used as an entry point for application-specific
//   code except interaction with a GigE Vision application which is handled
//   using the gige_get_user_reg(), gige_set_user_reg(), and gige_send_message()
//   functions defined in the user.c file
//
//   Example shows sending asynchronous messages to the GigE Vision application
//
void user_callback(u8 fdevice)
{
    static u32 acq_latch = 0;
    static u32 pktsize_latch = 0;

    // Send messages after start and stop of image acquisition
    if ((gige_get_user_register(fdevice, gendc_control) & GENDC_CONTROL_ENABLE) != (acq_latch & GENDC_CONTROL_ENABLE))
    {
        if (gige_get_user_register(fdevice, gendc_control) & GENDC_CONTROL_ENABLE)
        {
            gige_send_message(fdevice, GEV_EVENT_START_OF_TRANSFER, 0, 0, NULL, NULL);
        }
        else
        {
            gige_send_message(fdevice, GEV_EVENT_END_OF_TRANSFER, 0, 0, NULL, NULL);
        }
        acq_latch = gige_get_user_register(fdevice, gendc_control);
    }

    // Update padding, leader packet, GenDC descriptor, and GenDC flow mapping table on change of packet size
    if (pktsize_latch != (gige_get_user_register(fdevice, gendc_pktsize) & 0xFFFF))
    {
        gendc_update_descriptor(fdevice, video_width, video_height, video_offs_x, video_offs_y, framebuf_pad_x,
                                framebuf_pad_y, video_bpp, gige_get_user_register(fdevice, video_pixfmt));
        gendc_update_flow_table(fdevice, video_width, video_height, framebuf_pad_x, framebuf_pad_y, video_bpp);
        framebuf_update(fdevice, video_width, video_height, framebuf_pad_x, framebuf_pad_y, video_bpp, video_gendc_en);
        framebuf_img_leader(fdevice, gige_get_user_register(fdevice, video_pixfmt), video_width, video_height,
                            video_offs_x, video_offs_y, framebuf_pad_x, framebuf_pad_y,
                            video_gendc_en, gige_get_user_register(fdevice, gendc_desc_len), video_bpp);
        pktsize_latch = gige_get_user_register(fdevice, gendc_pktsize) & 0xFFFF;
    }
}

// ---- Generic libgige user callback ------------------------------------------
//
//          This function is called by the libgige library when some of the
//          predefined events occurs.
//
// id     = identifier of the event
// param  = 32b parameter of the event
// data   = various extended data can be passed to/from the function
//
// return value = event specific
//
u32 gige_event(u8 device, u32 id, u32 param, void *data)
{
    static u32 gendc_en_latch = 0;
    u32 ret = 0, val;

    switch (id)
    {
        // No event
        case LIB_EVENT_NONE:
            break;

        // Write access into the GVCP configuration register
        //      param = value written by a GEV application
        //      data  = unused
        case LIB_EVENT_GVCP_CONFIG_WRITE:

            val = gige_get_framebuffer_register(device, framebuf_control);

            if ((param & 4) || (gige_get_gev_version(device) == 2))   // ES bit set or GEV 2.x with extended IDs
                val |= FRAMEBUF_C_EXTSTAT;         // Enable extended GVSP status codes
            else
                val &= ~FRAMEBUF_C_EXTSTAT;        // Disable extended GVSP status codes

            gige_set_framebuffer_register(device, framebuf_control, val);

            break;

            // Open or close stream channel
            //      param = bit mask of open channels (channel 0 = bit 0, etc.)
            //      data  = unused
            case LIB_EVENT_STREAM_OPEN_CLOSE:

            if (param == 0)                                                                   // No more open stream channels
            {
                val = gige_get_framebuffer_register(device, framebuf_control);
                val |= FRAMEBUF_C_INIT;                                                         // Clear framebuffer
                gige_set_framebuffer_register(device, framebuf_control, val);
                val = gige_get_user_register(device, gendc_control);
                gige_set_user_register(device, gendc_control, val & ~GENDC_CONTROL_ENABLE);    // Reset video in mle
                gige_set_acquisition_status(device, 0, 0);                                      // Acquisition off
            }
            break;

        // Write access into the stream channel configuration register
        //      param = value written by a GEV application
        //      data  = pointer to the stream channel index (u32)
        case LIB_EVENT_SCCFG_WRITE:
            video_gendc_en = param & 0x80;
            if (video_gendc_en != gendc_en_latch)
            {
                if (video_gendc_en)
                {
                    video_gendc_mode  = 1;
                    gige_set_framebuffer_register(device, framebuf_pld_type, PLD_GENDC);
                }
                else
                {
                    video_gendc_mode  = 0;
                    gige_set_framebuffer_register(device, framebuf_pld_type, PLD_IMAGE);
                }
                gendc_update(device, video_width, video_height, framebuf_pad_x, framebuf_pad_y, video_bpp, video_gendc_en);
                gendc_update_descriptor(device, video_width, video_height, video_offs_x, video_offs_y, framebuf_pad_x,
                                        framebuf_pad_y, video_bpp, gige_get_user_register(device, video_pixfmt));
                gendc_update_flow_table(device, video_width, video_height, framebuf_pad_x, framebuf_pad_y, video_bpp);
                framebuf_update(device, video_width, video_height, framebuf_pad_x, framebuf_pad_y, video_bpp, video_gendc_en);
                framebuf_img_leader(device, gige_get_user_register(device, video_pixfmt), video_width, video_height,
                                    video_offs_x, video_offs_y, framebuf_pad_x, framebuf_pad_y,
                                    video_gendc_en, gige_get_user_register(device, gendc_desc_len), video_bpp);
                framebuf_img_trailer(device, video_height, video_gendc_en);
            }
            gendc_en_latch = video_gendc_en;
            break;

        // Application closed the control channel
        //      param = index of the (X)GigE core
        //      data  = unused
        case LIB_EVENT_APP_DISCONNECT:
            gendc_en_latch    = 0;                                                          // Disable GenDC mode
            video_gendc_en    = 0;
            video_gendc_mode  = 0;
            gige_set_user_register(device, video_pixfmt, GVSP_PIX_MONO8);
            video_bpp         = 8;
            gige_set_user_register(device, framebuf_pld_type, PLD_IMAGE);
            gendc_update(device, video_width, video_height, framebuf_pad_x, framebuf_pad_y, video_bpp, video_gendc_en);
            framebuf_update(device, video_width, video_height, framebuf_pad_x, framebuf_pad_y, video_bpp, video_gendc_en);
            framebuf_img_leader(device, gige_get_user_register(device, video_pixfmt), video_width, video_height,
                                    video_offs_x, video_offs_y, framebuf_pad_x, framebuf_pad_y,
                                    video_gendc_en, gige_get_user_register(device, gendc_desc_len), video_bpp);
            framebuf_img_trailer(device, video_height, video_gendc_en);
            break;

        // Physical link disconnected
        //      param = index of the (X)GigE core
        //      data  = unused
        case LIB_EVENT_LINK_DOWN:
            printf("[INFO] - Link down...\n");
            val = gige_get_user_register(device, gendc_control);
            gige_set_user_register(device, gendc_control, val & ~GENDC_CONTROL_ENABLE);
            gige_set_acquisition_status(device, 0, 0);                                      // Acquisition off
            break;

        // Write access to the physical link configuration register
        //      param = value written by a GEV application
        //      data  = unused
        case LIB_EVENT_LINK_CONFIG_WRITE:
            break;

        // A new trigger has been scheduled after reception of a valid scheduled action command.
        //      param = index of the action signal(0..127)
        //      data  =  pointer to a 64b unsigned integer with trigger time of the action
        case LIB_EVENT_SCHEDULED_ACTION:
            //{
            //    u64 *timestamp = (u64*)data;
            //    printf("Scheduled ACTION_CMD: signal %ld, timestamp %" PRIu64 "\r\n", param, *timestamp);
            //}
            break;

        // Undefined event
        default:
            break;
    }

    return ret;
}


// ---- Update bpp -----------------------------------
//
//   Update bpp depending on pixel format
//
static void updatebpp(uint32_t pixfmt)
{
    switch(pixfmt)
    {
    case GVSP_PIX_MONO8:
        video_bpp = 8;
        break;
    case GVSP_PIX_MONO10:
    case GVSP_PIX_MONO12:
    case GVSP_PIX_MONO16:
        video_bpp = 16;
        break;
    default:
        // assume 8 bit pixel for unsupported formats
        video_bpp = 8;
        break;
    }
}


// ---- Local function prototypes ----------------------------------------------

int map_mem(volatile u8** mem, size_t baddr, size_t size)
{
	int fd;

	fd = open("/dev/mem", O_RDWR);
	if (fd < 1)
		return fd;

	*mem = (u8 *)mmap(
		0,
		size,
		PROT_READ | PROT_WRITE,
		MAP_SHARED,
		fd,
		baddr
	);

	if (*mem == MAP_FAILED)
		return -1;

	return fd;
}

int unmap_mem(volatile u8* mem, int fd)
{
	munmap((void *)mem, sizeof(u8));
	close(fd);
	return 0;
}
