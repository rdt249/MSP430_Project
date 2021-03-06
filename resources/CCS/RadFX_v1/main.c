#include <intrinsics.h>
#include <msp430.h>
#include <string.h>


/* DESCRIPTION
 *
 * phase 0 : initialize all the modules.
 * phase 1 : output MCLK and ACLK to GPIO pins,
 *           after 'j' is received over UART, go on to phase 2.
 * phase 2 : read ADC and transmit result every time 'j' is received.
 */

/********************** GLOBAL VARIABLES AND SETTINGS *********************/

#define BAUD_RATE   9600
#define LPM LPM4_bits

static const char token = 'j';
int phase = 0;

/********************** INIT FUNCTIONS *********************/

void init_adc(){
    while(REFCTL0 & REFGENBUSY);       // If ref generator busy, WAIT
    REFCTL0 |= REFVSEL_0 | REFON;       // Select internal ref = 1.2V ///////////////////// FOR ADC12 REF VOLTAGE
                                        // Internal Reference ON
    // Configure ADC12
    ADC12CTL0 = ADC12SHT0_2 | ADC12ON;                                 /////////////////// ADC CONFIGURATION
    ADC12CTL1 = ADC12SHP;           // ADCCLK = MODOSC; sampling timer
    ADC12CTL2 |= ADC12RES_2;         // 12-bit conversion results
    ADC12IER0 |= ADC12IE3 | ADC12IE0;          // Enable ADC conv complete interrupt
    //ADC12MCTL0 |= ADC12INCH_3 | ADC12VRSEL_1; // A3 ADC input select; Vref=1.2V
    ADC12MCTL0 |= ADC12INCH_8;                      // MEM0 from channel A8

    while(!(REFCTL0 & REFGENRDY));      // Wait for reference generator
                      // to settle
    ADC12CTL0 |= ADC12ENC;// | ADC12SC;     // Sampling and conversion start
    //P1OUT |= BIT0; // turn on P1.0
}

void init_uart(void)
{
    UCA0CTLW0 = UCSWRST;                      // Put eUSCI in reset
    UCA0CTLW0 |= UCSSEL__SMCLK;               // CLK = SMCLK = 16MHz

#if BAUD_RATE == 2000000
    UCA0BR0 = 8;
    UCA0BR1 = 0;
    UCA0MCTLW = 0;
#elif BAUD_RATE == 1000000
    UCA0BR0 = 16;
    UCA0BR1 = 0;
    UCA0MCTLW = 0;
#elif BAUD_RATE == 460800
    UCA0BR0 = 2;
    UCA0BR1 = 0;
    UCA0MCTLW = UCOS16 | UCBRF_2 | (0xbb << 8);
#elif BAUD_RATE == 230400
    UCA0BR0 = 4;
    UCA0BR1 = 0;
    UCA0MCTLW = UCOS16 | UCBRF_5 | (0x55 << 8);
#elif BAUD_RATE == 115200
    UCA0BR0 = 8;
    UCA0BR1 = 0;
    UCA0MCTLW = UCOS16 | UCBRF_10 | (0xf7 << 8);
#elif BAUD_RATE == 9600
    UCA0BR0 = 104;
    UCA0BR1 = 0;
    UCA0MCTLW = UCOS16 | UCBRF_2 | (0xd6 << 8);
#endif

    UCA0CTLW0 &= ~UCSWRST;                    // Initialize eUSCI
    UCA0IE |= UCRXIE;// | UCTXIE;                // Enable USCI_A0 RX interrupt
}

void init_clocks(void){
    // Clock System Setup
    // Configure one FRAM waitstate as required by the device datasheet for MCLK
       // operation beyond 8MHz _before_ configuring the clock system.
       FRCTL0 = FRCTLPW | NWAITS_1;
       CSCTL0_H = CSKEY >> 8;                    // Unlock clock registers
       CSCTL1 = DCOFSEL_4 | DCORSEL;               // Set DCO to 16MHz
       CSCTL2 = SELA__VLOCLK | SELS__DCOCLK | SELM__DCOCLK; // Set SMCLK = MCLK = DCO,
       CSCTL3 = DIVA__1 | DIVS__1 | DIVM__1;     // Set all divider
       CSCTL0_H = 0;                             // Lock CS registers
}

