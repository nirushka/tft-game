#include <stdlib.h>
#include "bsp.h"
#include "mrfi.h"
#include "nwk_types.h"
#include "nwk_api.h"
#include "bsp_leds.h"
#include "bsp_buttons.h"
#include "app_remap_led.h"

#define NUM_CONNECTIONS  2
#define ROUND_TIMEOUT      5
#define ROUNDS_LIMIT        11

static void linkTo(void);
void toggleLED(uint8_t);

static uint8_t sRxCallback(linkID_t);   // application Rx frame handler.

void print(char msg[]);
void print_num(unsigned int num);
void print_float(double num);
int inArray(linkID_t who);            // find link in the array

// reserve space for the maximum possible peer Link IDs
static uint8_t  msgsReceived = 0;
static linkID_t sLinkIDs[NUM_CONNECTIONS] = {0};
static unsigned int wins[NUM_CONNECTIONS] = {0}, times[NUM_CONNECTIONS] = {0};
volatile uint8_t count=0;
int randNum=124;

void main(void) {
    BSP_Init();

    SMPL_Init(sRxCallback);

    /***************************** [ Timer & UART ] *****************************/
    BCSCTL1   = CALBC1_1MHZ;
    DCOCTL    = CALDCO_1MHZ;

    P3SEL    |= 0x30;
    UCA0CTL0  = 0;                      // flow=none; stop=1bit; parity=none; data=8bit
    UCA0CTL1  = UCSSEL0 + UCSSEL1;      // BRCLK
    UCA0BR0   = 104;                    // set Baud Rate to 9600 (using N=int(1.25))
    UCA0BR1   = 0;                      //    "" ---- ""
    UCA0MCTL  = 0x2;                    // set the UCA0MCTL according to the baud rate
    IFG2      = 0;
    IE2       = 0x1;                    // enable the RX

    UCA0CTL1 &= ~UCSWRST;               // disable software reset

    UCA0TXBUF = 0;                      // set value for the IFG2 to work

    BCSCTL3  |= LFXT1S_2;               // select 12kHz clock
    TACCTL0   = CCIE;                   // TACCR0 interrupt enabled

    TACCR0    = 40000;                  // ~ 8 sec
    TBCTL     = TBSSEL_1 + MC_1;

    print("\t\t===================================\r\n\tWelcome to The Fastest Thumb game!!\r\n\tPlease click on the button in order to see you color.\r\n\tYour mission as a player is to click on the button as fast as you can at the moment the LED turns on!\r\n\tThe player with the most wins will win the game.");

    /****************************************************************************/



    /* never coming back... */
    linkTo();

    /* but in case we do... */
    while(1);
}

static void linkTo() {
    uint8_t msg[2], numConnections=0;
    uint16_t i, max=0, winner;

    /* blink LEDs until we link successfully the players */
    while(numConnections < NUM_CONNECTIONS) {
        while(1) {
            if(SMPL_SUCCESS == SMPL_LinkListen(&sLinkIDs[numConnections])) {    //SMPL_Link()
                numConnections++;
                msg[0] = numConnections;

                // SMPL_Send(sLinkIDs[numConnections-1], msg, sizeof(msg));
                for(i=500; i>0; i--);   // delay before sending
                SMPL_Send(sLinkIDs[numConnections-1], msg, sizeof(msg));

                print("\t\t\tPlayer ");
                print_num(numConnections);
                print(" connected\r\n");
                break;
            }

            if(numConnections < 1) toggleLED(1);
            if(numConnections < 2) toggleLED(2);
        }
    }

    TACTL = TASSEL_1 + MC_3;         // ACLK, up-down mode -- start counting
    _BIS_SR(LPM1_bits + GIE);

    print("\t\t===================================\r\n\r\n");
    print("\t\t\tGet ready to play..\r\n\r\n");
    print("\t\t===================================\r\n\r\n");

    /* turn on RX. default is RX off. */
    SMPL_Ioctl(IOCTL_OBJ_RADIO, IOCTL_ACT_RADIO_RXON, 0);

    /* we're linked. turn off LEDs. */
    if(BSP_LED2_IS_ON()) {
        toggleLED(2);
    }
    if(BSP_LED1_IS_ON()) {
        toggleLED(1);
    }

    /* send activation messages */
    msg[0] = 0;     // message for all the connected devices
    msg[1] = 's';   // "start"
    while (count < ROUNDS_LIMIT) {
        for(i=NUM_CONNECTIONS; i>0; i--) {         // send to everyone
            SMPL_Send(sLinkIDs[i-1], msg, sizeof(msg));
        }
        count++;

        _BIS_SR(LPM1_bits + GIE);
    }

    for(i=NUM_CONNECTIONS; i>0; i--) {
        if(wins[i-1] > max) {
            max = wins[i-1];
            winner = i-1;
        }
    }
    print("\t\t===================================\r\n\r\n");
    print(winner == 0 ? "Green" : "Red");
    print(" wins the game!!\r\n");

    TACTL = MC_0 + TASSEL_2;    // stop the timer
    TA0R  = 0;

    while(1);
}


