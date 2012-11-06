//
// code used to control Intertechno radio switches
//
// Author:          Petre Rodan <petre.rodan@simplex.ro>
// Available from:  https://github.com/rodan/openchronos-rfsw
// License:         GNU GPLv3
//
// Based on a nice tutorial by Christian M. Schmid
// http://blog.chschmid.com/?page_id=193
//
// Usage:
//
// Line1 contains FF.DD
//
// FF is the decimal notation of the family in which the device is placed [1-16]
//                  A is 01, B is 02 ... P is 16.
//                  this option must be used for doorbell/PIR device selection
// DD is the decimal notation of the device number [1-16]
//                  on some factory remotes devices are placed in 4 groups,
//                  in this case device 2 from group 3 is device number 10.
//                  special devices like doorbell/PIRs must be set as device number 8.
//
// Line2 becomes 'on', 'off', 'spe' depending on what command was sent last
//
// radio glyphs come up when the command is sent.
//
// buttons:
//
// up      - send an 'on' command on the current device
// down    - send an 'off' command
// up+down - send a special command (for doorbells/PIRs), make sure DD is 8 in this case.
// #       - enter config mode. use up, down, # to select family and device, * to save.
//

#include <openchronos.h>
#include <drivers/display.h>
#include <drivers/timer.h>
#include <drivers/rf1a.h>

#define INTERTECHNO_CMD_ON  0x07        // command for turning switches on
#define INTERTECHNO_CMD_OFF 0x06        // command for turning switches off
#define INTERTECHNO_CMD_SP  0x0f        // special devices like doorbells, PIR detectors use this cmd
#define INTERTECHNO_SEQ_SIZE  16        // TX FIFO size to be allocated

// starting values
#define INTERTECHNO_DEF_FAMILY 12       // this translates as family 'L' on the rotary switch
#define INTERTECHNO_DEF_DEVICE 7        // device number 7 on remotes that have devices numbered 1 to 16
                                        // or device 3 group 2 on others

#define st(x)                       do { x } while (__LINE__ == -1)
#define ENTER_CRITICAL_SECTION(x)   st( x = __get_interrupt_state(); __disable_interrupt(); )
#define EXIT_CRITICAL_SECTION(x)    __set_interrupt_state(x)

void rf_init(void);
uint8_t rotate_byte(uint8_t in);
void rf_tx_cmd(uint8_t prefix, uint8_t cmd);
void WriteBurstPATable(unsigned char *buffer, unsigned char count);

uint8_t it_family = INTERTECHNO_DEF_FAMILY;
uint8_t it_device = INTERTECHNO_DEF_DEVICE;
uint8_t tmp_family = INTERTECHNO_DEF_FAMILY;
uint8_t tmp_device = INTERTECHNO_DEF_DEVICE;

static void intertechno_activated()
{
    _printf(0, LCD_SEG_L1_3_2, "%02u", it_family);
    _printf(0, LCD_SEG_L1_1_0, "%02u", it_device);
    display_symbol(0, LCD_SEG_L1_DP1, SEG_ON);
}

static void intertechno_deactivated()
{
    display_clear(0, 1);
}

static void intertechno_up_pressed()
{
    rf_tx_cmd(((it_family - 1) << 4) + it_device - 1, INTERTECHNO_CMD_ON);
    display_chars(0, LCD_SEG_L2_2_0, "ON ", SEG_SET);
}

static void intertechno_down_pressed()
{
    rf_tx_cmd(((it_family - 1) << 4) + it_device - 1, INTERTECHNO_CMD_OFF);
    display_chars(0, LCD_SEG_L2_2_0, "OFF", SEG_SET);
}

static void intertechno_updown_pressed()
{
    rf_tx_cmd(((it_family - 1) << 4) + it_device - 1, INTERTECHNO_CMD_SP);
    display_chars(0, LCD_SEG_L2_2_0, "SPE", SEG_SET);
}

/*************************** edit mode callbacks **************************/
static void it_edit_ff_sel(void)
{
    display_chars(0, LCD_SEG_L1_3_2, NULL, BLINK_ON);
}

