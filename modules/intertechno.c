//
// code used to control Intertechno radio switches
//
// Author:          Petre Rodan <petre.rodan@simplex.ro>
// Available from:  https://github.com/rodan/openchronos-rfsw
// License:         GNU GPLv3
//
// Based on a nice tutorial by Christian M. Schmid
// http://blog.chschmid.com/?page_id=193

#include <openchronos.h>
#include <drivers/display.h>
#include <drivers/timer.h>
#include <drivers/rf1a.h>

#define INTERTECHNO_CMD_ON  0x07
#define INTERTECHNO_CMD_OFF 0x06
#define INTERTECHNO_DEF_FAMILY 12       // this translates as family 'L' on the rotary switch
#define INTERTECHNO_DEF_DEVICE 7        // device number 7 on remotes that have devices numbered 1 to 16
#define INTERTECHNO_SEQ_PKT   2         // how many packets to be sent with every sequence (1, 2, 3 or 4)
#define INTERTECHNO_SEQ_SIZE  INTERTECHNO_SEQ_PKT*16        // 16, 32, 48 or 64 bytes to be allocated for the cmd sequence

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

/*************************** edit mode callbacks **************************/
static void edit_ff_sel(void)
{
        display_chars(0, LCD_SEG_L1_3_2, NULL, BLINK_ON);
}

static void edit_ff_dsel(void)
{
        display_chars(0, LCD_SEG_L1_3_2, NULL, BLINK_OFF);
}

static void edit_ff_set(int8_t step)
{
        helpers_loop(&tmp_family, 1, 16, step);
        _printf(0, LCD_SEG_L1_3_2, "%02u", tmp_family);
}

static void edit_dd_sel(void)
{
        display_chars(0, LCD_SEG_L1_1_0, NULL, BLINK_ON);
}

static void edit_dd_dsel(void)
{
        display_chars(0, LCD_SEG_L1_1_0, NULL, BLINK_OFF);
}

static void edit_dd_set(int8_t step)
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
        {&edit_dd_sel, &edit_dd_dsel, &edit_dd_set},
        {&edit_ff_sel, &edit_ff_dsel, &edit_ff_set},
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
                       NULL, NULL,
                       &intertechno_activated, &intertechno_deactivated);
}

