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
char buf[32];
char buf2[32];

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
    tft_spi_init();
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

void ReactionGame(){
    uint8_t score = 0; // The number of rounds completed
    uint8_t type = 1; // If multiple game modes are used, this can designate them.
    uint64_t fastestTime = 500; // Keep track of the fastest time, might end up being stored
    uint64_t slowestTime = 0; // Keep track of the slowest time, might end up being stored
    uint8_t button; // Which color lights up?
    uint64_t startTime = 0; //Time when the light lights
    uint64_t endTime = 0; // Time when the button buttons
    uint64_t totalTime = 0; // Difference between the two

    uint64_t ScoreTime = 500; // max allowed time to score in ms
    bool light = false;
    bool isPlaying = false;
    
    // TODO: Add audio
    
    while(isPlaying){
        if (type = 1){

        //General game loop:
        //Leadup
            sleep_ms(1000);
            drawText(10, 60, "5", ST7735_BLUE, ST7735_BLACK, 2);
            sleep_ms(1000);
            drawText(10, 60, "4", ST7735_GREEN, ST7735_BLACK, 2);
            sleep_ms(1000);
            drawText(10, 60, "3", ST7735_RED, ST7735_BLACK, 2);
            sleep_ms(1000);
            drawText(10, 60, "2", ST7735_YELLOW, ST7735_BLACK, 2);
            sleep_ms(1000);
            drawText(10, 60, "1", ST7735_WHITE, ST7735_BLACK, 2);
            sleep_ms(1000);
        // execute the rest:
            button = (rand() % 3) + 1;
            startTime = time_us_64();
            totalTime = 0;
            endTime = 0;

            while(!light){
            //Light the right color, and poll for any buttons going down
            //If the right button is pressed, wait for the buttons to go up
            //Otherwise, if the wrong button is pressed, fail
            //Final option: If the time is greater than 500, fail
                switch(button){
                case 1:
                    gpio_put(Bl_GP, 1);
                    gpio_put(Gl_GP, 0);
                    gpio_put(Rl_GP, 0);
                    gpio_put(Yl_GP, 0);
                    if(gpio_get(BB_GP)){
                    //Sucess
                        endTime = time_us_64();
                        light = true;
                    }
                    break;
                case 2:
                    gpio_put(Gl_GP, 1);
                    gpio_put(Bl_GP, 0);
                    gpio_put(Yl_GP, 0);
                    gpio_put(Rl_GP, 0);
                    if(gpio_get(GB_GP)){
                    // Sucess
                        endTime = time_us_64();
                        light = true;
                    }
                    break;
                case 3:
                    gpio_put(Rl_GP, 1);
                    gpio_put(Bl_GP, 0);
                    gpio_put(Gl_GP, 0);
                    gpio_put(Yl_GP, 0);
                    if(gpio_get(Rb_GP)){
                        // Sucess
                        endTime = time_us_64();
                        light = true;
                    }
                    break;
                case 4:
                    gpio_put(Yl_GP, 1);
                    gpio_put(Bl_GP, 0);
                    gpio_put(Gl_GP, 0);
                    gpio_put(Rl_GP, 0);
                    if(gpio_get(YB_GP)){
                        // Sucess
                        endTime = time_us_64();
                        light = true;
                    }
                    break;
                default:
                    gpio_put(Gl_GP, 0);
                    gpio_put(Bl_GP, 0);
                    gpio_put(Yl_GP, 0);
                    gpio_put(Rl_GP, 0);
                    break;
                }
                if (gpio_get(BB_GP) || gpio_get(GB_GP) || gpio_get(YB_GP) || gpio_get(Rb_GP))
                {
                    //Fail
                    light = true;
                    endTime = startTime + 501;
                }

                //Upon sucess, increment score
                //
            }
            while (gpio_get(BB_GP) || gpio_get(GB_GP) || gpio_get(YB_GP) || gpio_get(Rb_GP)){}
            totalTime = endTime - startTime;
            if(totalTime <= ScoreTime){
                sprintf(buf, "Time: %d", totalTime);
                score ++;
                sprintf(buf2, "Score: %d", score);
                if(totalTime > slowestTime){
                    slowestTime = totalTime;
                }
                if(totalTime < fastestTime){
                    fastestTime = totalTime;
                }

                //Actual display
                drawText(10, 60, buf, ST7735_WHITE, ST7735_BLACK, 2);
                sleep_ms(1000);
                drawText(10, 100, buf2, ST7735_WHITE, ST7735_BLACK, 2);
                sleep_ms(4000);
            }
            else{
                score = 501;
                //Shouldn't be able to reach this in game, used since uint doesn't support negative numbers
            }

            if(score <=100 && score != 501){
                //Display a win screen
                drawText(10,60,"Congradulations", ST7735_WHITE, ST7735_BLACK, 2);
                drawText(40,80, "You Win!", ST7735_WHITE, ST7735_BLACK, 2);

                sprintf(buf, "Fastest Reaction: %d", fastestTime);
                sprintf(buf2, "Slowest Reaction: %d", slowestTime);
                
                drawText(10, 100, buf, ST7735_WHITE, ST7735_BLACK, 1);
                drawText(10, 140, buf2, ST7735_WHITE, ST7735_BLACK, 1);
                
                isPlaying = false;
                //Go back to main menu
            }
            else if (score == 501){
                drawText(10, 60, "Too Bad", ST7735_WHITE, ST7735_BLACK, 2);
                drawText(40, 80, "You Lost!", ST7735_WHITE, ST7735_BLACK, 2);

                sprintf(buf, "Fastest Reaction: %d", fastestTime);
                sprintf(buf2, "Slowest Reaction: %d", slowestTime);

                drawText(10, 100, buf, ST7735_WHITE, ST7735_BLACK, 1);
                drawText(10, 140, buf2, ST7735_WHITE, ST7735_BLACK, 1);

                isPlaying = false;
            }
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
    TFT_BlackTab_Initialize();
    setTextWrap(true);
    fillScreen(ST7735_BLACK);
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
        if(gpio_get(GB_GP){
            ReactionGame();
        }
    }
}
