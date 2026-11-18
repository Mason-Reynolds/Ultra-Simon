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
#define NOTE_GOP_MS 20u
#define PWM_DIVIDER 64u
#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

//Defining the sound note structure
// Frequencies in millihertz retain tuning precision without floating-point notes.
typedef struct {
    uint32_t frequency_mhz;
    uint16_t duration_ms;
} sound_note_t;

//Defining the sound notes for each button and event
static const uint32_t button_notes[] = {130813, 146832, 164814, 174614};
static const sound_note_t start_notes[] = {
    {261626, 100}, {329628, 100}, {391995, 150},
    {329628, 100}, {261626, 250}
};
static const sound_note_t win_notes[] = {
    {261626, 100}, {329628, 100}, {391995, 100}, {523251, 300}
};
static const sound_note_t lose_notes[] = {
    {349228, 150}, {329628, 150}, {261626, 150}, {246942, 400}
};


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
static unsigned int audio_slice;
static const sound_note_t *melody;
static size_t melody_count;
static size_t note_index;
static bool in_gap;
static bool button_active;
static uint64_t deadline_us;

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
    gpio_init(SPK_GP);
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

static void mute(void)
{
    gpio_put(SPK_GP, false);
    gpio_set_function(SPK_GP, GPIO_FUNC_SIO);
    gpio_set_dir(SPK_GP, GPIO_OUT);
    pwm_set_enabled(audio_slice, false);
}

static void play_note(uint32_t frequency_mhz, uint16_t duration_ms)
{
    mute();
    uint64_t numerator = (uint64_t)clock_get_hz(clk_sys) * 1000u;
    uint64_t denominator = (uint64_t)PWM_DIVIDER * frequency_mhz;
    uint64_t period = (numerator + denominator / 2u) / denominator;
    if (period < 2u || period > 65536u) {
        return; // Unsupported clock/frequency combination: remain silent.
    }

    // Reinitialize while disconnected so old buffered compare/wrap values
    // cannot leak into the beginning of a new note.
    pwm_config config = pwm_get_default_config();
    pwm_config_set_clkdiv(&config, (float)PWM_DIVIDER);
    pwm_config_set_wrap(&config, (uint16_t)(period - 1u));
    pwm_init(audio_slice, &config, false);
    pwm_set_gpio_level(SPK_GP, (uint16_t)(period / 2u));
    gpio_set_function(SPK_GP, GPIO_FUNC_PWM);
    pwm_set_enabled(audio_slice, true);

    sleep_ms(duration_ms);
}

void sound_stop(void)
{
    if (!audio_slice) return;
    melody = NULL;
    melody_count = 0;
    note_index = 0;
    in_gap = false;
    button_active = false;
    mute();
}

void sound_button_start(uint8_t button)
{
    if (button >= ARRAY_COUNT(button_notes)) return;
    sound_stop();
    play_note(button_notes[button], NOTE_GOP_MS);
}

void sound_button-stop(uint8_t button)
{
    if (button >= ARRAY_COUNT(button_notes)) return;
    sound_stop();
}

static void play_melody(const sound_note_t *notes, size_t count)
{
    sound_stop();
    melody = notes;
    melody_count = count;
    note_index = 0;
    in_gap = false;
    button_active = false;
}

void sound_start(void) { play_melody(start_notes, ARRAY_COUNT(start_notes)); }
void sound_win(void) { play_melody(win_notes, ARRAY_COUNT(win_notes)); }
void sound_lose(void) { play_melody(lose_notes, ARRAY_COUNT(lose_notes)); }

void sound_update(void)
{
    if (!initialized || !melody) return;
    uint64_t now = time_us_64();
    if (now < deadline_us) return;

    if (!in_gap) {
        mute_output();
        if (note_index + 1u == melody_count) {
            sound_stop(); // No trailing gap after the final note.
            return;
        }
        in_gap = true;
        deadline_us = now + NOTE_GAP_MS * 1000u;
    } else {
        ++note_index;
        in_gap = false;
        play_frequency(melody[note_index].frequency_mhz);
        deadline_us = now + (uint64_t)melody[note_index].duration_ms * 1000u;
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