static void it_edit_ff_dsel(void)
{
    display_chars(0, LCD_SEG_L1_3_2, NULL, BLINK_OFF);
}

static void it_edit_ff_set(int8_t step)
{
    helpers_loop(&tmp_family, 1, 16, step);
    _printf(0, LCD_SEG_L1_3_2, "%02u", tmp_family);
}

static void it_edit_dd_sel(void)
{
    display_chars(0, LCD_SEG_L1_1_0, NULL, BLINK_ON);
}

static void it_edit_dd_dsel(void)
{
    display_chars(0, LCD_SEG_L1_1_0, NULL, BLINK_OFF);
}

static void it_edit_dd_set(int8_t step)
{
    helpers_loop(&tmp_device, 1, 16, step);
    _printf(0, LCD_SEG_L1_1_0, "%02u", tmp_device);
}

static void intertechno_save(void)
{
    it_family = tmp_family;
    it_device = tmp_device;
}

/* edit mode item table */
static struct menu_editmode_item intertechno_items[] = {
    {&it_edit_dd_sel, &it_edit_dd_dsel, &it_edit_dd_set},
    {&it_edit_ff_sel, &it_edit_ff_dsel, &it_edit_ff_set},
    {NULL}
};

static void intertechno_num_pressed()
{
    menu_editmode_start(&intertechno_save, intertechno_items);
}

void mod_intertechno_init()
{
    menu_add_entry("INTER",
                   &intertechno_up_pressed,
                   &intertechno_down_pressed,
                   &intertechno_num_pressed,
                   NULL,
                   NULL, &intertechno_updown_pressed,
                   &intertechno_activated, &intertechno_deactivated);
}

void rf_init(void)
{
    uint8_t PATable[2] = { 0x00, 0xC3 };    // logic 0 and logic 1 power levels for OOK modulation

    ResetRadioCore();

    // open PMM module registers for write access
    PMMCTL0_H = 0xA5;
    // global high power module request enable
    PMMCTL0_L |= PMMHPMRE;
    // lock PMM module registers for write access
    PMMCTL0_H = 0x00;

    // minimal register changes
    WriteSingleReg(IOCFG0, 0x06);       //GDO0 Output Configuration
    WriteSingleReg(PKTLEN, INTERTECHNO_SEQ_SIZE);       //Packet Length
    WriteSingleReg(PKTCTRL0, 0x00);     //Packet Automation Control
    WriteSingleReg(FREQ2, 0x10);        //Frequency Control Word, High Byte
    WriteSingleReg(FREQ1, 0xB0);        //Frequency Control Word, Middle Byte
    WriteSingleReg(FREQ0, 0x71);        //Frequency Control Word, Low Byte
    WriteSingleReg(MDMCFG4, 0x86);      //Modem Configuration
    WriteSingleReg(MDMCFG3, 0x70);      //Modem Configuration
    WriteSingleReg(MDMCFG2, 0x30);      //Modem Configuration
    WriteSingleReg(MDMCFG1, 0x02);      //Modem Configuration
    WriteSingleReg(MCSM0, 0x00);        //Main Radio Control State Machine Configuration
    WriteSingleReg(MCSM1, 0x00);        //Main Radio Control State Machine Configuration
    WriteSingleReg(FOCCFG, 0x76);       //Frequency Offset Compensation Configuration
    WriteSingleReg(WOREVT1, 0x87);      //High Byte Event0 Timeout
    WriteSingleReg(WOREVT0, 0x6B);      //Low Byte Event0 Timeout
    WriteSingleReg(WORCTRL, 0xF8);      //Wake On Radio Control
    WriteSingleReg(FREND0, 0x11);       //Front End TX Configuration

    WriteBurstPATable(&PATable[0], 2);
}

uint8_t rotate_byte(uint8_t in)
{
    uint8_t rv = 0;
    rv += (in & 0x10) << 3;
    rv += (in & 0x20) << 1;
    rv += (in & 0x40) >> 1;
    rv += (in & 0x80) >> 3;
    rv += (in & 0x1) << 3;
    rv += (in & 0x2) << 1;
    rv += (in & 0x4) >> 1;
    rv += (in & 0x8) >> 3;
    return rv;
}

