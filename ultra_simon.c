#include <stdio.h>
#include <stdlib.h>
#include "ST7735_TFT.h"
#include "hardware/spi.h"
#include "hardware/adc.h"
#include "hardware/timer.h"
#include "hardware/pwm.h"
#include "hardware/clocks.h"
#include "math.h"
#include "hw.h"

// definitions of the GPIO pins for each feature

#define BB_GP 28
#define RB_GP 4
#define YB_GP 15
#define GB_GP 22
#define BL_GP 27
#define RL_GP 5
#define YL_GP 14
#define GL_GP 26
#define RST_GP 16
#define TRST_GP 12
#define TCS_GP 9
#define TDC_GP 13
#define TDIN_GP 11
#define TCLK_GP 10
#define SPK_GP 0
#define NOTE_GAP_MS 20u
#define PWM_DIVIDER 64u
#define ARRAY_COUNT(a) (sizeof(a) / sizeof((a)[0]))

// Defining the sound note structure
//  Frequencies in millihertz retain tuning precision without floating-point notes.
typedef struct
{
    uint32_t frequency_mhz;
    uint16_t duration_ms;
} sound_note_t;

// Defining the sound notes for each button and event
static const uint32_t button_notes[] = {130813, 146832, 164814, 174614};
static const sound_note_t start_notes[] = {
    {261626, 100}, {329628, 100}, {391995, 150}, {329628, 100}, {261626, 250}};
static const sound_note_t win_notes[] = {
    {261626, 100}, {329628, 100}, {391995, 100}, {523251, 300}};
static const sound_note_t lose_notes[] = {
    {349228, 150}, {329628, 150}, {261626, 150}, {246942, 400}};

// important values

volatile uint64_t fallTim = 0;
volatile uint64_t riseTim = 0;
volatile uint64_t totalTim = 0;
volatile int SHigh = 0;
volatile int THigh = 0;
volatile int RHigh = 0;
volatile bool started = false;
volatile bool received = false;
volatile bool ready = false;
volatile bool wrong = false;
volatile bool light = false;
volatile bool secondMessage = false;
volatile int currSeq[50] = {0};                         // store the sequence for a game
static int buttonsGp[4] = {RB_GP, GB_GP, BB_GP, YB_GP}; // rgby order
static int lightsGp[4] = {RL_GP, GL_GP, BL_GP, YL_GP};  // rgby order
static char namesGp[4] = {'R', 'G', 'B', 'Y'};
char buf[32];
char buf2[32];
static unsigned int audio_slice;
static const sound_note_t *melody;
static size_t melody_count;
static size_t note_index;
static bool in_gap;
static bool button_active;
static uint64_t deadline_us;

static void init()
{
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
    gpio_init(SPK_GP);
    gpio_set_dir(SPK_GP, GPIO_OUT);
    gpio_put(SPK_GP, false);
    audio_slice = pwm_gpio_to_slice_num(SPK_GP);
    spi_init(spi1, 1000 * 1000);
    gpio_set_function(TCLK_GP, GPIO_FUNC_SPI);
    gpio_set_function(TDIN_GP, GPIO_FUNC_SPI);
    tft_spi_init();
}

void ButtonTest()
{
    // if a button is down, light up the LED
    if (gpio_get(BB_GP))
    {
        gpio_put(BL_GP, 1);
        gpio_put(GL_GP, 0);
        gpio_put(RL_GP, 0);
        gpio_put(YL_GP, 0);
    }
    if (gpio_get(GB_GP))
    {
        gpio_put(GL_GP, 1);
        gpio_put(BL_GP, 0);
        gpio_put(YL_GP, 0);
        gpio_put(RL_GP, 0);
    }
    if (gpio_get(RB_GP))
    {
        gpio_put(RL_GP, 1);
        gpio_put(BL_GP, 0);
        gpio_put(GL_GP, 0);
        gpio_put(YL_GP, 0);
    }
    if (gpio_get(YB_GP))
    {
        gpio_put(GL_GP, 0);
        gpio_put(BL_GP, 0);
        gpio_put(YL_GP, 1);
        gpio_put(RL_GP, 0);
    }
    if (gpio_get(RST_GP))
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
    pwm_set_enabled(audio_slice, false);
}

