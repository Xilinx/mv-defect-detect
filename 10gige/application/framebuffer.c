/******************************************************************************/
/*  GigE Vision Device Firmware                                               */
/*  Copyright (c) 2022. Sensor to Image GmbH. All rights reserved.            */
/*----------------------------------------------------------------------------*/
/*    File :  framebuf.c                                                      */
/*    Date :  2021-10-20                                                      */
/*     Rev :  1.6                                                             */
/*  Author :  LP                                                              */
/*----------------------------------------------------------------------------*/
/*  Template of the AXI framebuffer control functions to be used in a device  */
/*  user firmware                                                             */
/*----------------------------------------------------------------------------*/
/*  1.0  |  2020-04-29  |  JP  |  Initial version based on a reference design */
/*  1.1  |  2020-05-20  |  JP  |  Calculation of current SCPS increment       */
/*  1.2  |  2020-06-05  |  JP  |  Portable for 32b/64b platforms              */
/*  1.3  |  2021-02-16  |  JP  |  Wide AXI address support                    */
/*  1.4  |  2021-03-08  |  JP  |  Updated static buffer address allocation,   */
/*       |              |      |  compatible with 64b address on 32b platform */
/*  1.5  |  2021-03-09  |  MAS |  Extended Chunk mode support added           */
/*  1.6  |  2021-10-20  |  RW  |  Ported to linux                             */
/******************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <inttypes.h>

#include "libGigE.h"
#include "user.h"
#include "framebuffer.h"
#include "gendc.h"

uint16_t framebuf_pad_x;    // Line padding
uint16_t framebuf_pad_y;    // Block (frame) padding
uint64_t framebuf_bpb;      // Total bytes per block

// ---- Initialization of the framebuffer --------------------------------------
//
//               Initialization of the framebuffer. The function returns pointer
//               to the memory allocated for the framebuffer.
//
// low_latency = enable the low-latency readout mode
// deinterlace = perform de-interlacing of the interlaced video
// gev1mode    = operate in GEV 1.x compatibility mode
//               0: GEV 2.x (64b block id, extended GVSP status codes)
//               1: GEV 1.x (16b block id, no extended GVSP status codes)
// size        = number of bytes to allocate
//
// memory for frmaebuffer is set in uboot (add boot arg mem=960M)
//
uint16_t init_famebuffer(uint8_t device, int low_latency, int deinterlace, int gev1mode, uint32_t size)
{
    uint32_t lframebuf_version, lframebuf_date, temp;
    uint64_t fb_top;

    // Framebuffer core check
    if(gige_get_framebuffer_register(device, framebuf_id) != FPGA_ID)
        return 1;

    // set framebuffer
    gige_set_framebuffer_register(device, framebuf_bot_l, MEM_IMAGEBUFFER_ADDR & 0xFFFFFFFF);
    gige_set_framebuffer_register(device, framebuf_bot_h, (MEM_IMAGEBUFFER_ADDR>>32) & 0xFFFFFFFF);
    fb_top = MEM_IMAGEBUFFER_ADDR + MEM_IMAGEBUFFER_SIZE - 1;
    gige_set_framebuffer_register(device, framebuf_top_l, fb_top & 0xFFFFFFFF);
    gige_set_framebuffer_register(device, framebuf_top_h, (fb_top>>32) & 0xFFFFFFFF);

    gige_set_framebuffer_register(device, framebuf_control, FRAMEBUF_C_LEADTS | FRAMEBUF_C_INIT | FRAMEBUF_C_CLRSTAT |
                                 (low_latency ? RDMODE_LOW_LAT : 0) |
                                 (deinterlace ? FRAMEBUF_C_DEINT  : 0) |
                                 (gev1mode    ? FRAMEBUF_C_BID16B  : 0) |
                                 (!gev1mode   ? FRAMEBUF_C_EXTSTAT : 0));

    gige_set_user_register(device, framebuf_pld_type, PLD_IMAGE);
    gige_set_framebuffer_register(device, framebuf_lead_offs, 0x0000);
    gige_set_framebuffer_register(device, framebuf_trail_offs, 0x0400);

    // Enable dynamic trailers
    // temp = gige_get_framebuffer_register(framebuf_control);
    // temp |= FRAMEBUF_C_DYNTRAIL;
    // gige_set_framebuffer_register(framebuf_control, temp);

    lframebuf_version = gige_get_framebuffer_register(device, framebuf_version);
    lframebuf_date = gige_get_framebuffer_register(device, framebuf_date);

    // Print IP core information
    printf("[UU] Framebuffer, v%d.%d.%d (%04X-%02X-%02X)\r\n",
                lframebuf_version >> 24, (lframebuf_version >> 16) & 0xFF, (lframebuf_version & 0xFFFF),
                lframebuf_date >> 16, (lframebuf_date >> 8) & 0xFF, lframebuf_date & 0xFF);
    printf("     0x%08X bytes starting at 0x%08X\r\n", MEM_IMAGEBUFFER_SIZE, (uint32_t)MEM_IMAGEBUFFER_ADDR);

    return 0;
}

uint16_t close_famebuffer(uint8_t device)
{
    return 0;
}

uint16_t reset_framebuffer(uint8_t device)
{
    // framebuffer reset
    gige_set_framebuffer_register(device, framebuf_control, gige_get_framebuffer_register(device, framebuf_control) | FRAMEBUF_C_INIT);
    return 0;
}


// ---- Calculation of frame padding and bytes per frame -----------------------
//
//         This function updates current line/frame padding and total bytes
//         per frame according to the video frame width and height
//
// bpp   = bytes per pixel
// deint = framebuffer is performing deinterlacing
// burst = framebuffer burst length in bytes
// size_chunk   = size of chunk data
//
uint16_t framebuf_padding(uint8_t device, uint32_t pixel_format, uint32_t size_x, uint32_t size_y)
{
    uint32_t burst, bpp, temp;

    // Burst length in bytes and bytes per pixel
    burst = (((gige_get_framebuffer_register(device, framebuf_status) & FRAMEBUF_S_BURST) >> 8) + 1) *
            (1 << (gige_get_framebuffer_register(device, framebuf_status) & FRAMEBUF_S_BITS_AXI));
    bpp   = (pixel_format & 0x00FF0000) >> 16;

    // Padding
    if (gige_get_framebuffer_register(device, framebuf_control) & FRAMEBUF_C_DEINT)  // Interlaced scan
    {
        temp = burst - (((size_x * bpp)/8) % burst);
        if (temp >= burst)
            temp = 0;
        framebuf_pad_x = temp;
        framebuf_pad_y = 0;
    }
    else                                        // Progressive scan
    {
        temp = burst - (((size_x * size_y * bpp)/8) % burst);
        if (temp >= burst)
            temp = 0;
        framebuf_pad_x = 0;
        framebuf_pad_y = temp;
    }

    return 0;
}

// ---- Get currently supported SCPS increment ---------------------------------
//
uint32_t framebuf_scps_inc(uint8_t device)
{
    uint32_t dw = 1 << ( gige_get_framebuffer_register(device, framebuf_status) & FRAMEBUF_S_BITS_AXI);
    uint32_t ow = 1 << ((gige_get_framebuffer_register(device, framebuf_status) & FRAMEBUF_S_BITS_OUT) >> 24);

    return (dw >= ow ? dw : ow);
}

// ---- Initialize image leader packet -----------------------------------------
//
void framebuf_img_leader(uint8_t device, uint32_t pixel_format, uint32_t size_x, uint32_t size_y, uint32_t offset_x, uint32_t offset_y,
                         uint32_t pad_x, uint32_t pad_y, uint32_t gendc_en, uint32_t gendc_dlen, uint32_t bpp)
{
    // Calculate padding and bytes per block
    // ... use precalculated globals instead!
    //framebuf_padding(pixel_format, size_x, size_y);
    uint32_t len_part = ((((size_x * bpp) / 8) + pad_x) * size_y) + pad_y;
    uint32_t pad_img = gendc_part_padding(device, size_x, size_y, (pixel_format & 0x00FF0000) >> 16, 0);
    uint32_t offset = gige_get_framebuffer_register(device, framebuf_lead_offs);

    if (!gendc_en)
    {
        // Load leader packet DPRAM
        gige_set_framebuffer_register(device, framebuf_pkt_dpram + (offset + 0), pixel_format);
        gige_set_framebuffer_register(device, framebuf_pkt_dpram + (offset + 4), size_x);
        gige_set_framebuffer_register(device, framebuf_pkt_dpram + (offset + 8), size_y);
        gige_set_framebuffer_register(device, framebuf_pkt_dpram + (offset + 12), offset_x);
        gige_set_framebuffer_register(device, framebuf_pkt_dpram + (offset + 16), offset_y);
        gige_set_framebuffer_register(device, framebuf_pkt_dpram + (offset + 20), ((pad_x & 0xFFFF) << 16) |
                                                                                  ((pad_y + pad_img) & 0xFFFF));
        // Total payload length is needed for U3V core only!
        // gige_set_framebuffer_register(framebuf_pkt_dpram + (offset + 24), 0);
        // gige_set_framebuffer_register(framebuf_pkt_dpram + (offset + 28),framebuf_bpb);

        // Leader packet payload length
        gige_set_framebuffer_register(device, framebuf_lead_len, 24);
    }
    else
    {
        // Load leader packet DPRAM
        gige_set_framebuffer_register(device, framebuf_pkt_dpram + (offset + 0), 0);
        gige_set_framebuffer_register(device, framebuf_pkt_dpram + (offset + 4), gendc_dlen + (PART_NUM * len_part));
        gige_set_framebuffer_register(device, framebuf_pkt_dpram + (offset + 8), 0);
        gige_set_framebuffer_register(device, framebuf_pkt_dpram + (offset + 12), gendc_dlen);

        // Leader packet payload length
        gige_set_framebuffer_register(device, framebuf_lead_len, 16);
    }
}

// ---- Set maximum block length -----------------------------------------------
//
// NOTE: GenDC packets contain additional 16 bytes of GenDC 'headers'
//       It is taken into account in the gendc_fb_len()
//
void framebuf_update(uint8_t device, uint32_t size_x, uint32_t size_y, uint32_t pad_x, uint32_t pad_y, uint32_t bpp, uint32_t gendc_en)
{
    uint32_t burst = (((gige_get_framebuffer_register(device, framebuf_status) & FRAMEBUF_S_BURST) >> 8) + 1) *
                     (1 << (gige_get_framebuffer_register(device, framebuf_status) & FRAMEBUF_S_BITS_AXI));

    framebuf_bpb = gendc_fb_len(device, size_x, size_y, pad_x, pad_y, bpp, gendc_en);
    // Safe maximum block length (total bytes per block + 1 burst)
    gige_set_framebuffer_register(device, framebuf_blk_max_h, (uint32_t)((framebuf_bpb + (uint64_t)burst) >> 32));
    gige_set_framebuffer_register(device, framebuf_blk_max_l, (uint32_t)((framebuf_bpb + (uint64_t)burst) & 0xFFFFFFFF));
}

// ---- Initialize image trailer packet ----------------------------------------
//
void framebuf_img_trailer(uint8_t device, uint32_t size_y, uint32_t gendc_en)
{
    uint32_t offset = gige_get_framebuffer_register(device, framebuf_trail_offs);

    // Image payload type
    if (!gendc_en)
    {
        // Load trailer packet DPRAM (not needed if dynamic trailers are activated)
        gige_set_framebuffer_register(device, framebuf_pkt_dpram + (offset + 0), size_y);
        // Trailer packet payload length (also needed if dynamic trailers are activated!)
        gige_set_framebuffer_register(device, framebuf_trail_len, 4);
    }
    else
    {
        gige_set_framebuffer_register(device, framebuf_trail_len, 0);
    }
}

// ---- List contents of the registers -----------------------------------------
//
void framebuf_printregs(uint8_t device)
{
    uint32_t i;
    uint32_t lframebuf_lead_len, lframebuf_lead_offs;
    uint32_t lframebuf_trail_len, lframebuf_trail_offs;

    printf("ID registers:\r\n");
    printf("  Core ID               = 0x%08X\r\n", gige_get_framebuffer_register(device, framebuf_id));
    printf("  Version               = %d.%d.%d\r\n", gige_get_framebuffer_register(device, framebuf_version) >> 24, (gige_get_framebuffer_register(device, framebuf_version) >> 16) & 0xFF, gige_get_framebuffer_register(device, framebuf_version) & 0xFFFF);
    printf("  Build date            = %04X-%02X-%02X\r\n", gige_get_framebuffer_register(device, framebuf_date) >> 16, (gige_get_framebuffer_register(device, framebuf_date) >> 8) & 0xFF, gige_get_framebuffer_register(device, framebuf_date) & 0xFF);
    printf("Control registers:\r\n");
    printf("  Control               = 0x%08X\r\n", gige_get_framebuffer_register(device, framebuf_control));
    printf("  Framebuffer bottom    = 0x%08X\r\n", gige_get_framebuffer_register(device, framebuf_bot));
    printf("  Framebuffer top + 1   = 0x%08X\r\n", gige_get_framebuffer_register(device, framebuf_top));
    printf("  Maximum block length  = 0x%08X = %d\r\n", gige_get_framebuffer_register(device, framebuf_blk_max), gige_get_framebuffer_register(device, framebuf_blk_max));
    printf("  Leader DPRAM offset   = 0x%04X\r\n", gige_get_framebuffer_register(device, framebuf_lead_offs));
    printf("  Leader length         = %d\r\n", gige_get_framebuffer_register(device, framebuf_lead_len));
    printf("  Trailer DPRAM offset  = 0x%04X\r\n", gige_get_framebuffer_register(device, framebuf_trail_offs));
    printf("  Trailer length        = %d\r\n", gige_get_framebuffer_register(device, framebuf_trail_len));
    printf("Status registers:\r\n");
    printf("  Status                = 0x%08X\r\n", gige_get_framebuffer_register(device, framebuf_status));
    printf("                          %u bits input + AXI data width\r\n", 8 * (1 << (gige_get_framebuffer_register(device, framebuf_status) & FRAMEBUF_S_BITS_AXI)));
    printf("                          %u bits output data width\r\n", 8 * (1 << ((gige_get_framebuffer_register(device, framebuf_status) & FRAMEBUF_S_BITS_OUT) >> 24)));
    printf("                          %u words bursts\r\n", 1 + ((gige_get_framebuffer_register(device, framebuf_status) & FRAMEBUF_S_BURST) >> 8));

    if (gige_get_framebuffer_register(device, framebuf_status) & FRAMEBUF_S_DYNTRAIL)
        printf("                          dynamic and external trailers supported\r\n");
    if (gige_get_framebuffer_register(device, framebuf_status) & FRAMEBUF_S_DF_OVFLW)
        printf("                          descriptor FIFO overflowed\r\n");
    if (gige_get_framebuffer_register(device, framebuf_status) & FRAMEBUF_S_RF_OVFLW)
        printf("                          resend FIFO overflowed\r\n");
    if (gige_get_framebuffer_register(device, framebuf_status) & FRAMEBUF_S_IF_OVFLW)
        printf("                          input FIFO overflowed\r\n");
    if (gige_get_framebuffer_register(device, framebuf_status) & FRAMEBUF_S_IF_EMPTY)
        printf("                          input FIFO empty\r\n");
    if (gige_get_framebuffer_register(device, framebuf_status) & FRAMEBUF_S_TF_OVFLW)
        printf("                          trailer FIFO overflowed\r\n");
    if (gige_get_framebuffer_register(device, framebuf_status) & FRAMEBUF_S_DF_FULL)
        printf("                          descriptor FIFO is full\r\n");
	if (gige_get_framebuffer_register(device, framebuf_status) & FRAMEBUF_S_WR_ACT)
        printf("                          memory write active\r\n");
    if (gige_get_framebuffer_register(device, framebuf_status) & FRAMEBUF_S_RD_ACT)
        printf("                          memory read active\r\n");

    printf("  Descr. FIFO writes    = %d\r\n", gige_get_framebuffer_register(device, framebuf_desc_wr));
    printf("  Descr. FIFO reads     = %d\r\n", gige_get_framebuffer_register(device, framebuf_desc_rd));
    printf("  Descr. FIFO drops     = %d\r\n", gige_get_framebuffer_register(device, framebuf_desc_drop));
    printf("  Write dropped blocks  = %d\r\n", gige_get_framebuffer_register(device, framebuf_wr_drop));
    printf("  Write no space in FB  = %d\r\n", gige_get_framebuffer_register(device, framebuf_wr_nosp));
    printf("  Write desc. FIFO full = %d\r\n", gige_get_framebuffer_register(device, framebuf_wr_fifo_f));
    printf("  Read skipped          = %d\r\n", gige_get_framebuffer_register(device, framebuf_rd_skip));
    printf("  Read canceled         = %d\r\n", gige_get_framebuffer_register(device, framebuf_rd_cancel));
    printf("  Read sent             = %d\r\n", gige_get_framebuffer_register(device, framebuf_rd_sent));
    printf("  Resend FIFO writes    = %d\r\n", gige_get_framebuffer_register(device, framebuf_rsnd_wr));
    printf("  Resend FIFO reads     = %d\r\n", gige_get_framebuffer_register(device, framebuf_rsnd_rd));
    printf("  Resend FIFO drops     = %d\r\n", gige_get_framebuffer_register(device, framebuf_rsnd_drop));
    printf("  Resend OK             = %d\r\n", gige_get_framebuffer_register(device, framebuf_rsnd_ok));
    printf("  Resend N/A            = %d\r\n", gige_get_framebuffer_register(device, framebuf_rsnd_na));
    printf("Pointers:\r\n");
    printf("  Write bottom          = 0x%08X\r\n", gige_get_framebuffer_register(device, framebuf_p_wr_bot));
    printf("  Write top             = 0x%08X\r\n", gige_get_framebuffer_register(device, framebuf_p_wr_top));
    printf("  Read bottom           = 0x%08X\r\n", gige_get_framebuffer_register(device, framebuf_p_rd_bot));
    printf("  Resend bottom         = 0x%08X\r\n", gige_get_framebuffer_register(device, framebuf_p_rs_bot));
    printf("Descriptor registers:\r\n");
    printf("  Block start address   = 0x%08X\r\n", gige_get_framebuffer_register(device, framebuf_d_start));
    printf("  Block length          = 0x%08X = %d\r\n", gige_get_framebuffer_register(device, framebuf_d_len), gige_get_framebuffer_register(device, framebuf_d_len));
    printf("  Block timestamp       = 0x%08X%08X\r\n", gige_get_framebuffer_register(device, framebuf_d_ts_h), gige_get_framebuffer_register(device, framebuf_d_ts_l));
    printf("Interrupt registers:\r\n");
    printf("  Interrupt mask        = 0x%08X\r\n", gige_get_framebuffer_register(device, framebuf_int_mask));
    printf("  Interrupt request     = 0x%08X\r\n", gige_get_framebuffer_register(device, framebuf_int_req));
    printf("Leader DPRAM:");

    lframebuf_lead_len = (gige_get_framebuffer_register(device, framebuf_lead_len) + 3) / 4;
    lframebuf_lead_offs = gige_get_framebuffer_register(device, framebuf_lead_offs);

    lframebuf_trail_len = (gige_get_framebuffer_register(device, framebuf_trail_len) + 3) / 4;
    lframebuf_trail_offs = gige_get_framebuffer_register(device, framebuf_trail_offs);

    for (i = 0; i < lframebuf_lead_len; i++)
    {
        if ((i % 8) == 0)
            printf("\r\n  ");
        printf("%08X ", gige_get_framebuffer_register(device, framebuf_pkt_dpram + (lframebuf_lead_offs + (i*4))));
    }
    printf("\r\nTrailer DPRAM:");
    for (i = 0; i < lframebuf_trail_len; i++)
    {
        if ((i % 8) == 0)
            printf("\r\n  ");
        printf("%08X ", gige_get_framebuffer_register(device, framebuf_pkt_dpram + (lframebuf_trail_offs + (i*4))));
    }
    printf("\r\n");
}
