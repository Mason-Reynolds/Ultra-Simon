#include <stdio.h>
#include <stdlib.h>
#include "pico/stdlib.h"
#include "ST7735_TFT.h"
#include "hardware/spi.h"
#include "hardware/adc.h"
#include "hardware/timer.h"
#include "math.h"

//definitions of the GPIO pins for each feature

#define BB_GP 28
#define RB_GP 12
#define YB_GP 15
#define GB_GP 22
#define BL_GP 27
#define RL_GP 13
#define YL_GP 14
#define GL_GP 26
#define RST_GP 16
#define TRST_GP 17
#define TCS_GP 18
#define TDC_GP 19
#define TDIN_GP 20
#define TCLK_GP 21
#define SPK_GP 0

//important values

volatile uint64_t fallTim = 0;
volatile uint64_t riseTim = 0;
volatile uint64_t totalTim = 0;
volatile bool started = false;
volatile bool received = false;
volatile bool ready = false;
volatile bool wrong = false;
volatile int currSeq[50] = {0}; //store the sequence for a game
static int buttonsGp[4] = {12, 28, 22, 15}; //rgby order
static int lightsGp[4] = {13, 27, 26, 14}; //rgby order
static char namesGp[4] = {'R', 'G', 'B', 'Y'};

static void init(){
    gpio_init(BL_GP);
    gpio_init(RL_GP);
    gpio_init(GL_GP);
    gpio_init(YL_GP);
    gpio_init(BB_GP);
    gpio_init(RB_GP);
    gpio_init(GB_GP);
    gpio_init(YB_GP);
    gpio_init(RST_GP);
    gpio_set_dir(BL_GP, GPIO_OUT);
    gpio_set_dir(RL_GP, GPIO_OUT);
    gpio_set_dir(GL_GP, GPIO_OUT);
    gpio_set_dir(YL_GP, GPIO_OUT);
    gpio_set_dir(RST_GP, GPIO_IN);
    gpio_set_dir(BB_GP, GPIO_IN);
    gpio_set_dir(RB_GP, GPIO_IN);
    gpio_set_dir(GB_GP, GPIO_IN);
    gpio_set_dir(YB_GP, GPIO_IN);
    gpio_pull_down(BB_GP);
    gpio_pull_down(RB_GP);
    gpio_pull_down(YB_GP);
    gpio_pull_down(GB_GP);
    gpio_pull_down(RST_GP);
}

void ButtonTest() {
//if a button is down, light up the LED 
    if(gpio_get(BB_GP))
    {
        gpio_put(BL_GP, 1);
        gpio_put(GL_GP, 0);
        gpio_put(RL_GP, 0);
        gpio_put(YL_GP, 0);
    }
    if(gpio_get(GB_GP))
    {
        gpio_put(GL_GP, 1);
        gpio_put(BL_GP, 0);
        gpio_put(YL_GP, 0);
        gpio_put(RL_GP, 0);
    }
    if(gpio_get(RB_GP))
    {
        gpio_put(RL_GP, 1);
        gpio_put(BL_GP, 0);
        gpio_put(GL_GP, 0);
        gpio_put(YL_GP, 0);
    }
    if(gpio_get(YB_GP))
    {
        gpio_put(GL_GP, 0);
        gpio_put(BL_GP, 0);
        gpio_put(YL_GP, 1);
        gpio_put(RL_GP, 0);
    }
    if(gpio_get(RST_GP))
    {
        gpio_put(GL_GP, 0);
        gpio_put(BL_GP, 0);
        gpio_put(YL_GP, 0);
        gpio_put(RL_GP, 0);
    }
}


//Include menu structure and graphics here




//Include the main Simon game here
void mainSimon(){
    //print "SIMON"
    for (int k = 0; k < 50; ++k){
        currSeq[k] = rand() % 4; //fill the sequence queue with 50 inputs selected randomly from the GPIO array
    }

    int round;

    for (round = 1; round <= 50; ++round){
        //simon says...
        for (int i = 0; i < round; ++i){
            //print namesGp[currSeq[i]] to the screen in the color it represents, overwrite after every print, sleep for .5 sec
            //also, play the button's representative sound and flash its corresponding LED
            if(currSeq[i] == 0){
                gpio_put(BL_GP, 1);
                sleep_ms(1000);
                gpio_put(BL_GP, 0);
                sleep_ms(1000);
            }
            else if(currSeq[i] == 1){
                gpio_put(GL_GP, 1);
                sleep_ms(1000);
                gpio_put(GL_GP, 0);
                sleep_ms(1000);
            }
            else if(currSeq[i] == 2){
                gpio_put(RL_GP, 1);
                sleep_ms(1000);
                gpio_put(RL_GP, 0);
                sleep_ms(1000);
            }
            else if(currSeq[i] == 3){
                gpio_put(YL_GP, 1);
                sleep_ms(1000);
                gpio_put(YL_GP, 0);
                sleep_ms(1000);
            }
        }

        for (int i = 0; i < round; ++i) {
            //print "enter sequence."

            while(!gpio_get(BB_GP) && !gpio_get(RB_GP) && !gpio_get(GB_GP) && !gpio_get(YB_GP)){
                //tight_loop_contents();
            }

            if(gpio_get(BB_GP))
            {
                if(currSeq[i] == 0){}
                else{
                    wrong = true;
                    //break;
                }                
            }
            if(gpio_get(GB_GP))
            {
                if(currSeq[i] == 1){}
                else{
                    wrong = true;
                    //break;
                }
            }
            if(gpio_get(RB_GP))
            {
                if(currSeq[i] == 2){}
                else{
                    wrong = true;
                    //break;
                }
            }
            if(gpio_get(YB_GP))
            {
                if(currSeq[i] == 3){}
                else{
                    wrong = true;
                    //break;
                }
            }
            if(wrong == true){
                break;
            }

            sleep_ms(250);
            /*for (int k = 0; k < 50; ++k){
                gpio_put(BL_GP, 1);
                sleep_ms(10);
                gpio_put(BL_GP, 0);
                sleep_ms(10);
            }*/
        }
        if(wrong == true){
            //print "you just lost the game"
            for (int i = 0; i < round; ++i){
                //print the characters (namesGp[currSeq[i]])
            }
            gpio_put(BL_GP, 1);
            gpio_put(RL_GP, 1);
            gpio_put(GL_GP, 1);
            gpio_put(YL_GP, 1);
            sleep_ms(1000);
            gpio_put(BL_GP, 0);
            gpio_put(RL_GP, 0);
            gpio_put(GL_GP, 0);
            gpio_put(YL_GP, 0);
            wrong = false;
            break;
        }
        //print round
        if(round == 50){
            //print "you win"
        }
    }
}






