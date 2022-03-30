/******************************************************************************/
/*  1 GigE Vision Reference Design                                           */
/*----------------------------------------------------------------------------*/
/*    File :  main.c                                                          */
/*    Date :  2022-03-09                                                      */
/*     Rev :  0.6                                                             */
/*  Author :  JP                                                              */
/*----------------------------------------------------------------------------*/
/*  NBASE-T GigE Vision reference design firmware for ZCU102                  */
/*----------------------------------------------------------------------------*/
/*  0.1  |  2018-07-20  |  JP  |  Initial release                             */
/*  0.2  |  2020-08-14  |  PD  |  Core and Library Update                     */
/*  0.3  |  2020-08-20  |  PD  |  10G/25G PCS/PMA added                       */
/*  0.4  |  2021-09-14  |  SS  |  Update to latest core and Vivado 2020.2     */
/*  0.5  |  2021-10-20  |  RW  |  Ported to linux                             */
/*  0.6  |  2022-03-09  |  SS  |  Prepared for kr260                          */
/******************************************************************************/
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <syslog.h>
#include <termios.h>
#include <stdint.h>

#include "libGigE.h"
#include "user.h"
#include "flash.h"
#include "framebuffer.h"
#include "gendc.h"

// ---- Global constants and variables -----------------------------------------
// Operate in GEV 2.x gige_set_gev_version
#define _GEV_VERSION_2_

static int exit_flag = 0;  /* SIGINT, SIGTERM, SIGQUIT */

u8 device = 1;

static void sigexit(int sig)
{
    exit_flag = 1;
}

int creatpidfile(void)
{
    FILE	*f;
    pid_t	pid;
    char	*pidfile = "/var/run/gvrd.pid";

    pid = getpid();
    if ((f = fopen(pidfile, "w")) == NULL) {
        syslog(LOG_ERR, "Failed to open %s: %m", pidfile);
        return(-1);
    }
    fprintf(f, "%d\n", pid);
    fclose(f);
    return(0);
}

void removepidfile(void)
{
    unlink("/var/run/gvrd.pid");
}

int system_net_config(u8 dhcp, u32 ip, u32 netmask, u32 gateway)
{
    // do nothing
    return(0);
}

int getchar_no_wait(void)
{
    struct termios oldt,
    newt;
    int ch;

    tcgetattr( STDIN_FILENO, &oldt );
    newt = oldt;
    newt.c_lflag &= ~( ICANON | ECHO );
    newt.c_cc[VTIME]    = 1;     /* inter-character timer unused */
    newt.c_cc[VMIN]     = 0;     /* inter-character timer unused */
    tcsetattr( STDIN_FILENO, TCSANOW, &newt );
    ch = getchar();
    tcsetattr( STDIN_FILENO, TCSANOW, &oldt );
    return ch;
}

int getchar_no_wait1(void)
{
    struct termios oldt,
    newt;
    int ch;

    tcgetattr( 0, &oldt );
    newt = oldt;
    newt.c_lflag &= ~( ICANON | ECHO );
    newt.c_cc[VTIME]    = 0;     /* inter-character timer unused */
    newt.c_cc[VMIN]     = 1;     /* inter-character timer unused */
    tcsetattr( STDIN_FILENO, TCSANOW, &newt );
    //if(read(0,&ch,1) < 0)
    //    perror("read()");
    ch = getchar();
    tcsetattr( STDIN_FILENO, TCSANOW, &oldt );
    return ch;
}

