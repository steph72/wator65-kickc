#include <stdio.h>
#include <mega65.h>
#include <mega65-dma.h>
#include <conio.h>
#include <conio-lines.h>
#include <printf.h>

#define WT_WATER 6
#define WT_FISH 5
#define WT_SHARK 2

#define WIDTH 80
#define HEIGHT 50
#define WSIZE 4000

byte *canvas;

byte *fmode = 0x0;

byte *vic3_control = 0xd031;
byte *vic3_border = 0xd020; // border
byte *vic3_bg = 0xd021;     // background
byte *vic3_cb = 0xd018;     // character base

byte *vic3_scnptr4 = 0xd060;    // 32bit screen pointer
byte *vic3_scnptr3 = 0xd061;
byte *vic3_scnptr2 = 0xd062;
byte *vic3_scnptr1 = 0xd063;

byte *kbscan = 0xd610;          // keyboard scanner
byte *kbmod = 0xd611;

byte *sharkEnergy;

byte *surviveTime;

byte dirPermutations[20] = {

    0,
    1,
    2,
    3,
    0,
    2,
    1,
    3,
    0,
    3,
    1,
    2,
    0,
    2,
    3,
    1,
    0,
    1,
    2,
    3,

};

signed char directions[4] = {-1, 1, -WIDTH, WIDTH};

byte fishTimeToReproduce;
byte sharkTimeToReproduce;
byte initialSharkEnergy;
int initialSharks;
int initialFish;

// DMA list entry for filling data in the 1MB memory space
struct DMA_LIST_F018B memfill_dma_command4 = {
    DMA_COMMAND_FILL, // command
    0,                // count
    0,                // source
    0,                // source bank
    0,                // destination
    0,                // destination bank
    0,                // sub-command
    0                 // modulo-value
};

void memfill_dma4(char dest_bank, void *dest, char src_bank, void *src, unsigned int num)
{
    // Remember current F018 A/B mode
    char dmaMode = DMA->EN018B;
    // Set up command
    memfill_dma_command4.count = num;
    memfill_dma_command4.src_bank = src_bank;
    memfill_dma_command4.src = src;
    memfill_dma_command4.dest_bank = dest_bank;
    memfill_dma_command4.dest = dest;
    // Set F018B mode
    DMA->EN018B = 1;
    // Set address of DMA list
    DMA->ADDRMB = 0;
    DMA->ADDRBANK = 0;
    DMA->ADDRMSB = > &memfill_dma_command4;
    // Trigger the DMA (without option lists)
    DMA->ADDRLSBTRIG = < &memfill_dma_command4;
    // Re-enable F018A mode
    DMA->EN018B = dmaMode;
}

void mega65_io_enable()
{
    *IO_KEY = 0x47;
    *IO_KEY = 0x53;
    *fmode = 65;
}

void init()
{
    *vic3_bg = 0x25;
    mega65_io_enable();
    canvas = (byte *)malloc(WSIZE);
    sharkEnergy = (byte *)malloc(WSIZE);
    surviveTime = (byte *)malloc(WSIZE);
}

void dealloc()
{
    free(canvas);
    free(sharkEnergy);
    free(surviveTime);
}

void setTextScreen()
{
    *vic3_control &= 247;   // disable interlace
    *vic3_scnptr4 = 0x00;
    *vic3_scnptr3 = 0x08;
    *vic3_scnptr2 = 0x00;
    *vic3_scnptr1 = 0x00;
}

void setWatorScreen()
{

    long textScreen = 0x40000;

    mega65_io_enable();

    *vic3_control |= 128; // enable 80chars
    *vic3_control |= 8;   // enable interlace
    *vic3_bg = 0;
    *vic3_border = 0;

    *vic3_scnptr4 = 0x00;
    *vic3_scnptr3 = 0x00;
    *vic3_scnptr2 = 0x04;
    *vic3_scnptr1 = 0x00;

    // lfill(0x40000, 81, WSIZE); // fill text screen with solid circles
    memfill_dma4(4, 0x0000, 0, 81, WSIZE);

    // lfill((long)canvas, WT_WATER, WSIZE);
    memfill_dma4(0, canvas, 0, WT_WATER, WSIZE);

    // lfill((long)sharkEnergy, initialSharkEnergy, WSIZE);
    memfill_dma4(0, sharkEnergy, 0, initialSharkEnergy, WSIZE);

    // lfill((long)surviveTime, 0, WSIZE);
    memfill_dma4(0, surviveTime, 0, 0, WSIZE);
}

