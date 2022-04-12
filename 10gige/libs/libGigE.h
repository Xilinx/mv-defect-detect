/******************************************************************************/
/*  GigE Vision Core Firmware                                                 */
/*  Copyright (c) 2022. Sensor to Image GmbH. All rights reserved.            */
/*----------------------------------------------------------------------------*/
/*    File :  libGigE.h                                                       */
/*    Date :  2022-01-11                                                      */
/*     Rev :  1.91                                                            */
/*  Author :  RW                                                              */
/*----------------------------------------------------------------------------*/
/*  Definitions of hardware interface registers                               */
/*----------------------------------------------------------------------------*/
/*  0.1  |  2008-01-24  |  JP  |  Initial release                             */
/*  0.2  |  2009-04-20  |  JP  |  Modified to support Altera Nios II CPU      */
/*  0.3  |  2009-05-25  |  JP  |  Added valid bits of the gige_gcsr register  */
/*  0.4  |  2009-07-15  |  JP  |  Added receiver configuration registers      */
/*  0.5  |  2011-01-24  |  RW  |  Ported to linux                             */
/*  0.6  |  2011-01-28  |  RW  |  Added callback functions, user register r/w */
/*  0.7  |  2016-05-03  |  RW  |  Added zynq support                          */
/*  0.8  |  2017-01-10  |  RW  |  Added generic libgige user callback         */
/*  0.9  |  2017-03-08  |  RW  |  Added LIB_EVENT_SCCFG_WRITE define          */
/*  1.0  |  2017-05-16  |  RW  |  Added gige multipart register               */
/*  1.1  |  2017-07-04  |  RW  |  Added gige_set_data_rates function          */
/*  1.2  |  2018-04-23  |  RW  |  New event IDs for libgige events            */
/*  1.3  |  2018-05-14  |  RW  |  New public functions to set and get current */
/*       |              |      |  acquisition status                          */
/*  1.4  |  2019-05-10  |  RW  |  Added gige_send_pending_ack function        */
/*       |              |      |  Added gige_set_pending_ack_timeout and      */
/*       |              |      |  gige_get_pending_ack_timeout function       */
/*  1.41 |  2019-09-19  |  RW  |  Initial support of GEV 2.2 and GenDC        */
/*  1.42 |  2020-03-25  |  RW  |  New constants & functions for physical link */
/*       |              |      |  configuration and capability registers,     */
/*       |              |      |  new library event                           */
/*  1.5  |  2020-03-26  |  RW  |  Added authentication and licensing functions*/
/*       |              |      |  Added stream channel direction functions    */
/*       |              |      |  define bool only if not c++                 */
/*       |              |      |  Added extern "C" for c++                    */
/*       |              |      |  Multilib support                            */
/*  1.51 |  2020-09-25  |  RW  |  gige_send_message() returns timestamp       */
/*  1.52 |  2020-10-04  |  RW  |  New function gige_force_gev_version()       */
/*  1.6  |  2020-12-15  |  RW  |  Updated authentication functions            */
/*  1.7  |  2021-02-15  |  RW  |  Added action commad support                 */
/*  1.8  |  2021-06-01  |  RW  |  New library event for scheduled action      */
/*       |              |      |  Functions to set/get packet resend support  */
/*  1.9  |  2021-12-29  |  RW  |  New functions to set/get stream channel     */
/*       |              |      |  maximum block size gige_set/get_scmbs()     */
/*  1.91 |  2022-01-11  |  RW  |  Include stdint.h                            */ 
/******************************************************************************/
#ifndef _LIBGIGE_H_
#define _LIBGIGE_H_

# include <stdint.h>

typedef int64_t      s64;
typedef uint64_t     u64;
typedef uint32_t     u32;     // Also defined in xbasic_types.h
typedef uint16_t     u16;     // Also defined in xbasic_types.h
typedef uint8_t      u8;      // Also defined in xbasic_types.h
#ifndef __cplusplus
typedef uint8_t      bool;    // Also defined in xbasic_types.h
#endif

// user callback functions
typedef void ( *USER_CALLBACK_FUNC )(u8 device);
typedef u32 ( *USER_GET_CALLBACK_FUNC )(u8 device, u32 address, u16 *status);
typedef void ( *USER_SET_CALLBACK_FUNC )(u8 device, u32 address, u32 value, u16 *status);

// generic libgige user callback
typedef u32 ( *GIGE_EVENT_CALLBACK_FUNC )(u8 device, u32 id, u32 param, void *data);

// network config callback functions
typedef int ( *LOCAL_NET_CONFIG_CALLBACK_FUNC )(u8 device, u32 ip, u32 netmask, u32 gateway);
typedef int ( *SYSTEM_NET_CONFIG_CALLBACK_FUNC )(u8 device, u8 dhcp, u32 ip, u32 netmask, u32 gateway);
typedef int ( *LOCAL_LINK_NET_CONFIG_CALLBACK_FUNC )(u8 device);

