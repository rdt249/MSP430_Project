#include <msp430.h>
//#include <launchpad.h>
//#include <gpio.h>
//#include <uart.h>
#define samples 943
void clocks(void);
void adc();
void uart_tx_number(long number);
void uart_tx(char data);
void uart_tx_string(char *string);
int i = 0;
int result[samples];
int main(void)
{
       WDTCTL = WDTPW | WDTHOLD;   // stop watchdog timer
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
      P1SEL1 |= BIT3;              // Configure P1.3 for ADC
      P1SEL0 |= BIT3;
      // Disable the GPIO power-on default high-impedance mode to activate
      // previously configured port settings
      PM5CTL0 &= ~LOCKLPM5;
//===============Calling Modules====================
      adc(); // initialize ADC and UART
      __bis_SR_register(LPM0_bits | GIE); // convert 943 times and TX results

      clocks();                                 // Call Clock Function
      uart_tx('$');
      uart_tx('\n');
      long period = 36956522;
      while(period > 0){
          period -= 1;
      }
//==============Entering Low Power Mode=============
      __bis_SR_register(LPM4_bits);             // Enter Low Power Mode 4

    return 0;
}
void clocks(void){
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
// Port 1 ISR
/*#pragma vector=PORT1_VECTOR
__interrupt void Port_1(void){
     P1.1 ISR
       This is the ISR for the launchpad button S1
    if(P1IFG & BIT1){
        PMMCTL0 |= PMMSWBOR;
//        WDTCTL = 0;
//        SYSRSTIV = SYSRSTIV_DOBOR;
//        launchpad_red(1);   // turn on red led
//        launchpad_green(0); // turn off green led
    }
      P1.2 ISR
       This is the ISR for the launchpad button S2
//    if(P1IFG & BIT2){
//        launchpad_red(0);   // turn off red led
//        launchpad_green(1); // turn on green led
//    }
    P1IFG = 0;                      // clear IFGs
    __bic_SR_register_on_exit(LPM3_bits | GIE);
}
*/
void adc(){
        PM5CTL0 &= ~LOCKLPM5;   //////////////////////////////////////////////////////////////// COMMON FOR ALL PURPOSE
        // By default, REFMSTR=1 => REFCTL is used to configure the internal reference
        // Configure one FRAM waitstate as required by the device datasheet for MCLK
        // operation beyond 8MHz _before_ configuring the clock system.
        FRCTL0 = FRCTLPW | NWAITS_1;
        CSCTL0_H = CSKEY >> 8;                    // Unlock clock registers
        CSCTL1 = DCOFSEL_4 | DCORSEL;               // Set DCO to 16MHz
        CSCTL2 = SELA__VLOCLK | SELS__DCOCLK | SELM__DCOCLK; // Set SMCLK = MCLK = DCO,
        CSCTL3 = DIVA__1 | DIVS__1 | DIVM__1;     // Set all divider
        CSCTL0_H = 0;                             // Lock CS registers
        while(REFCTL0 & REFGENBUSY);       // If ref generator busy, WAIT
        REFCTL0 |= REFVSEL_0 | REFON;       // Select internal ref = 1.2V ///////////////////// FOR ADC12 REF VOLTAGE
                                            // Internal Reference ON
        // Configure ADC12
        ADC12CTL0 = ADC12SHT0_2 | ADC12ON;                                 /////////////////// ADC CONFIGURATION
        ADC12CTL1 = ADC12SHP;           // ADCCLK = MODOSC; sampling timer
        ADC12CTL2 |= ADC12RES_2;         // 12-bit conversion results
        ADC12IER0 |= ADC12IE3 | ADC12IE0;          // Enable ADC conv complete interrupt
        ADC12MCTL0 |= ADC12INCH_3 | ADC12VRSEL_1; // A1 ADC input select; Vref=1.2V
        while(!(REFCTL0 & REFGENRDY));      // Wait for reference generator
                          // to settle
        ADC12CTL0 |= ADC12ENC | ADC12SC;     // Sampling and conversion start
        P1OUT |= BIT0; // turn on P1.0
        // Configure USCI_A0 for UART mode
         UCA0CTLW0 = UCSWRST;                      // Put eUSCI in reset
         UCA0CTLW0 |= UCSSEL__SMCLK;               // CLK = SMCLK
         // Baud Rate calculation
         // 16000000/(16*9600) = 52.083
         // Fractional portion = 0.083
         // User's Guide Table 21-4: UCBRSx = 0x04
         // UCBRFx = int ( (52.083-52)*16) = 1
         UCA0BR0 = 104;                             // 16000000/16/9600
         UCA0BR1 = 0x00;
         UCA0MCTLW |= UCOS16 | UCBRF_2 | 0xD600;
         UCA0CTLW0 &= ~UCSWRST;                    // Initialize eUSCI
         //UCA0IE |= UCRXIE;                          // Enable USCI_A0 RX interrupt
        //__bis_SR_register(LPM3_bits + GIE);   // LPM0, ADC10_ISR will force exit
        //__no_operation();                         // For debugger
}
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector = ADC12_VECTOR
__interrupt void ADC12_ISR(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(ADC12_VECTOR))) ADC12_ISR (void)
#else
#error Compiler not supported!
#endif
{
 switch (__even_in_range(ADC12IV, ADC12IV_ADC12RDYIFG))
 {
  case ADC12IV_NONE:    break;    // Vector 0: No interrupt
  case ADC12IV_ADC12OVIFG: break;    // Vector 2: ADC12MEMx Overflow
  case ADC12IV_ADC12TOVIFG: break;    // Vector 4: Conversion time overflow
  case ADC12IV_ADC12HIIFG: break;    // Vector 6: ADC12BHI
  case ADC12IV_ADC12LOIFG: break;    // Vector 8: ADC12BLO
  case ADC12IV_ADC12INIFG: break;    // Vector 10: ADC12BIN
  case ADC12IV_ADC12IFG0:         // Vector 12: ADC12MEM0 Interrupt
      P1OUT &= ~BIT0; // turn off P1.0
      if(i < samples){
          result[i] = ADC12MEM0;
          i++;
          ADC12CTL0 |= ADC12ENC | ADC12SC; // Sampling and conversion start
          P1OUT |= BIT0; // turn on P1.0
      }
      else{
          ADC12CTL0 &= ~ADC12ENC;
          ADC12IER0 &= ~ADC12IE0;
          P1OUT &= ~BIT0; // turn off P1.0
          // tx results
          i = 0;
          UCA0IE |= UCTXIE;
          uart_tx('[');
          uart_tx('\n');
          for(i = 0; i < samples; i++){
              uart_tx_number(result[i]);
              uart_tx('\n');
          }
          uart_tx(']');
          uart_tx('\n');
          __bic_SR_register_on_exit(LPM0_bits | GIE);
           /*
          i = 0;
          ADC12CTL0 |= ADC12ENC;
          ADC12IER0 |= ADC12IE0;
          */
      }
      break;                // Clear CPUOFF bit from 0(SR)
  default: break;
 }
}
void uart_tx(char data){         // writes a byte to serial comms
    while (!(UCA0IFG & UCTXIFG));                // USCI_A0 TX buffer ready?
    UCA0TXBUF = data;
}
void uart_tx_string(char *string){   // writes a string to serial comms
    int length = strlen(string);
    while(length>0)
    {
        uart_tx(*string);
        length--;
        *string++;
    }
}
void uart_tx_number(long number){
    int digits = 0;
    char character[10] = {'0','1','2','3','4','5','6','7','8','9'};
    if(number == 0){
        uart_tx('0');   // if number = 0, go ahead and send 0
    }
    long reverse = 0;
    while(number > 0){
        reverse = reverse * 10 + (number % 10);
        number /= 10;
        digits++;
    }
    while(digits > 0){
        uart_tx(character[reverse % 10]);
        reverse /= 10;
        digits--;
    }
}