void doFish(int idx)
{
    byte i;
    byte dir;
    int newIdx;
    int indexToGo;
    bool didMove;

    byte permIdx;

    byte s;

    permIdx = rand() & 15;
    didMove = false;

    s = ++surviveTime[idx];

    for (i = 0; i < 4; ++i)
    {
        dir = dirPermutations[permIdx + i];
        newIdx = idx + directions[dir];

        if (newIdx < 0)
        {
            newIdx += WSIZE;
        }
        else if (newIdx > WSIZE)
        {
            newIdx -= WSIZE;
        }

        if (canvas[newIdx] == WT_WATER)
        {
            didMove = true;
            indexToGo = newIdx;
            break;
        }
    }

    if (!didMove)
    {
        return;
    }

    canvas[indexToGo] = WT_FISH;
    surviveTime[indexToGo] = s;
    canvas[idx] = WT_WATER;

    if (s > fishTimeToReproduce)
    {
        {
            canvas[idx] = WT_FISH;
            surviveTime[idx] = 0;
        }
    }
}

void doShark(int idx)
{
    byte i;
    byte dir;
    int newIdx;
    int newSharkIndex;
    bool didEat;

    byte permIdx;

    permIdx = rand() & 15;
    newSharkIndex = idx;
    didEat = false;

    for (i = 0; i < 4; ++i)
    {
        dir = dirPermutations[i + permIdx];
        newIdx = idx + directions[dir];

        if (newIdx < 0)
        {
            newIdx += WSIZE;
        }
        else if (newIdx > WSIZE)
        {
            newIdx -= WSIZE;
        }

        if (canvas[newIdx] == WT_FISH)
        {
            didEat = true;
            newSharkIndex = newIdx;
            break;
        }

        if (canvas[newIdx] == WT_WATER)
        {
            newSharkIndex = newIdx;
        }
    }

    --sharkEnergy[idx];

    if (newSharkIndex != idx)
    {
        // move
        canvas[newSharkIndex] = WT_SHARK;
        surviveTime[newSharkIndex] = surviveTime[idx];
        sharkEnergy[newSharkIndex] = sharkEnergy[idx];
        canvas[idx] = WT_WATER;
    }

    if (didEat)
    {
        // shark did eat -- increase energy
        if (sharkEnergy[newSharkIndex] < 255)
        {
            ++sharkEnergy[newSharkIndex];
        }
    }
    else
    {
        // check shark energy
        if (sharkEnergy[newSharkIndex] == 0)
        {
            canvas[newSharkIndex] = WT_WATER;
            return;
        }
    }

    if (surviveTime[newSharkIndex] >= sharkTimeToReproduce)
    {
        if (idx != newSharkIndex)
        {
            canvas[idx] = WT_SHARK;
            surviveTime[idx] = 0;
            surviveTime[newSharkIndex] = 0;
            sharkEnergy[idx] = initialSharkEnergy;
        }
    }

    ++surviveTime[newSharkIndex];
}

long mainloop(void)
{
    int i;
    long generations = 0;
    byte t;

    int numSharks;

    *kbscan = 0;

    do
    {
        generations++;
        numSharks = 0;

        for (i = 0; i < WSIZE; ++i)
        {
            t = canvas[i];

            if (t == WT_FISH)
            {
                doFish(i);
            }
            else if (t == WT_SHARK)
            {
                numSharks++;
                doShark(i);
            }
        }

        memcpy_dma256(0xff, 0x08, 0x0000, 0x00, 0x00, canvas, WSIZE);

    } while (numSharks && (!(*kbscan == 32)));

    *kbscan=0;

    while (*kbscan!=0) {
        // wait for kbd to settle down    
    }
    
    return generations;
}