//Include the reaction time game here
void timinSimon(){
    //print "Timin' Simon"
    for (int k = 0; k < 25; ++k){
        currSeq[k] = rand() % 3; //fill the sequence queue with 50 randomly selected time lengths from a set of 3
    }

    int round;

    for (round = 1; round <= 25; ++round){
        //timin' simon says...
        for (int i = 0; i < round; ++i){
            //print namesGp[currSeq[i]] to the screen in the color it represents, overwrite after every print, sleep for .5 sec
            //also, play the button's representative sound and flash its corresponding LED
            if(currSeq[i] == 0){
                gpio_put(YL_GP, 1);
                sleep_ms(500);
                gpio_put(YL_GP, 0);
                sleep_ms(1000);
            }
            else if(currSeq[i] == 1){
                gpio_put(YL_GP, 1);
                sleep_ms(1000);
                gpio_put(YL_GP, 0);
                sleep_ms(1000);
            }
            else if(currSeq[i] == 2){
                gpio_put(YL_GP, 1);
                sleep_ms(1500);
                gpio_put(YL_GP, 0);
                sleep_ms(1000);
            }
        }

        for (int i = 0; i < round; ++i) {
            //print "enter sequence."

            while(!gpio_get(YB_GP)){}

            if (gpio_get(YB_GP)){
                riseTim = time_us_64();
                sleep_ms(10);
            }
            while (gpio_get(YB_GP)){}
            fallTim = time_us_64();

            totalTim = fallTim-riseTim;
            
            if(totalTim > 250000 && totalTim < 750000 && currSeq[i] == 0){
            }
            else if (totalTim > 750000 && totalTim < 1250000 && currSeq[i] == 1){
            }
            else if (totalTim > 1250000 && totalTim < 1750000 && currSeq[i] == 2){
            }
            else{wrong = true;}

            /*if(gpio_get(BB_GP))
            {
                if(currSeq[i] == 0){}
                else{
                    wrong = true;
                    //break;
                }                
            }
            if(gpio_get(GB_GP))
            {
                if(currSeq[i] == 1){}
                else{
                    wrong = true;
                    //break;
                }
            }
            if(gpio_get(RB_GP))
            {
                if(currSeq[i] == 2){}
                else{
                    wrong = true;
                    //break;
                }
            }
            if(wrong == true){
                break;
            }*/

            sleep_ms(250);
            /*for (int k = 0; k < 50; ++k){
                gpio_put(BL_GP, 1);
                sleep_ms(10);
                gpio_put(BL_GP, 0);
                sleep_ms(10);
            }*/
        }
        if(wrong == true){
            //print "you just lost the game"
            for (int i = 0; i < round; ++i){
                //print the characters (namesGp[currSeq[i]])
            }
            gpio_put(BL_GP, 1);
            gpio_put(RL_GP, 1);
            gpio_put(GL_GP, 1);
            gpio_put(YL_GP, 1);
            sleep_ms(1000);
            gpio_put(BL_GP, 0);
            gpio_put(RL_GP, 0);
            gpio_put(GL_GP, 0);
            gpio_put(YL_GP, 0);
            wrong = false;
            break;
        }
        //print round
        if(round == 50){
            //print "you win"
        }
    }
}

//Include the button timing game here
void lengthPress(){
    int timelength = 0;
    (void)timelength;
}

void stateReset(){
    wrong = false;
    for (int i = 0; i < 50; ++i){
        currSeq[i] = 0;
    }
}

//Include extra code for score saving and extra features here

int main()
{
    stdio_init_all();

    init();

    while (true) {
        if (gpio_get(YB_GP)){
            mainSimon();
        }
        if (gpio_get(RB_GP)){
            timinSimon();
        }
        if(gpio_get(BB_GP)){
            ButtonTest();
        }
    }
}
