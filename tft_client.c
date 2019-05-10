#include "bsp.h"
#include "mrfi.h"
#include "nwk_types.h"
#include "nwk_api.h"
#include "bsp_leds.h"
#include "bsp_buttons.h"
#include "app_remap_led.h"

static void linkTo(void);
void toggleLED(uint8_t);
static uint8_t sRxCallback(linkID_t);
static linkID_t sLinkID1 = 0;
uint8_t first_message = 0;
uint8_t player_color;
uint8_t time_over = 0;
uint8_t first_time = 1;
/* application Rx frame handler. */

#define SPIN_ABOUT_A_SECOND  NWK_DELAY(1000)

void main (void)
{
  BSP_Init();
  BCSCTL3 |= LFXT1S_2;            // VLOCLK
  BCSCTL1 = CALBC1_1MHZ;          // DCO 1MHZ
  DCOCTL = CALDCO_1MHZ;
  /* This call will fail because the join will fail since there is no Access Point
   * in this scenario. But we don't care -- just use the default link token later.
   * We supply a callback pointer to handle the message returned by the peer.
   */
  SMPL_Init(sRxCallback);
  /* wait for a button press... */
  do {
    if (BSP_BUTTON1() || BSP_BUTTON2())
    {
      break;
    }
  } while (1);

  /* never coming back... */
  linkTo();

  /* but in case we do... */
  while(1) ;
}

static void linkTo()
{
  volatile uint16_t i;
  toggleLED(1);
  toggleLED(2);
  while (SMPL_SUCCESS != SMPL_Link(&sLinkID1))
  {
    SPIN_ABOUT_A_SECOND;
  }

  SMPL_Ioctl( IOCTL_OBJ_RADIO, IOCTL_ACT_RADIO_RXON, 0);

  toggleLED(1);
  toggleLED(2);
  //waiting for color message
  while(!first_message){
      for(i=0;i<65000;i++);
      toggleLED(1);
  }


  /* player red = led2 */
  if(player_color == 1)
  {
    if(!BSP_LED1_IS_ON())
        toggleLED(1);
    if(BSP_LED2_IS_ON())
        toggleLED(2);
  } else if(player_color == 2){
      if(BSP_LED1_IS_ON())
          toggleLED(1);
      if(!BSP_LED2_IS_ON())
          toggleLED(2);
  } else {
      SPIN_ABOUT_A_SECOND;
  }

  TACCTL0 = CCIE;               // TACCR0 interrupt enabled
  TACCR0 = 24000;          // Timer count up to
  TACTL = TASSEL_1 + MC_1;      //  ACLK, UP_MODE
  _BIS_SR(LPM3_bits + GIE); // LPM3 (low voltage ACLK enabled) + general interrupt enable.
  //Show led color for 2 sec
  if(player_color == 1)
      toggleLED(1);
  else
      toggleLED(2);
  first_time = 0;

  while (1);
}


void toggleLED(uint8_t which)
{
  if (1 == which)
  {
    BSP_TOGGLE_LED1();
  }
  else if (2 == which)
  {
    BSP_TOGGLE_LED2();
  }
  return;
}

/* handle received frames. */
static uint8_t sRxCallback(linkID_t port)
{
  uint8_t msg[2],len;

  /* is the callback for the link ID we want to handle? */
  if (port == sLinkID1)
  {
    /* yes. go get the frame. we know this call will succeed. */
     if ((SMPL_SUCCESS == SMPL_Receive(sLinkID1, msg, &len)) && len)
     {
         if(!first_message){
             first_message=1;
             player_color=*msg;
         } else { // game started
             TA0R = 0;
             TACCR0 = 60000;          // Timer count up to
             TACTL = TASSEL_1 + MC_1;      //  ACLK, UP_MODE
             if(player_color == 1)
                   toggleLED(1);
               else
                   toggleLED(2);

             do {
               if (BSP_BUTTON1() || BSP_BUTTON2() || time_over)
               {
                 time_over=0;
                 TACTL &= ~MC_1;
                 break;
               }
             } while (1);

                if (player_color == 1)
                    toggleLED(1);
                else
                    toggleLED(2);

             *((uint16_t*)msg) = TA0R;
             SMPL_Send(sLinkID1, msg, sizeof(msg));
         }
     }
     return 1;
  }
  return 0;
}

#pragma vector=TIMERA0_VECTOR
__interrupt void Timer_A(void){
    if(!first_time)
        time_over=1;
    else {
        TACTL &= ~MC_1;
        LPM1_EXIT;
    }
}