// action user callback
typedef void ( *GIGE_ACTION_TRIGGER_CALLBACK_FUNC )(u8 device, u32 *sig);

// init parameters
typedef struct
{
  u8  manuf[32];
  u8  model[32];
  u8  dver[32];
  u8  minfo[48];
  u32 bus_clk_freq;
  u8 data_rate;
  u16 eth_mtu;
  u16 mtu_min;
  u16 mtu_inc;
  u8 verbosity;
  u32 sdram_base_addr;
  u32 sdram_high_addr;
  u8 i2c_eeprom;
  u32 eeprom_write_delay;
  u8 i2c_sensor;
  u32 sensor_write_delay;
  USER_CALLBACK_FUNC user_callback;
  USER_GET_CALLBACK_FUNC user_get_callback;
  USER_SET_CALLBACK_FUNC user_set_callback;
  u8 lib_mode;
  char interface_name[10];
} INIT_PARAM;

#define true 1
#define false 0

// Device modes (transmitter, receiver, non-streaming)
#define DEV_MODE_TX     0x00
#define DEV_MODE_RX     0x01
#define DEV_MODE_NSTM   0x02
#define DEV_MODE_MULTI  0xFF

#define NETWORK_STATE_DOWN    0 
#define NETWORK_STATE_CONFIG  1 
#define NETWORK_STATE_UP      2  

// ---- Debug information control ----------------------------------------------
// Particular levels
#define DBG_QUIET       0
#define DBG_NORMAL      1
#define DBG_ICMP        2
#define DBG_VERBOSE     3

#define ACQ_MODE_CONTINUOUS             0x00000001

// ---- GigE core CPU interface ------------------------------------------------

// Address regions
#define GIGE_REGS       0x0000C000

// Basic configuration and status registers
#define gige_gcsr      (GIGE_REGS + 0x0000)
#define gige_prng      (GIGE_REGS + 0x0004)
#define gige_clk_freq  (GIGE_REGS + 0x0008)
#define gige_tstamp_h  (GIGE_REGS + 0x000C)
#define gige_tstamp_l  (GIGE_REGS + 0x0010)
#define gige_tocnt_div (GIGE_REGS + 0x0014)
#define gige_tocnt     (GIGE_REGS + 0x0018)
#define gige_dhcp_tmr  (GIGE_REGS + 0x001C)
#define gige_timer_h   (GIGE_REGS + 0x0020)
#define gige_timer_l   (GIGE_REGS + 0x0024)
//                                  0x0028
#define gige_ethsize   (GIGE_REGS + 0x002C)
#define gige_mac_h     (GIGE_REGS + 0x0030)
#define gige_mac_l     (GIGE_REGS + 0x0034)
//                                  0x0038
//                                  0x003C
//                                  0x0040
#define gige_ip        (GIGE_REGS + 0x0044)
#define gige_txlen     (GIGE_REGS + 0x0048)
#define gige_rxlen     (GIGE_REGS + 0x004C)
#define gige_ctl_mac_h (GIGE_REGS + 0x0050)
#define gige_ctl_mac_l (GIGE_REGS + 0x0054)
//                                  0x0058
//                                  0x005C
//                                  0x0060
#define gige_ctl_ip    (GIGE_REGS + 0x0064)
#define gige_ctl_port  (GIGE_REGS + 0x0068)
#define gige_stm_mac_h (GIGE_REGS + 0x006C)
#define gige_stm_mac_l (GIGE_REGS + 0x0070)
//                                  0x0074
//                                  0x0078
//                                  0x007C
#define gige_stm_ip    (GIGE_REGS + 0x0080)
#define gige_stm_port  (GIGE_REGS + 0x0084)
#define gige_stm_delay (GIGE_REGS + 0x0088)
#define gige_stm_size  (GIGE_REGS + 0x008C)
#define gige_rx_mac_h  (GIGE_REGS + 0x0090)
#define gige_rx_mac_l  (GIGE_REGS + 0x0094)
//                                  0x0098
//                                  0x009C
//                                  0x00A0
#define gige_rx_ip     (GIGE_REGS + 0x00A4)
#define gige_rx_port   (GIGE_REGS + 0x00A8)
//
#define gige_stm_num   (GIGE_REGS + 0x00C0)
#define gige_stm_idx   (GIGE_REGS + 0x00C4)
#define gige_multipart (GIGE_REGS + 0x00C8)

// Constant registers defined in user application

// ---- GigE Vision status codes -----------------------------------------------