void toggleLED(uint8_t which) {
    if(1 == which) {
        BSP_TOGGLE_LED1();
    }
    else if(2 == which) {
        BSP_TOGGLE_LED2();
    }
    return;
}

/* handle received frames. */
static uint8_t sRxCallback(linkID_t port) {
    uint8_t msg[2], len, arrID, who;
    int i, min=65534;

    if ((arrID = inArray(port)) >= 0) {
        if ((SMPL_SUCCESS == SMPL_Receive(sLinkIDs[arrID], msg, &len)) && len) {
            times[arrID] = *((uint16_t *) msg); // keep times
            msgsReceived++;
        }

        if(msgsReceived == NUM_CONNECTIONS) {   // decide who the winner is
            TACTL = MC_0 + TASSEL_2;            // stop the timer
            TA0R  = 0;

            for(i=NUM_CONNECTIONS-1; i>=0; i--) {
                if(times[i] < min) {            // get the fastest player
                    min = times[i];
                    who = i;
                }
            }

            wins[who]++;                        // increment the wins

            print(who == 0 ? "Green" : "Red");
            print(" player wins the round!\r\nGreen's time: ");
            print_float((times[0]/12));
            print(" msec.  Red's time: ");
            print_float((times[1]/12));
            print(" msec.\r\n\r\n");


            // get ready for next round
            msgsReceived = 0;
            for(i=NUM_CONNECTIONS; i>0; i--) { times[i-1] = 0; }

            // set random time between messages
            srand(TBR);                         // re-seed
            randNum = rand();

            if(!(randNum % 5))      TACCR0 = 50000;
            else if(!(randNum % 4)) TACCR0 = 40000;
            else if(!(randNum % 3)) TACCR0 = 30000;
            else if(!(randNum % 2)) TACCR0 = 20000;
            else if(!(randNum % 1)) TACCR0 = 15000;

            TACTL  = TASSEL_1 + MC_3;          // start again
        }
    }

    return 0;
}

void print(char msg[]) {
    unsigned int j=0;

    for(j=0; msg[j] != '\0'; j++) {
        while(!(IFG2 & UCA0TXIFG));  // wait for the buffer to be cleared
        UCA0TXBUF = msg[j];
    }
}

void print_num(unsigned int num) {
    unsigned int idx=0, len=0, arr_num[5]={0};

    arr_num[0] = num;

    while(idx < 5) {
        while(arr_num[idx] > 9) {
            arr_num[idx] -= 10;
            arr_num[idx+1]++;
        }

        if(arr_num[idx] > 0) len = idx;
        idx++;
    }

    for(idx=len+1; idx>0; idx--) {
        while(!(IFG2 & UCA0TXIFG));  // wait for the buffer to be cleared
        UCA0TXBUF = arr_num[idx-1]+48;
    }
}

void print_float(double num) {
    unsigned int idx=0, len=0, arr_num[5]={0};

    arr_num[0] = num*10;

    while(idx < 5) {
        while(arr_num[idx] > 9) {
            arr_num[idx] -= 10;
            arr_num[idx+1]++;
        }

        if(arr_num[idx] > 0) len = idx;
        idx++;
    }

    for(idx=len+1; idx>0; idx--) {
        while(!(IFG2 & UCA0TXIFG));  // wait for the buffer to be cleared
        if(idx == 1) {
            UCA0TXBUF = 46;                 // "."
            while(!(IFG2 & UCA0TXIFG));     // wait for the buffer to be cleared
        }
        UCA0TXBUF = arr_num[idx-1]+48;
    }
}

int inArray(linkID_t who) {
    int8_t i;

    for(i=NUM_CONNECTIONS-1; i>=0; i--) {
        if(sLinkIDs[i] == who) return i;
    }

    return -1;
}

#pragma vector = TIMERA0_VECTOR
__interrupt void Timer_A(void) {
    int i;

    if(msgsReceived > 0) { // we've got a winner
        for(i=NUM_CONNECTIONS-1; i>=0; i--) {
            if(times[i] > 0) {
                wins[i]++;
                print(i == 0 ? "Green" : "Red");
                print(" player wins the round!\r\n");
                break;
            }
        }
    }

    // get ready for next round
    msgsReceived = 0;
    for(i=NUM_CONNECTIONS-1; i>=0; i--) { times[i] = 0; }

    LPM1_EXIT;    // stop sleeping
}

#pragma vector = USCIAB0RX_VECTOR
__interrupt void USCIAB0RX_ISR(void) {
    if(IFG2 && UCA0RXBUF) {
        UCA0TXBUF = UCA0RXBUF;
    }
}