void initWorld(int numFish, int numSharks)
{
    int i;
    unsigned int idx;
    for (i = 0; i < numFish; ++i)
    {
        do
        {
            do
            {
                idx = rand() & 4095;
            } while (idx > 4000);

        } while (canvas[idx] != (byte)WT_WATER);
        canvas[idx] = WT_FISH;
    }
    for (i = 0; i < numSharks; ++i)
    {
        do
        {
            do
            {
                idx = rand() & 4095;
            } while (idx > 4000);
        } while (canvas[idx] != (byte)WT_WATER);
        canvas[idx] = WT_SHARK;
    }
}

char cgetc()
{
    char res;
    while (*kbscan == 0);
    res = *kbscan;
    *kbscan = 0;
    return res;
}

void doSim()
{

    long gens;

    setWatorScreen();
    srand(16127);
    initWorld(initialFish, initialSharks);
    gens = mainloop();
    dealloc();
    setTextScreen();
    clrscr();
}

void main()
{
    char cmd;
    byte c;
    bool quit;
    bool didRunSimulation;
    unsigned int inc;
    unsigned int params[5] = {10, 50, 40, 180, 100};

    byte currentParameter;

    init();
    quit = false;

    currentParameter = 0;

    do
    {
        didRunSimulation = false;
        clrscr();
        bgcolor(0);
        bordercolor(6);
        textcolor(LIGHT_BLUE);
        chline(80);
        textcolor(GREEN);

        //   --------------------------------------------------------------------------------
        cputs(
            "wa-tor for the mega65 version 2.1\n"
            "s. kleinert, october 2020\n");
        textcolor(LIGHT_BLUE);
        chline(80);

        cputsxy(6, 5, "fish time to reproduce  :");
        cputsxy(6, 7, "shark time to reproduce :");
        cputsxy(6, 9, "initial shark energy    :");
        cputsxy(6, 11, "initial # of sharks     :");
        cputsxy(6, 13, "initial # of fish       :");

        gotoxy(0, 15);
        chline(80);
        textcolor(GREEN);
        cputs("\ncursor up/down          -  select parameter\n"
              "cursor left-right       -  change value (fine)\n"
              "space                   -  start/stop simulation\n");

        do
        {
            textcolor(ORANGE);

            for (c = 0; c < 5; ++c)
            {
                if (c < 3)
                {
                    if ((unsigned int)(params[c]) > 255)
                    {
                        params[c] = 255;
                    }
                }
                else
                {

                    if ((unsigned int)(params[c]) > 2000)
                    {
                        params[c] = 2000;
                    }
                }
                gotoxy(32, 5 + (c * 2));
                printf("%4u", params[c]);
            }

            textcolor(RED | 16);
            cputsxy(38, 5 + (currentParameter * 2), "<--");
            cmd = cgetc();
            cputsxy(38, 5 + (currentParameter * 2), "   ");

            if (*kbmod & 4)
            {
                inc = 100;
            }
            else
            {
                inc = 1;
            }

            switch (cmd)
            {
            case 0x91: // up
                if (currentParameter > 0)
                    currentParameter--;
                break;

            case 0x11: // down
                if (currentParameter < 4)
                    currentParameter++;
                break;

            case 0x9d: // left
                if (params[currentParameter] >= inc)
                    params[currentParameter] = params[currentParameter] - inc;
                break;

            case 0x1d: // right
                params[currentParameter] = params[currentParameter] + inc;
                break;

            case 0x20: // space
            {
                fishTimeToReproduce = <params[0];  // 10
                sharkTimeToReproduce = <params[1]; // 50;
                initialSharkEnergy = <params[2]; // 40;
                initialSharks = (int) params[3];
                initialFish = (int) params[4];

                doSim();
                didRunSimulation = true;
            }

            default:
                break;
            }
        } while (!didRunSimulation);

    } while (!quit);

}