#define GEV_STATUS_SUCCESS              0x0000
#define GEV_STATUS_NOT_IMPLEMENTED      0x8001
#define GEV_STATUS_INVALID_PARAMETER    0x8002
#define GEV_STATUS_INVALID_ADDRESS      0x8003
#define GEV_STATUS_WRITE_PROTECT        0x8004
#define GEV_STATUS_BAD_ALIGNMENT        0x8005
#define GEV_STATUS_ACCESS_DENIED        0x8006
#define GEV_STATUS_BUSY                 0x8007
#define GEV_STATUS_LOCAL_PROBLEM        0x8008
#define GEV_STATUS_MSG_MISMATCH         0x8009
#define GEV_STATUS_INVALID_PROTOCOL     0x800A
#define GEV_STATUS_NO_MSG               0x800B
#define GEV_STATUS_PACKET_UNAVAILABLE   0x800C
#define GEV_STATUS_DATA_OVERRUN         0x800D
#define GEV_STATUS_INVALID_HEADER       0x800E
#define GEV_STATUS_ERROR                0x8FFF


// ---- GigE Vision events -----------------------------------------------------

#define GEV_EVENT_TRIGGER               0x0002
#define GEV_EVENT_START_OF_EXPOSURE     0x0003
#define GEV_EVENT_END_OF_EXPOSURE       0x0004
#define GEV_EVENT_START_OF_TRANSFER     0x0005
#define GEV_EVENT_END_OF_TRANSFER       0x0006
#define GEV_EVENT_ERROR                 0x8001
#define GEV_EVENT_DEVSPEC               0x9000


// ---- GigE Vision pixel formats ----------------------------------------------

#define GVSP_PIX_MONO8                  0x01080001
#define GVSP_PIX_MONO8_SIGNED           0x01080002
#define GVSP_PIX_MONO10                 0x01100003
#define GVSP_PIX_MONO10_PACKED          0x010C0004
#define GVSP_PIX_MONO12                 0x01100005
#define GVSP_PIX_MONO12_PACKED          0x010C0006
#define GVSP_PIX_MONO14                 0x01100025
#define GVSP_PIX_MONO16                 0x01100007
#define GVSP_PIX_BAYGR8                 0x01080008
#define GVSP_PIX_BAYRG8                 0x01080009
#define GVSP_PIX_BAYGB8                 0x0108000A
#define GVSP_PIX_BAYBG8                 0x0108000B
#define GVSP_PIX_BAYGR10                0x0110000C
#define GVSP_PIX_BAYRG10                0x0110000D
#define GVSP_PIX_BAYGB10                0x0110000E
#define GVSP_PIX_BAYBG10                0x0110000F
#define GVSP_PIX_BAYGR12                0x01100010
#define GVSP_PIX_BAYRG12                0x01100011
#define GVSP_PIX_BAYGB12                0x01100012
#define GVSP_PIX_BAYBG12                0x01100013
#define GVSP_PIX_RGB8_PACKED            0x02180014
#define GVSP_PIX_BGR8_PACKED            0x02180015
#define GVSP_PIX_RGBA8_PACKED           0x02200016
#define GVSP_PIX_BGRA8_PACKED           0x02200017
#define GVSP_PIX_RGB10_PACKED           0x02300018
#define GVSP_PIX_BGR10_PACKED           0x02300019
#define GVSP_PIX_RGB12_PACKED           0x0230001A
#define GVSP_PIX_BGR12_PACKED           0x0230001B
#define GVSP_PIX_RGB10V1_PACKED         0x0220001C
#define GVSP_PIX_RGB10V2_PACKED         0x0220001D
#define GVSP_PIX_YUV411_PACKED          0x020C001E
#define GVSP_PIX_YUV422_PACKED          0x0210001F
#define GVSP_PIX_YUV444_PACKED          0x02180020
#define GVSP_PIX_RGB8_PLANAR            0x02180021
#define GVSP_PIX_RGB10_PLANAR           0x02300022
#define GVSP_PIX_RGB12_PLANAR           0x02300023
#define GVSP_PIX_RGB16_PLANAR           0x02300024

// Event IDs for user firmware callback
#define LIB_EVENT_NONE                  0x00000000
#define LIB_EVENT_GVCP_CONFIG_WRITE     0x00000001
#define LIB_EVENT_STREAM_OPEN_CLOSE     0x00000002
#define LIB_EVENT_SCCFG_WRITE           0x00000003
#define LIB_EVENT_APP_DISCONNECT        0x00000004
#define LIB_EVENT_LINK_DOWN             0x00000005
#define LIB_EVENT_LINK_CONFIG_WRITE     0x00000006
#define LIB_EVENT_SCHEDULED_ACTION      0x00000007

// Physical link configuration capability bits
#define LINK_CONFIG_CAP_MASK            0x0000000F
#define LINK_CONFIG_CAP_SL              0x00000001
#define LINK_CONFIG_CAP_ML              0x00000002
#define LINK_CONFIG_CAP_SLAG            0x00000004
#define LINK_CONFIG_CAP_DLAG            0x00000008