void rf_tx_cmd(uint8_t prefix, uint8_t cmd)
{
    uint8_t p = 0;
    uint8_t rprefix;
    uint8_t it_buff[INTERTECHNO_SEQ_SIZE];
    int8_t i;

    rprefix = rotate_byte(prefix);

    // replace 1 with 0x8e and 0 with 0x88
    for (i = 7; i >= 0; i--) {
        if (rprefix & (1 << i)) {
            it_buff[p++] = 0x8e;
        } else {
            it_buff[p++] = 0x88;
        }
    }

    for (i = 3; i >= 0; i--) {
        if (cmd & (1 << i)) {
            it_buff[p++] = 0x8e;
        } else {
            it_buff[p++] = 0x88;
        }
    }

    // sync sequence
    it_buff[p++] = 0x80;
    it_buff[p++] = 0;
    it_buff[p++] = 0;
    it_buff[p++] = 0;

    // display RF symbol
    display_symbol(0, LCD_ICON_BEEPER1, SEG_ON);
    display_symbol(0, LCD_ICON_BEEPER2, SEG_ON);
    display_symbol(0, LCD_ICON_BEEPER3, SEG_ON);

    rf_init();

    Strobe(RF_SCAL);            // re-calibrate radio

    // factory remotes send the sequence 4 times
    for (i = 0; i < 4; i++) {
        // voodoo
        //RF1AIES |= BIT9;
        //RF1AIFG &= ~BIT9;       // Clear pending interrupts
        //RF1AIE &= ~BIT9;        // Disable TX end-of-packet interrupt

        WriteBurstReg(RF_TXFIFOWR, it_buff, INTERTECHNO_SEQ_SIZE);      // fill up FIFO
        Strobe(RF_STX);         // TX
        timer0_delay(60, LPM3_bits);    // each sequence should take at least 54.6 ms

        // for unknown reasons in 90% of cases during the _last_ cycle TXFIFOWR gets filled
        // but STX doesn't send it
        // TXFIFO either remains filled forever, until RF_SFTX, or RF_STX - whichever is first
        // temporary fix
        if (ReadSingleReg(TXBYTES)) {
            Strobe(RF_STX);
            timer0_delay(60, LPM3_bits);
        }

        while ((Strobe(RF_SNOP) & 0xF0) != 0) ; // wait until radio core ready and IDLE state
    }

    Strobe(RF_SIDLE);           // IDLE
    Strobe(RF_SFTX);            // flush TXFIFO
    Strobe(RF_SPWD);            // power-down mode

    // clear RF symbol
    display_symbol(0, LCD_ICON_BEEPER1, SEG_OFF);
    display_symbol(0, LCD_ICON_BEEPER2, SEG_OFF);
    display_symbol(0, LCD_ICON_BEEPER3, SEG_OFF);

}

// FIXME should be moved to drivers/rf1a.c
// function taken from the RF1A lib available at http://www.ti.com/lit/zip/slaa460
// *****************************************************************************
// @fn          WritePATable
// @brief       Write to multiple locations in power table 
// @param       unsigned char *buffer   Pointer to the table of values to be written 
// @param       unsigned char count     Number of values to be written
// @return      none
// *****************************************************************************
void WriteBurstPATable(unsigned char *buffer, unsigned char count)
{
    volatile char i = 0;
    uint16_t int_state;

    ENTER_CRITICAL_SECTION(int_state);

    while (!(RF1AIFCTL1 & RFINSTRIFG)) ;
    RF1AINSTRW = 0x7E00 + buffer[i];    // PA Table burst write   

    for (i = 1; i < count; i++) {
        RF1ADINB = buffer[i];   // Send data
        while (!(RFDINIFG & RF1AIFCTL1)) ;      // Wait for TX to finish
    }
    i = RF1ADOUTB;              // Reset RFDOUTIFG flag which contains status byte

    while (!(RF1AIFCTL1 & RFINSTRIFG)) ;
    RF1AINSTRB = RF_SNOP;       // reset PA Table pointer

    EXIT_CRITICAL_SECTION(int_state);
}