int main(int argc, char* argv[])
{
    INIT_PARAM init_param;
    u32 help;

    printf("GVRD\n");

    // Set init parameter
    strcpy((char *)init_param.manuf ,"Sensor to Image GmbH");
    strcpy((char *)init_param.model ,"GVRD-KR260");
    strcpy((char *)init_param.dver  ,"1.0.0");
    strcpy((char *)init_param.minfo ,"KR260 10Gbps GigE Vision Reference Design");

    init_param.bus_clk_freq       = 100000000; // frequency of the CPU bus clock in Hz
    init_param.data_rate          = 0;
    init_param.eth_mtu            = SCPS_MAX;  // set mtu max
    init_param.mtu_min            = SCPS_MIN;  // set mtu min
    init_param.mtu_inc            = SCPS_INC;  // set mtu inc
    init_param.verbosity          = DBG_ICMP;
    init_param.sdram_base_addr    = 0;         // No MPMC in Zynq FPGAs
    init_param.sdram_high_addr    = 0;         // No MPMC in Zynq FPGAs
    init_param.i2c_eeprom         = 0;         // EEPROM is a file
    //init_param.i2c_eeprom	= 0xA0;     // 24C64 I2C EEPROM
    init_param.eeprom_write_delay = 10000;    // 10 ms for 24C64
    init_param.i2c_sensor         = 0xB8;     // Image sensor (unused)
    init_param.sensor_write_delay = 0;        // None
    init_param.lib_mode           = DEV_MODE_TX;   // sender

    // set network interface name
    strcpy((char *)init_param.interface_name,"eth0");

    // This callback function must be always implemented!
    // It is called by the libGigE
    init_param.user_callback = (USER_CALLBACK_FUNC)user_callback;     // set user callback function
    init_param.user_get_callback = (USER_GET_CALLBACK_FUNC)get_user_reg;  // set read GigE Vision user-space bootstrap register callback function
    init_param.user_set_callback = (USER_SET_CALLBACK_FUNC)set_user_reg;  // set write GigE Vision user-space bootstrap register callback function

    // set the system net config callback function, this is need for test application ( without set any network settings)
    // gige_set_system_net_config_callback(system_net_config);

    // Initialize GigE hardware and image sensor
    if(gige_init(device, &init_param) != 0)
    {
        printf("Can't open eeprom file. Please run update_eeprom and update_atable applications.\n");
        exit(0);
    }

#ifndef _GEV_VERSION_2_
    // GEV 1.2 mode
    gige_set_gev_version(device, 1);                    // GEV 1.x
    // Initialize Framebuffer core
    init_famebuffer(device,1, 0, 1, 0x04000000);        // Low latency, progressive-scan, GEV 1.x, buffer size
    printf("[UU] Running in GEV 1.2 mode\n");
#else
    // GEV 2.0 mode
    gige_set_gev_version(device, 2);                    // GEV 2.x
    gige_force_gev_version(device, 0x00020001);         // Force gev version to 2.1
    // Initialize Framebuffer core
    init_famebuffer(device, 1, 0, 0, 0x04000000);       // Low latency, progressive-scan, GEV 2.x, buffer size
    printf("[UU] Running in GEV 2.1 mode\n");
#endif

    // set generic libgige user callback------------------------------------------
    gige_set_event_callback(device, gige_event);

    // Enable GenDC Support
    gige_set_gendc_support(device, 1);

    gige_set_sceba(device, 0, MAP_SCEBA);

    // set action callback------------------------------------------
    //gige_set_action_trigger_callback(device, action_trigger);

    // Set the data rates for correct SCPD
    gige_set_data_rates(device, 200, 10000);            // f(tx_stm_clk) = 100MHz, 1Gbps Ethernet link

    // Initialize user settings
    user_init(device);

    // Initialize flash
    flash_init();

    // Print device information to std_out
    gige_print_header(device);

    // Start GigE
    gige_start(device);

    // init signal handler
    signal(SIGTERM, sigexit); // catch kill signal
    signal(SIGHUP, sigexit);  // catch hang up signal
    signal(SIGQUIT, sigexit); // catch quit signal
    signal(SIGINT, sigexit);  // catch a CTRL-c signal

    // create pid file
    creatpidfile();

    //setvbuf(stdin,NULL,_IONBF,0);
    // wait of end (kill)
    while(!exit_flag)
    {
        //pause();

        int c;
        c = getchar_no_wait1();
        if(c != -1)
            printf("key: %d\n",c);

        if(c == 27)
            break;
        if(c == 'p')
            framebuf_printregs(device);

        if(c == 'a')
        {
            printf("GigE Auth Status = 0x%02X\r\n", gige_get_auth_status(device));
        }
        if(c == 'l')
        {
            printf("GigE Lic Checksum = 0x%08X\r\n", gige_get_license_checksum(device));
        }
        if(c == 'g')
        {
            gendc_info(device);
        }

    }

    // Close flash
    flash_close();

    // Close GigE hardware
    gige_close(device);

    // close frmebuffer
    close_famebuffer(device);

    // delete pid file
    removepidfile();

    // printf("GEVServer end...\n");

    exit(0);
}