void rf_init(void)
{
        uint8_t PATable[2] = { 0x00, 0xC3 };    //0 dBm

        ResetRadioCore();

        // open PMM module registers for write access
        PMMCTL0_H = 0xA5;
        // global high power module request enable
        PMMCTL0_L |= PMMHPMRE;
        // lock PMM module registers for write access
        PMMCTL0_H = 0x00;

        //WriteSingleReg(IOCFG2, 0x29);       //GDO2 Output Configuration
        //WriteSingleReg(IOCFG1, 0x2E);       //GDO1 Output Configuration
        WriteSingleReg(IOCFG0, 0x06);   //GDO0 Output Configuration
        //WriteSingleReg(FIFOTHR, 0x07);      //FIFO thresholds
        //WriteSingleReg(SYNC1, 0xD3);        //Sync Word, High Byte
        //WriteSingleReg(SYNC0, 0x91);        //Sync Word, Low Byte
        WriteSingleReg(PKTLEN, INTERTECHNO_SEQ_SIZE);   //Packet Length
        //WriteSingleReg(PKTCTRL1, 0x04);     //Packet Automation Control
        WriteSingleReg(PKTCTRL0, 0x00); //Packet Automation Control
        //WriteSingleReg(ADDR, 0x00);         //Device Address
        //WriteSingleReg(CHANNR, 0x00);       //Channel Number
        //WriteSingleReg(FSCTRL1, 0x0F);      //Frequency Synthesizer Control
        //WriteSingleReg(FSCTRL0, 0x00);      //Frequency Synthesizer Control
        WriteSingleReg(FREQ2, 0x10);    //Frequency Control Word, High Byte
        WriteSingleReg(FREQ1, 0xB0);    //Frequency Control Word, Middle Byte
        WriteSingleReg(FREQ0, 0x71);    //Frequency Control Word, Low Byte
        WriteSingleReg(MDMCFG4, 0x86);  //Modem Configuration
        WriteSingleReg(MDMCFG3, 0x70);  //Modem Configuration
        WriteSingleReg(MDMCFG2, 0x30);  //Modem Configuration
        WriteSingleReg(MDMCFG1, 0x02);  //Modem Configuration
        //WriteSingleReg(MDMCFG0, 0xF8);      //Modem Configuration
        //WriteSingleReg(DEVIATN, 0x47);      //Modem Deviation Setting
        //WriteSingleReg(MCSM2, 0x07);        //Main Radio Control State Machine Configuration
        //WriteSingleReg(MCSM1, 0x30);        //Main Radio Control State Machine Configuration
        WriteSingleReg(MCSM0, 0x00);      //Main Radio Control State Machine Configuration
        //WriteSingleReg(MCSM0, 0x10);    //Main Radio Control State Machine Configuration
        WriteSingleReg(FOCCFG, 0x76);   //Frequency Offset Compensation Configuration
        //WriteSingleReg(BSCFG, 0x6C);        //Bit Synchronization Configuration
        //WriteSingleReg(AGCCTRL2, 0x03);     //AGC Control
        //WriteSingleReg(AGCCTRL1, 0x40);     //AGC Control
        //WriteSingleReg(AGCCTRL0, 0x91);     //AGC Control
        WriteSingleReg(WOREVT1, 0x87);  //High Byte Event0 Timeout
        WriteSingleReg(WOREVT0, 0x6B);  //Low Byte Event0 Timeout
        WriteSingleReg(WORCTRL, 0xF8);  //Wake On Radio Control
        WriteSingleReg(FREND0, 0x11);   //Front End TX Configuration
        //WriteSingleReg(FSCAL3, 0xE9);       //Frequency Synthesizer Calibration
        //WriteSingleReg(FSCAL2, 0x2A);       //Frequency Synthesizer Calibration
        //WriteSingleReg(FSCAL1, 0x00);       //Frequency Synthesizer Calibration
        //WriteSingleReg(FSCAL0, 0x1F);       //Frequency Synthesizer Calibration
        //WriteSingleReg(TEST0, 0x09);        //Various Test Settings
        //WriteSingleReg(LQI, 0x80);  //Demodulator Estimate for Link Quality
        //WriteSingleReg(LQI, 0xFF);  //Demodulator Estimate for Link Quality
        //WriteSingleReg(RSSI, 0x80); //Received Signal Strength Indication
        //WriteSingleReg(MARCSTATE, 0x01);    //Main Radio Control State Machine State
        //WriteSingleReg(PKTSTATUS, 0x80);    //Current GDOx Status and Packet Status
        //WriteSingleReg(VCO_VC_DAC, 0xFD);   //Current Setting from PLL Calibration Module
        //WriteSingleReg(TXBYTES, 0x00);      //Underflow and Number of Bytes

        //WriteSingleReg(PA_TABLE1, 0x50);       //0dB output power

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
        uint8_t ib = 0, j;
        uint8_t rprefix;
        uint8_t it_buff[INTERTECHNO_SEQ_SIZE];
        int8_t i;               // unsigned kills it
        uint16_t int_state;

        rprefix = rotate_byte(prefix);

        for (j = 1; j <= INTERTECHNO_SEQ_PKT; j++) {
                for (i = 7; i >= 0; i--) {
                        switch (rprefix & (1 << i)) {
                        case 0:
                                it_buff[ib] = 0x88;
                                break;
                        default:
                                it_buff[ib] = 0x8e;
                                break;
                        }
                        ib++;
                }
                for (i = 3; i >= 0; i--) {
                        switch (cmd & (1 << i)) {
                        case 0:
                                it_buff[ib] = 0x88;
                                break;
                        default:
                                it_buff[ib] = 0x8e;
                                break;
                        }
                        ib++;
                }

                // send sync sequence
                it_buff[ib] = 0x80;
                it_buff[ib + 1] = 0;
                it_buff[ib + 2] = 0;
                it_buff[ib + 3] = 0;
                ib += 4;
        }

        // display RF symbol
        display_symbol(0, LCD_ICON_BEEPER1, SEG_ON);
        display_symbol(0, LCD_ICON_BEEPER2, SEG_ON);
        display_symbol(0, LCD_ICON_BEEPER3, SEG_ON);

        rf_init();

        ENTER_CRITICAL_SECTION(int_state);

        // voodoo zone
        RF1AIES |= BIT9;
        RF1AIFG &= ~BIT9;       // Clear pending interrupts
        RF1AIE &= ~BIT9;        // Disable TX end-of-packet interrupt

        Strobe(RF_SCAL);        // re-calibrate radio
        timer0_delay(10, LPM3_bits); // probably redundant

        // send twice
        for (i = 0; i < 2; i++) {
                WriteBurstReg(RF_TXFIFOWR, it_buff, INTERTECHNO_SEQ_SIZE); // fill up FIFO
                timer0_delay(10, LPM3_bits); // probably redundant
                Strobe(RF_STX); // TX
                timer0_delay(60 * INTERTECHNO_SEQ_PKT, LPM3_bits); // each packet should take at least 54.6 ms
                while ((Strobe(RF_SNOP) & 0xF0) != 0);
        }

        Strobe(RF_SIDLE);       // IDLE
        Strobe(RF_SPWD);        // power-down mode

        EXIT_CRITICAL_SECTION(int_state);

        // clear RF symbol
        display_symbol(0, LCD_ICON_BEEPER1, SEG_OFF);
        display_symbol(0, LCD_ICON_BEEPER2, SEG_OFF);
        display_symbol(0, LCD_ICON_BEEPER3, SEG_OFF);
}

// *************************************************************************************************
// @fn          WriteBurstPATable
// @brief       Write to multiple locations in power table. also dragons
// @param       unsigned char *buffer   Pointer to the table of values to be written 
// @param       unsigned char count     Number of values to be written
// @return      none
// *************************************************************************************************
void WriteBurstPATable(unsigned char *buffer, unsigned char count)
{
        volatile char i = 0;
        uint16_t int_state;

        ENTER_CRITICAL_SECTION(int_state);
        while (!(RF1AIFCTL1 & RFINSTRIFG));
        RF1AINSTRW = 0x7E00 + buffer[i];        // PA Table burst write   

        for (i = 1; i < count; i++) {
                RF1ADINB = buffer[i];   // Send data
                while (!(RFDINIFG & RF1AIFCTL1));       // Wait for TX to finish
        }
        i = RF1ADOUTB;          // Reset RFDOUTIFG flag which contains status byte

        while (!(RF1AIFCTL1 & RFINSTRIFG));
        RF1AINSTRB = RF_SNOP;   // reset PA Table pointer

        EXIT_CRITICAL_SECTION(int_state);
}