static void start_tone(uint32_t frequency_mhz)
{
    mute();
    uint64_t numerator = (uint64_t)clock_get_hz(clk_sys) * 1000u;
    uint64_t denominator = (uint64_t)PWM_DIVIDER * frequency_mhz;
    uint64_t period = (numerator + denominator / 2u) / denominator;
    if (period < 2u || period > 65536u)
    {
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
}

static void play_note(uint32_t frequency_mhz, uint16_t duration_ms)
{
    start_tone(frequency_mhz);
    sleep_ms(duration_ms);
    mute();
}

void sound_stop(void)
{
    if (!audio_slice)
        return;
    melody = NULL;
    melody_count = 0;
    note_index = 0;
    in_gap = false;
    button_active = false;
    mute();
}

void sound_button_start(uint8_t button)
{
    if (button >= ARRAY_COUNT(button_notes))
        return;
    sound_stop();
    play_note(button_notes[button], NOTE_GAP_MS);
}

void sound_button_stop(uint8_t button)
{
    if (button >= ARRAY_COUNT(button_notes))
        return;
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
    deadline_us = 0;
}

void sound_start(void) { play_melody(start_notes, ARRAY_COUNT(start_notes)); }
void sound_win(void) { play_melody(win_notes, ARRAY_COUNT(win_notes)); }
void sound_lose(void) { play_melody(lose_notes, ARRAY_COUNT(lose_notes)); }

void sound_update(void)
{
    if (!melody)
        return;
    uint64_t now = time_us_64();
    if (now < deadline_us)
        return;

    if (in_gap)
    {
        ++note_index;
        if (note_index >= melody_count)
        {
            sound_stop();
            return;
        }
        start_tone(melody[note_index].frequency_mhz);
        in_gap = false;
        deadline_us = now + (uint64_t)melody[note_index].duration_ms * 1000u;
        return;
    }

    if (note_index == 0 && deadline_us == 0)
    {
        start_tone(melody[note_index].frequency_mhz);
        deadline_us = now + (uint64_t)melody[note_index].duration_ms * 1000u;
        return;
    }

    mute();
    if (note_index + 1u >= melody_count)
    {
        sound_stop();
        return;
    }
    in_gap = true;
    deadline_us = now + NOTE_GAP_MS * 1000u;
}



// Include the main Simon game here
void mainSimon()
{
    drawText(5, 10, "Welcome To", ST7735_WHITE, ST7735_BLACK, 2);
    sleep_ms(500);
    drawText(5, 30, "SIMON", ST7735_CYAN, ST7735_BLACK, 2);
    sleep_ms(100);
    drawText(5, 50, "COWELL!", ST7735_CYAN, ST7735_BLACK, 2);
    sleep_ms(500);
    drawText(5, 70, "Press any button to play.", ST7735_MAGENTA, ST7735_BLACK, 1);

    while (!gpio_get(BB_GP) && !gpio_get(RB_GP) && !gpio_get(GB_GP) && !gpio_get(YB_GP)){}

    fillScreen(ST7735_BLACK);

    for (int k = 0; k < 50; ++k)
    {
        currSeq[k] = rand() % 4; // fill the sequence queue with 50 inputs selected randomly from the GPIO array
    }

    int round;

    for (round = 1; round <= 50; ++round)
    {
        drawText(5, 10, "Simon Cowell Says...", ST7735_MAGENTA, ST7735_BLACK, 1);
        sleep_ms(800);

        for (int i = 0; i < round; ++i)
        {
            // print namesGp[currSeq[i]] to the screen in the color it represents, overwrite after every print, sleep for .5 sec
            // also, play the button's representative sound and flash its corresponding LED
            if (currSeq[i] == 0)
            {
                gpio_put(BL_GP, 1);
                drawText(5, 50, "BLUE", ST7735_BLUE, ST7735_BLACK, 2);
                sleep_ms(800);
                gpio_put(BL_GP, 0);
                fillScreen(ST7735_BLACK);
                sleep_ms(500);
            }
            else if (currSeq[i] == 1)
            {
                gpio_put(GL_GP, 1);
                drawText(5, 50, "GREEN", ST7735_GREEN, ST7735_BLACK, 2);
                sleep_ms(500);
                gpio_put(GL_GP, 0);
                fillScreen(ST7735_BLACK);
                sleep_ms(500);
            }
            else if (currSeq[i] == 2)
            {
                gpio_put(RL_GP, 1);
                drawText(5, 50, "RED", ST7735_RED, ST7735_BLACK, 2);
                sleep_ms(500);
                gpio_put(RL_GP, 0);
                fillScreen(ST7735_BLACK);
                sleep_ms(500);
            }
            else if (currSeq[i] == 3)
            {
                gpio_put(YL_GP, 1);
                drawText(5, 50, "YELLAH", ST7735_YELLOW, ST7735_BLACK, 2);
                sleep_ms(500);
                gpio_put(YL_GP, 0);
                fillScreen(ST7735_BLACK);
                sleep_ms(500);
            }
        }

        for (int i = 0; i < round; ++i)
        {
            drawText(5, 50, "Your Turn:", ST7735_CYAN, ST7735_BLACK, 2);
            while (!gpio_get(BB_GP) && !gpio_get(RB_GP) && !gpio_get(GB_GP) && !gpio_get(YB_GP) && !gpio_get(RST_GP)){}

            if (gpio_get(RST_GP)){
                if(secondMessage == false){
                    fillScreen(ST7735_BLACK);
                    drawText(5, 50, "Bye!", ST7735_WHITE, ST7735_BLACK, 2);
                    sleep_ms(2000);
                    fillScreen(ST7735_BLACK);
                    sleep_ms(10000);
                    drawText(5, 50, "What?", ST7735_WHITE, ST7735_BLACK, 1);
                    sleep_ms(3000);
                    fillScreen(ST7735_BLACK);
                    drawText(5, 50, "What do you want?", ST7735_WHITE, ST7735_BLACK, 1);
                    sleep_ms(3000);
                    fillScreen(ST7735_BLACK);
                    sleep_ms(5000);
                    drawText(5, 50, "You're still here?", ST7735_WHITE, ST7735_BLACK, 1);
                    sleep_ms(3000);
                    fillScreen(ST7735_BLACK);
                    sleep_ms(2000);
                    drawText(5, 50, "You know this is", ST7735_WHITE, ST7735_BLACK, 1);
                    drawText(5, 60, "your fault, right?", ST7735_WHITE, ST7735_BLACK, 1);
                    sleep_ms(5000);
                    fillScreen(ST7735_BLACK);
                    sleep_ms(3000);
                    drawText(5, 50, "Ok, fine.", ST7735_WHITE, ST7735_BLACK, 1);
                    sleep_ms(1500);
                    fillScreen(ST7735_BLACK);
                    drawText(5, 50, "But you owe me.", ST7735_WHITE, ST7735_BLACK, 1);
                    sleep_ms(4000);
                    fillScreen(ST7735_BLACK);
                    secondMessage = true;
                    return;
                }
                if(secondMessage == true){
                    fillScreen(ST7735_BLACK);
                    drawText(5, 50, "Oh, come on.", ST7735_WHITE, ST7735_BLACK, 1);
                    sleep_ms(3000);
                    fillScreen(ST7735_BLACK);
                    sleep_ms(2000);
                    drawText(5, 50, "You're not very nice.", ST7735_WHITE, ST7735_BLACK, 1);
                    sleep_ms(3000);
                    fillScreen(ST7735_BLUE);
                    drawText(5, 50, "FatalError", ST7735_YELLOW, ST7735_BLUE, 2);
                    sleep_ms(10000);
                    return;
                }
                
            }

            if (gpio_get(BB_GP))
            {
                if (currSeq[i] == 0)
                {
                }
                else
                {
                    wrong = true;
                    // break;
                }
            }
            if (gpio_get(GB_GP))
            {
                if (currSeq[i] == 1)
                {
                }
                else
                {
                    wrong = true;
                    // break;
                }
            }
            if (gpio_get(RB_GP))
            {
                if (currSeq[i] == 2)
                {
                }
                else
                {
                    wrong = true;
                    // break;
                }
            }
            if (gpio_get(YB_GP))
            {
                if (currSeq[i] == 3)
                {
                }
                else
                {
                    wrong = true;
                    // break;
                }
            }
            if (wrong == true)
            {
                break;
            }

            sleep_ms(250);
        }
        if (wrong == true)
        {
            fillScreen(ST7735_BLACK);
            drawText(5, 10, "You look a little", ST7735_RED, ST7735_BLACK, 1);
            drawText(5, 20, "odd, your dancing is", ST7735_RED, ST7735_BLACK, 1);
            drawText(5, 30, "terrible, the sing-", ST7735_RED, ST7735_BLACK, 1);
            drawText(5, 40, "ing was horrendous,", ST7735_RED, ST7735_BLACK, 1);
            drawText(5, 50, "and you look like", ST7735_RED, ST7735_BLACK, 1);
            drawText(5, 60, "one of those", ST7735_RED, ST7735_BLACK, 1);
            drawText(5, 70, "creatures that live", ST7735_RED, ST7735_BLACK, 1);
            drawText(5, 80, "in the jungle with", ST7735_RED, ST7735_BLACK, 1);
            drawText(5, 90, "the massive eyes.", ST7735_RED, ST7735_BLACK, 1);
            drawText(15, 110, "-Simon Cowell", ST7735_RED, ST7735_BLACK, 1);

            round -=1;

            sprintf(buf, "Score: %d", round);

            drawText(5, 130, buf, ST7735_CYAN, ST7735_BLACK, 1);

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
            

            while (!gpio_get(BB_GP) && !gpio_get(RB_GP) && !gpio_get(GB_GP) && !gpio_get(YB_GP)){}
            fillScreen(ST7735_BLACK);
            if(round-1 > SHigh){
                SHigh = round-1;
            }
            break;
        }

        else if(wrong != true){
            drawText(5, 50, "Good, Next", ST7735_MAGENTA, ST7735_BLACK, 2);
            sleep_ms(1000);
            fillScreen(ST7735_BLACK);
        }
        // print round
        if (round == 50)
        {
            drawText(5, 50, "Good Job, You're a Wizard!", ST7735_CYAN, ST7735_BLACK, 1);
            sleep_ms(1000);
            fillScreen(ST7735_BLACK);
            SHigh = 50;
            return;
        }
    }
}

// Include the reaction time game here
void timinSimon()
{
    drawText(5, 10, "Welcome To", ST7735_WHITE, ST7735_BLACK, 2);
    sleep_ms(500);
    drawText(5, 30, "EDUARDO", ST7735_CYAN, ST7735_BLACK, 2);
    sleep_ms(100);
    drawText(5, 50, "BROS!", ST7735_CYAN, ST7735_BLACK, 2);
    sleep_ms(500);
    drawText(5, 70, "Press any button to play.", ST7735_MAGENTA, ST7735_BLACK, 1);

    while (!gpio_get(BB_GP) && !gpio_get(RB_GP) && !gpio_get(GB_GP) && !gpio_get(YB_GP)){}
    
    for (int k = 0; k < 25; ++k)
    {
        currSeq[k] = rand() % 3; // fill the sequence queue with 50 randomly selected time lengths from a set of 3
    }

    int round;

    for (round = 1; round <= 25; ++round)
    {
        fillScreen(ST7735_BLACK);
        drawText(5, 50, "Listen...", ST7735_YELLOW, ST7735_BLACK, 2);

        for (int i = 0; i < round; ++i)
        {
            // print namesGp[currSeq[i]] to the screen in the color it represents, overwrite after every print, sleep for .5 sec
            // also, play the button's representative sound and flash its corresponding LED
            if (currSeq[i] == 0)
            {
                gpio_put(YL_GP, 1);
                sleep_ms(500);
                gpio_put(YL_GP, 0);
                sleep_ms(800);
            }
            else if (currSeq[i] == 1)
            {
                gpio_put(YL_GP, 1);
                sleep_ms(1000);
                gpio_put(YL_GP, 0);
                sleep_ms(800);
            }
            else if (currSeq[i] == 2)
            {
                gpio_put(YL_GP, 1);
                sleep_ms(1500);
                gpio_put(YL_GP, 0);
                sleep_ms(800);
            }
        }

        fillScreen(ST7735_BLACK);
        drawText(5, 50, "Press the", ST7735_WHITE, ST7735_BLACK, 1);
        drawText(5, 60, "Yellah", ST7735_YELLOW, ST7735_BLACK, 2);
        drawText(5, 80, "Button in the same", ST7735_WHITE, ST7735_BLACK, 1);
        drawText(5, 90, "combination of lengths.", ST7735_WHITE, ST7735_BLACK, 1);

        for (int i = 0; i < round; ++i)
        {

            while (!gpio_get(YB_GP) && !gpio_get(RST_GP))
            {
            }

            if (gpio_get(RST_GP)){
                fillScreen(ST7735_BLACK);
                drawText(5, 50, "Reset Button:", ST7735_CYAN, ST7735_BLACK, 1);
                drawText(5, 60, "No BS Edition", ST7735_CYAN, ST7735_BLACK, 1);
                sleep_ms(1000);
                fillScreen(ST7735_BLACK);
                return;
            }

            if (gpio_get(YB_GP))
            {
                riseTim = time_us_64();
                sleep_ms(10);
            }
            while (gpio_get(YB_GP))
            {
            }
            fallTim = time_us_64();

            totalTim = fallTim - riseTim;

            if (totalTim > 250000 && totalTim < 750000 && currSeq[i] == 0)
            {
            }
            else if (totalTim > 750000 && totalTim < 1250000 && currSeq[i] == 1)
            {
            }
            else if (totalTim > 1250000 && totalTim < 1750000 && currSeq[i] == 2)
            {
            }
            else
            {
                wrong = true;
            }

            sleep_ms(250);
        }
        if (wrong == true)
        {
            fillScreen(ST7735_BLACK);
            drawText(5, 10, "Oh, Robert, I'm", ST7735_RED, ST7735_BLACK, 1);
            drawText(5, 20, "afraid you just", ST7735_RED, ST7735_BLACK, 1);
            drawText(5, 30, "killed my favorite", ST7735_RED, ST7735_BLACK, 1);
            drawText(5, 40, "song of all time.", ST7735_RED, ST7735_BLACK, 1);
            drawText(15, 60, "-Simon Cowell", ST7735_RED, ST7735_BLACK, 1);

            round -=1;

            sprintf(buf, "Score: %d", round);

            drawText(5, 130, buf, ST7735_CYAN, ST7735_BLACK, 1);

            gpio_put(BL_GP, 1);
            gpio_put(RL_GP, 1);
            gpio_put(GL_GP, 1);
            gpio_put(YL_GP, 1);
            sleep_ms(1000);
            gpio_put(BL_GP, 0);
            gpio_put(RL_GP, 0);
            gpio_put(GL_GP, 0);
            gpio_put(YL_GP, 0);

            while (!gpio_get(BB_GP) && !gpio_get(RB_GP) && !gpio_get(GB_GP) && !gpio_get(YB_GP)){}
            if(round-1 > THigh){
                THigh = round-1;
            }
            wrong = false;
            break;
        }
        else if(wrong == false){
            fillScreen(ST7735_BLACK);
            drawText(5, 50, "Good, Next", ST7735_CYAN, ST7735_BLACK, 2);
            sleep_ms(800);

        }
        // print round
        if (round == 25)
        {
            fillScreen(ST7735_BLACK);
            drawText(5, 50, "Gee Willikers!", ST7735_CYAN, ST7735_BLACK, 1);
            drawText(5, 60, "You're too good!", ST7735_CYAN, ST7735_BLACK, 1);
            sleep_ms(3000);
            fillScreen(ST7735_BLACK);
            THigh = 25;
            return;
        }
    }
}

void resetReaction()
{
    received = false;
    started = true;
    wrong = false;
}

void ReactionGame()
{

    uint8_t score = 0;             // The number of rounds completed
    uint8_t type = 1;              // If multiple game modes are used, this can designate them.
    uint64_t fastestTime = 500000; // Keep track of the fastest time, might end up being stored
    uint64_t slowestTime = 0;      // Keep track of the slowest time, might end up being stored
    uint8_t button;                // Which color lights up?
    riseTim = 0;
    fallTim = 0;
    totalTim = 0;
    bool didReset;

    uint64_t ScoreTime = 1000000; // max allowed time to score in us
    // 1000000 is base, drop to 750000 at the end
    // for 25 rounds, drop 10000 per round

    // TODO: Add audio

    while (started)
    {
        fillScreen(ST7735_BLACK);
        drawText(5, 10, "Welcome To", ST7735_WHITE, ST7735_BLACK, 2);
        sleep_ms(500);
        drawText(5, 30, "MONKEY", ST7735_CYAN, ST7735_BLACK, 2);
        sleep_ms(100);
        drawText(5, 50, "KONG!", ST7735_CYAN, ST7735_BLACK, 2);
        sleep_ms(500);
        drawText(5, 70, "Press any button to play.", ST7735_MAGENTA, ST7735_BLACK, 1);

        while (!gpio_get(BB_GP) && !gpio_get(RB_GP) && !gpio_get(GB_GP) && !gpio_get(YB_GP)){}
        
        resetReaction();
        if (type == 1)
        {

            // General game loop:
            // Leadup
            didReset = false;
            while (gpio_get(BB_GP) || gpio_get(GB_GP) || gpio_get(YB_GP) || gpio_get(RB_GP))
            {
            }
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
            button = (rand() % 4) + 1;
            riseTim = time_us_64();
            totalTim = 0;
            fallTim = 0;

            while (!received)
            {

                switch (button)
                {
                case 1:
                    gpio_put(BL_GP, 1);
                    gpio_put(GL_GP, 0);
                    gpio_put(RL_GP, 0);
                    gpio_put(YL_GP, 0);
                    if (gpio_get(BB_GP))
                    {
                        // Sucess
                        fallTim = time_us_64();
                        received = true;
                    }
                    break;
                case 2:
                    gpio_put(GL_GP, 1);
                    gpio_put(BL_GP, 0);
                    gpio_put(YL_GP, 0);
                    gpio_put(RL_GP, 0);
                    if (gpio_get(GB_GP))
                    {
                        // Sucess
                        fallTim = time_us_64();
                        received = true;
                    }
                    break;
                case 3:
                    gpio_put(RL_GP, 1);
                    gpio_put(BL_GP, 0);
                    gpio_put(GL_GP, 0);
                    gpio_put(YL_GP, 0);
                    if (gpio_get(RB_GP))
                    {
                        // Sucess
                        fallTim = time_us_64();
                        received = true;
                    }
                    break;
                case 4:
                    gpio_put(YL_GP, 1);
                    gpio_put(BL_GP, 0);
                    gpio_put(GL_GP, 0);
                    gpio_put(RL_GP, 0);
                    if (gpio_get(YB_GP))
                    {
                        // Sucess
                        fallTim = time_us_64();
                        received = true;
                    }
                    break;
                default:
                    gpio_put(GL_GP, 0);
                    gpio_put(BL_GP, 0);
                    gpio_put(YL_GP, 0);
                    gpio_put(RL_GP, 0);
                    break;
                }
                if ((gpio_get(BB_GP) || gpio_get(GB_GP) || gpio_get(YB_GP) || gpio_get(RB_GP)) && !received)
                {
                    // Fail
                    received = true;
                    fallTim = riseTim + ScoreTime + 1;
                }

                // Upon sucess, increment score
                //
            }
            if(gpio_get(RST_GP)){
                    fillScreen(ST7735_BLACK);
                    gpio_put(GL_GP, 0);
                    gpio_put(BL_GP, 0);
                    gpio_put(YL_GP, 0);
                    gpio_put(RL_GP, 0);
                    drawText(5, 10,"Are you sure you want to leave?", ST7735_CYAN, ST7735_BLACK, 2);
                    drawText(5, 50, "Y", ST7735_BLUE, ST7735_BLACK, 2);
                    drawText(5, 70, "N", ST7735_RED, ST7735_BLACK, 2);
                    gpio_put(BL_GP, 1);
                    gpio_put(RL_GP, 1);
                    while(!gpio_get(BB_GP) && !gpio_get(RB_GP)){}
                    gpio_put(BL_GP, 0);
                    gpio_put(RL_GP, 0);
                    if(gpio_get(BB_GP)){
                        fillScreen(ST7735_BLACK);
                        drawText(5, 10, "Are you really sure?", ST7735_CYAN, ST7735_BLACK, 2);
                        drawText(5, 50, "Y", ST7735_YELLOW, ST7735_BLACK, 2);
                        drawText(5, 70, "N", ST7735_GREEN, ST7735_BLACK, 2);
                        gpio_put(YL_GP, 1);
                        gpio_put(GL_GP, 1);
                        while (!gpio_get(GB_GP) && !gpio_get(YB_GP)){}
                        gpio_put(YL_GP, 0);
                        gpio_put(GL_GP, 0);
                        if(gpio_get(YB_GP)){
                            fillScreen(ST7735_BLACK);
                            drawText(0, 0,
                                     "Friends, Romans,     "
                                     "countrymen, lend me  "
                                     "your ears; I come to "
                                     "bury Caesar, not to  "
                                     "praise him. The evil "
                                     "that men do lives    "
                                     "after them; The good "
                                     "is oft interred with "
                                     "their bones; So let  "
                                     "it be with Caesar.   "
                                     "The noble Brutus hath"
                                     "told you Caesar was  "
                                     "ambitious: If it were"
                                     "so, it was a grievous"
                                     "fault, And grievously"
                                     "hath Caesar answer'd "
                                     "it",
                                     ST7735_WHITE, ST7735_BLACK, 1);
                            sleep_ms(10000);
                            return;
                        }
                        else if(gpio_get(GB_GP)){
                            fillScreen(ST7735_BLACK);
                            drawText(5, 10, "Well then why'd you press RESET", ST7735_CYAN, ST7735_BLACK, 1);
                            drawText(5, 30, "and say you were sure?", ST7735_CYAN, ST7735_BLACK, 1);
                            didReset = true;
                            drawText(5, 70, "Fine, resetting", ST7735_CYAN, ST7735_BLACK, 1);
                            drawText(5, 90, "only the round", ST7735_CYAN, ST7735_BLACK, 1);
                            sleep_ms(3000);
                        }
                    }
                    else if(gpio_get(RB_GP)){
                        fillScreen(ST7735_BLACK);
                        drawText(5, 10, "Well then why'd you press RESET?", ST7735_CYAN, ST7735_BLACK, 1);
                        didReset = true;
                        drawText(5, 30, "Fine, resetting", ST7735_CYAN, ST7735_BLACK, 1);
                        drawText(5, 50, "only the round", ST7735_CYAN, ST7735_BLACK, 1);
                        sleep_ms(3000);
                    }
                }
            while (gpio_get(BB_GP) || gpio_get(GB_GP) || gpio_get(YB_GP) || gpio_get(RB_GP))
            {
            }

            gpio_put(GL_GP, 0);
            gpio_put(BL_GP, 0);
            gpio_put(YL_GP, 0);
            gpio_put(RL_GP, 0);

            totalTim = fallTim - riseTim;
            if(!didReset){
                if (totalTim <= ScoreTime)
                {
                    sprintf(buf, "Time: %u", totalTim);
                    score++;
                    sprintf(buf2, "Score: %u", score);
                    if (totalTim > slowestTime)
                    {
                        slowestTime = totalTim;
                    }
                    if (totalTim < fastestTime)
                    {
                        fastestTime = totalTim;
                    }
    
                    // Actual display
                    drawText(10, 60, buf, ST7735_WHITE, ST7735_BLACK, 2);
                    sleep_ms(1000);
                    drawText(10, 100, buf2, ST7735_WHITE, ST7735_BLACK, 2);
                    sleep_ms(4000);
                    ScoreTime -= 10000;
                }
                else
                {
                    wrong = true;
                }
    
                if (score >= 25 && !wrong)
                {
                    // Display a win screen
                    drawText(10, 60, "Congradulations", ST7735_WHITE, ST7735_BLACK, 2);
                    drawText(10, 80, "You Win!", ST7735_WHITE, ST7735_BLACK, 2);
    
                    sleep_ms(500);
    
                    sprintf(buf, "%u", fastestTime);
                    sprintf(buf2, "%u", slowestTime);
    
                    drawText(5, 100, "Fastest Time:", ST7735_WHITE, ST7735_BLACK, 1);
                    drawText(5, 110, buf, ST7735_WHITE, ST7735_BLACK, 1);
    
                    sleep_ms(500);
    
                    drawText(5, 140, "Slowest Time:", ST7735_WHITE, ST7735_BLACK, 1);
                    drawText(5, 150, buf2, ST7735_WHITE, ST7735_BLACK, 1);
    
                    sleep_ms(5000);
    
                    started = false;
                    // Go back to main menu
                }
                else if (wrong)
                {
                    drawText(10, 60, "Too Bad", ST7735_WHITE, ST7735_BLACK, 2);
                    drawText(10, 80, "You Lost!", ST7735_WHITE, ST7735_BLACK, 2);
    
                    sleep_ms(500);
    
                    sprintf(buf, "%u", fastestTime);
                    sprintf(buf2, "%u", slowestTime);
    
                    drawText(5, 100, "Fastest Time:", ST7735_WHITE, ST7735_BLACK, 1);
                    drawText(5, 110, buf, ST7735_WHITE, ST7735_BLACK, 1);
    
                    sleep_ms(500);
    
                    drawText(5, 140, "Slowest Time:", ST7735_WHITE, ST7735_BLACK, 1);
                    drawText(5, 150, buf2, ST7735_WHITE, ST7735_BLACK, 1);
    
                    sleep_ms(5000);
    
                    started = false;
                }
            }
        }
    }
}

// Include the button timing game here
void lengthPress()
{
    int timelength = 0;
    (void)timelength;
}

void stateReset()
{
    wrong = false;
    for (int i = 0; i < 50; ++i)
    {
        currSeq[i] = 0;
    }
}

// Include extra code for score saving and extra features here

int main()
{
    stdio_init_all();

    init();
    TFT_BlackTab_Initialize();
    setTextWrap(true);
    fillScreen(ST7735_BLACK);
    srand((uint)time_us_32);
    while (true)
    {
        drawText(5, 10, "RECREATION LAD", ST7735_WHITE, ST7735_BLACK, 1);
        drawText(5, 30, "Select an game:", ST7735_WHITE, ST7735_BLACK, 1);
        drawText(5, 50, "Simon Cowell", ST7735_YELLOW, ST7735_BLACK, 1);
        drawText(5, 70, "Eduardo Bros.", ST7735_RED, ST7735_BLACK, 1);
        drawText(5, 90, "Monkey Kong", ST7735_GREEN, ST7735_BLACK, 1);
        drawText(5, 110, "Legend of Triangle", ST7735_BLUE, ST7735_BLACK, 1);

        if (gpio_get(YB_GP))
        {
            fillScreen(ST7735_BLACK);
            mainSimon();
            fillScreen(ST7735_BLACK);
        }
        if (gpio_get(RB_GP))
        {
            fillScreen(ST7735_BLACK);
            timinSimon();
            fillScreen(ST7735_BLACK);
        }
        if (gpio_get(BB_GP))
        {
            //Print the High Scores
        }
        if (gpio_get(GB_GP))
        {
            resetReaction();
            ReactionGame();
            fillScreen(ST7735_BLACK);
        }
    }
}