// Physical link configuration options
#define LINK_CONFIG_MASK                0x00000003
#define LINK_CONFIG_SL                  0
#define LINK_CONFIG_ML                  1
#define LINK_CONFIG_SLAG                2
#define LINK_CONFIG_DLAG                3

#ifdef __cplusplus
extern "C" {
#endif

// Control of the registers
u32 gige_get_register(u8 device, u32 addr);
void gige_set_register(u8 device, u32 addr, u32 value);

u32 gige_get_user_register(u8 device, u32 addr);
void gige_set_user_register(u8 device, u32 addr, u32 value);

u32 gige_get_framebuffer_register(u8 device, u32 addr);
void gige_set_framebuffer_register(u8 device, u32 addr, u32 value);

// Control of the parameters
void gige_set_params(u8 device, u8 uart_bypass, u8 heartbeat_en, u8 payload_type);
void gige_get_params(u8 device, u8 *uart_bypass, u8 *heartbeat_en, u8 *payload_type);

void gige_set_gev_version(u8 device, u8 ver);
u8 gige_get_gev_version(u8 device);
void gige_force_gev_version(u8 device, u32 ver);

// Initialization and configuration functions
int gige_init(u8 device, INIT_PARAM *init_param);
void gige_close(u8 device);
int gige_start(u8 device);
u8 gige_get_link_status(u8 device);
int gige_get_socket(u8 device);
void gige_set_sernum(u8 device, char *sn);
void gige_fb_init(u8 device, u32 bottom, u32 top);
u8 gige_get_network_status(u8 device);
void gige_set_event_callback(u8 device, GIGE_EVENT_CALLBACK_FUNC c_func);
void gige_set_data_rates(u8 device, u32 stm_tx_freq, u32 eth_rate);
void gige_set_acquisition_status(u8 device, u32 channel, u32 status);
u32 gige_get_acquisition_status(u8 device, u32 channel);

void gige_set_pending_ack_timeout(u8 device, u32 timeout);
u32 gige_get_pending_ack_timeout(u8 device);

void gige_set_resend_support(u8 device, u8 en);
u8 gige_get_resend_support(u8 device);

// Advanced parameters acces
void gige_set_multipart_support(u8 device, u8 en);
u8   gige_get_multipart_support(u8 device);
void gige_set_gendc_support(u8 device, u8 en);
u8   gige_get_gendc_support(u8 device);
void gige_set_sceba(u8 device, u32 channel, u32 address);
u32  gige_get_sceba(u8 device, u32 channel);
void gige_set_scmbs(u8 device, u32 channel, u64 mbs);
u64  gige_get_scmbs(u8 device, u32 channel);
void gige_set_link_config_cap(u8 device, u32 capability);
u32  gige_get_link_config_cap(u8 device);
void gige_set_link_config(u8 device, u32 configuration);
u32  gige_get_link_config(u8 device);

// Input/output functions
void gige_print_header(u8 device);
void gige_send_message(u8 device, u16 event, u16 channel, u16 data_len, u8 *data, u64 *msg_timestamp);
void gige_send_pending_ack(u8 device, u16 millisToComplete);

// eeprom functions
u8 gige_eeprom_write_dword(u8 device, u16 address, u32 value);
u8 gige_eeprom_read_dword(u8 device, u16 address, u32 *value);

// sensor functions
void gige_sensor_write_byte(u8 device, u8 address, u8 value);
u8 gige_sensor_read_byte(u8 device, u8 address);
void gige_sensor_write_word(u8 device, u8 address, u16 value);
u16 gige_sensor_read_word(u8 device, u8 address);

// network config callback functions
void gige_set_local_net_config_callback(u8 device, LOCAL_NET_CONFIG_CALLBACK_FUNC c_func);
void gige_set_system_net_config_callback(u8 device, SYSTEM_NET_CONFIG_CALLBACK_FUNC c_func);
void gige_set_local_link_net_config_callback(u8 device, LOCAL_LINK_NET_CONFIG_CALLBACK_FUNC c_func);

u8 gige_i2c_read(u8 device, u32 dev_addr, u32 address, u32 *value, u8 nr_addr, u8 nr_data);

// Authentication and licensing
uint8_t gige_get_auth_status(uint8_t device);
uint32_t gige_get_license_checksum(uint8_t device);
int gige_get_license_hash(uint8_t device, uint8_t *hash);

// stream channel direction
int gige_set_stmdir(u8 device, u32 channel, int dir_rx);
int gige_get_stmdir(u8 device, u32 channel);

// action callback
void gige_set_action_trigger_callback(u8 device, GIGE_ACTION_TRIGGER_CALLBACK_FUNC action_trigger_func);

#ifdef __cplusplus
}
#endif
#endif