void init_gpio()
{
    P1OUT &= ~BIT0;                           // Clear P1.0 output latch for a defined power-on state
    P1DIR |= BIT0;                            // Set P1.0 to output direction
//===============Clocks GPIO Initialization===================
    P4DIR |= BIT1;
    P4SEL0 |= BIT1;                           // Output ACLK
    P4SEL1 |= BIT1;

    P10DIR |= BIT2;
    P10SEL1 |= BIT2;                           // Output SMCLK
    P10SEL0 |= BIT2;

    P4DIR |= BIT0;
    P4SEL1 |= BIT0;                           // Output MCLK
    P4SEL0 |= BIT0;
//===============USCI_A0 UART operation===================
    P2SEL0 |= BIT0 | BIT1;                    // USCI_A0 UART operation
    P2SEL1 &= ~(BIT0 | BIT1);
//===============ADC input GPIO pin configuration===================
    //P1SEL1 |= BIT3;              // Configure P1.3 for ADC
    //P1SEL0 |= BIT3;
    P9SEL0 |= 0x0f; // ADC12 operation (A8 ~ A11)
    P9SEL1 |= 0x0f;

    // Disable the GPIO power-on default high-impedance mode to activate
    // previously configured port settings
    PM5CTL0 &= ~LOCKLPM5;
}

/************************* MISC FUNCTIONS ************************/

void uart_tx(char data)
{         // writes a byte to serial comms
    while(!(UCA0IFG & UCTXIFG));
    UCA0TXBUF = data;
}

void uart_tx_string(char *string)
{   // writes a string to serial comms
    int length = strlen(string);    // find length of string
    while(length>0) // iterate over length of string
    {
        uart_tx(*string);   // tx one letter
        length--;   // decrement length
        *string++;  // increment pointer
    }
}

void uart_tx_number(long number)
{
    int digits = 0;
    if(number == 0){
        uart_tx('0');   // if number = 0, go ahead and send 0
    }
    long reverse = 0;
    while(number > 0){
        reverse = reverse * 10 + (number % 10);
        number /= 10;
        digits++;
    }
    char word_bank[10] = {'0','1','2','3','4','5','6','7','8','9'};
    while(digits > 0){
        uart_tx(word_bank[reverse % 10]);
        reverse /= 10;
        digits--;
    }
}

void read_adc(void)
{
    ADC12CTL0 |= ADC12SC;    // start conversion
    __bis_SR_register(LPM0_bits | GIE); // wait for conversion to complete
    uart_tx_number(ADC12MEM0);
    uart_tx('\n');
}

/********************** MAIN FUNCTION *********************/

int main(void)
{
    WDTCTL = WDTPW | WDTHOLD;   // stop watchdog timer

    init_clocks();
    init_gpio();
    init_uart();
    init_adc();

    phase = 1;

    __bis_SR_register(GIE); // wait for clock characterization
    while(phase == 1)
    {
        __no_operation();
    }

    while(phase == 2)
    {
        __bis_SR_register(LPM | GIE);
        read_adc();
    }
}

/********************** INTERRUPTS *********************/

#pragma vector=USCI_A0_VECTOR   // UART interrupt vector
__interrupt void USCI_A0_ISR(void)
{
    switch(UCA0IV)
    {
        case USCI_NONE: break;
        case USCI_UART_UCRXIFG: // RX flag
            if(UCA0RXBUF == token)
            {
                __bic_SR_register_on_exit(LPM | GIE);
                phase = 2;
            }
            break;
        case USCI_UART_UCTXIFG: break;
        case USCI_UART_UCSTTIFG: break;
        case USCI_UART_UCTXCPTIFG: break;
    }
}

#pragma vector = ADC12_VECTOR   // ADC interrupt vector
__interrupt void ADC12_ISR(void)
{
    ADC12IFGR0 = 0; // clear IFG
    __bic_SR_register_on_exit(LPM | GIE);
}
