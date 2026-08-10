#include<windows.h>
#include<mmsystem.h>          // for mciSendString (river + horn audio)
#include<GL/glut.h>
#include<stdlib.h>
#include<math.h>
#include<stdio.h>
#include<string.h>

#pragma comment(lib,"winmm.lib")   // link the Windows multimedia library
// If you're building with MinGW/g++ instead of MSVC and this pragma is
// ignored, compile with:  g++ village.cpp -o village.exe -lglut32 -lwinmm

//====================== SOUND HELPERS ======================//
// Using mciSendString instead of plain PlaySound so multiple sounds
// (river loop + horn) can play at the same time without cutting each other off.

// Set to true to see a popup box explaining any sound error (useful for debugging).
// Once sound is confirmed working, set this back to false so no popups interrupt the program.
bool SOUND_DEBUG = true;

void mciCheck(DWORD err, const char* label)
{
    if(err != 0 && SOUND_DEBUG)
    {
        char errMsg[256];
        mciGetErrorStringA(err, errMsg, 256);

        char full[400];
        sprintf(full, "[%s] MCI error %lu:\n%s", label, err, errMsg);
        MessageBoxA(NULL, full, "Sound Error", MB_OK | MB_ICONERROR);
    }
}

void playLoopSound(const char* file, const char* alias)
{
    char cmd[256];
    DWORD err;

    sprintf(cmd, "open \"%s\" type waveaudio alias %s", file, alias);
    err = mciSendStringA(cmd, NULL, 0, NULL);
    mciCheck(err, "open loop");

    // NOTE: the waveaudio MCI driver does not support the "repeat" flag,
    // so instead we start it once here and manually restart it every
    // frame once it finishes (see checkLoopSound(), called from display()).
    sprintf(cmd, "play %s from 0", alias);
    err = mciSendStringA(cmd, NULL, 0, NULL);
    mciCheck(err, "play loop");
}

// Call this every frame (from display()) for any sound started with
// playLoopSound(), so it restarts automatically when it reaches the end.
void checkLoopSound(const char* alias)
{
    char status[128];
    char cmd[128];
    sprintf(cmd, "status %s mode", alias);
    mciSendStringA(cmd, status, 128, NULL);

    if(strcmp(status, "stopped") == 0)
    {
        char playCmd[128];
        sprintf(playCmd, "play %s from 0", alias);
        mciSendStringA(playCmd, NULL, 0, NULL);
    }
}

void playOnceSound(const char* file, const char* alias)
{
    char cmd[256];
    DWORD err;

    sprintf(cmd, "close %s", alias);      // close any previous instance so repeated presses don't queue up
    mciSendStringA(cmd, NULL, 0, NULL);   // ignore error here — alias may not exist yet on first press

    sprintf(cmd, "open \"%s\" type waveaudio alias %s", file, alias);
    err = mciSendStringA(cmd, NULL, 0, NULL);
    mciCheck(err, "open once");

    sprintf(cmd, "play %s from 0", alias);
    err = mciSendStringA(cmd, NULL, 0, NULL);
    mciCheck(err, "play once");
}

double  r=.2,s=.3;
int i;
float  tx=10,bx=10;
float sx = -150;     // sun x position
float mx = 220;      // moon x position (start outside right)
bool isNight = false;
int starBlink = 0; //star
float solarAngle = 0; //solar system

int cycleStart = 0;          // ms timestamp when the current day/night phase began
const int CYCLE_MS = 10000;  // each phase (day or night) lasts 10 seconds

bool isRaining = false; // rain
float rainX[200], rainY[200];
// Fish animation
float fishX1 = -180;
float fishX2 = 120;
float fishX3 = -50;

float manX = -200; // man
float womanX = -120;
float childX = -50;
bool nightMode = false;

float scaleFactor = 1.0f; //boat
bool scaleUp = true;
float gearAngle = 0.0f;
float carX = -150;   // car position

float birdX = -200;  // birds flying position

//DDa algorithom
void DDA(float x1, float y1, float x2, float y2)
{
    float dx = x2 - x1;
    float dy = y2 - y1;

    float steps = (abs(dx) > abs(dy)) ? abs(dx) : abs(dy);

    float xInc = dx / steps;
    float yInc = dy / steps;

    float x = x1;
    float y = y1;

    glBegin(GL_POINTS);
    for(int i = 0; i <= steps; i++)
    {
        glVertex2f(x, y);
        x += xInc;
        y += yInc;
    }
    glEnd();
}

//mid point
void drawCircle(int xc, int yc, int r)
{
    int x = 0;
    int y = r;
    int d = 1 - r;

    glBegin(GL_POINTS);

    while(x <= y)
    {
        glVertex2i(xc + x, yc + y);
        glVertex2i(xc - x, yc + y);
        glVertex2i(xc + x, yc - y);
        glVertex2i(xc - x, yc - y);
        glVertex2i(xc + y, yc + x);
        glVertex2i(xc - y, yc + x);
        glVertex2i(xc + y, yc - x);
        glVertex2i(xc - y, yc - x);

        if(d < 0)
            d += 2*x + 3;
        else
        {
            d += 2*(x - y) + 5;
            y--;
        }
        x++;
    }

    glEnd();
}

// APPLE
void drawApple(int x, int y)
{
    //Apple body
    glColor3ub(255,0,0); // red
    drawCircle(x, y, 3);

    //Leaf
    glColor3ub(0,150,0);
    glBegin(GL_TRIANGLES);
        glVertex2i(x, y+4);
        glVertex2i(x+3, y+6);
        glVertex2i(x+1, y+3);
    glEnd();

    //Stem
    glColor3ub(101,67,33);
    glBegin(GL_LINES);
        glVertex2i(x, y+3);
        glVertex2i(x, y+6);
    glEnd();
}


void init()
{
    glClearColor(1.0f,1.0f,1.0f,1.0f);
    glOrtho(-210,210,-220,310,-210,310);

    //Star
    glEnable(GL_POINT_SMOOTH);
    glPointSize(4);
    glPointSize(2.0);

    // rain
    for(int i=0;i<200;i++)
{
    rainX[i] = rand()%400 - 200;
    rainY[i] = rand()%300;
}

    cycleStart = glutGet(GLUT_ELAPSED_TIME);

    //river water-flow sound, looping in the background for the whole run
    // using the FULL path avoids Code::Blocks working-directory issues
    playLoopSound("C:\\Users\\This PC\\Documents\\lab\\river.wav", "riverSound");
}

//Windmaill
void windmill(int x, int y)
{
    // pole
    glColor3ub(139,69,19);
    glBegin(GL_POLYGON);
        glVertex2i(x, y);
        glVertex2i(x+4, y);
        glVertex2i(x+4, y+80);
        glVertex2i(x, y+80);
    glEnd();

    // head (rotating)
    glPushMatrix();
    glTranslatef(x+2, y+80, 0);
    glRotatef(gearAngle, 0, 0, 1);

    glColor3ub(250,0,0);

    glBegin(GL_LINES);
        glVertex2f(0,0);
        glVertex2f(20,0);

        glVertex2f(0,0);
        glVertex2f(-20,0);

        glVertex2f(0,0);
        glVertex2f(0,20);

        glVertex2f(0,0);
        glVertex2f(0,-20);
    glEnd();

    glPopMatrix();
}


//cloud
void cloud(double x, double y)
{


    glBegin(GL_TRIANGLE_FAN);
        for(i=0;i<360;i++)
        {
            x=x+cos((i*3.14)/180)*r;
            y=y+sin((i*3.14)/180)*r;

            glVertex2d(x,y);

        }

    glEnd();



}


//sun

void sun(double x, double y)
{


    glBegin(GL_TRIANGLE_FAN);
        for(i=0;i<360;i++)
        {
            x=x+cos((i*3.14)/180)*s;
            y=y+sin((i*3.14)/180)*s;

            glVertex2d(x,y);

        }


    glEnd();



}


//clean disc helpers (used for sun/moon so they render as perfect circles)

void drawDisc(double cx, double cy, double rad)
{
    glBegin(GL_TRIANGLE_FAN);
        glVertex2d(cx, cy);
        for(int a=0;a<=360;a+=6)
        {
            double angle = a * 3.1416 / 180;
            glVertex2d(cx + cos(angle)*rad, cy + sin(angle)*rad);
        }
    glEnd();
}

void drawEllipseShape(double cx, double cy, double rx, double ry)
{
    glBegin(GL_TRIANGLE_FAN);
        glVertex2d(cx, cy);
        for(int a=0;a<=360;a+=8)
        {
            double angle = a * 3.1416 / 180;
            glVertex2d(cx + cos(angle)*rx, cy + sin(angle)*ry);
        }
    glEnd();
}

//moon

void moon(double x, double y)
{
    //Outer glow halo (soft, translucent, perfectly round)
    glColor4f(0.85f,0.85f,0.75f,0.20f);
    drawDisc(x, y, 18);

    glColor4f(0.9f,0.9f,0.85f,0.30f);
    drawDisc(x, y, 14);

    //Big moon (base)
    glColor3ub(255,255,255);
    drawDisc(x, y, 11);

    // shadow crescent
    glColor3ub(10,10,40);   // same as night sky (natural shadow)
    drawDisc(x + 5, y + 2, 11);
}



//star

void star(float x, float y)
{
    glColor3ub(255,255,255);
    glBegin(GL_POINTS);
        glVertex2f(x,y);
    glEnd();
}

//Twinkling star field (positions + per-star brightness)
float starX[10] = {-180,-140,-100,-60,-20,20,60,100,140,170};
float starY[10] = {260,240,270,250,260,240,270,250,260,230};
float starBright[10];

void drawStarsTwinkle()
{
    glPointSize(2.0f);
    glBegin(GL_POINTS);
    for(int k=0;k<10;k++)
    {
        float b = starBright[k];
        glColor3f(b,b,b*0.95f);
        glVertex2f(starX[k], starY[k]);
    }
    glEnd();

    // A few larger twinkling stars for sparkle
    glPointSize(3.5f);
    glBegin(GL_POINTS);
    for(int k=0;k<5;k++)
    {
        float b = starBright[k*2];
        glColor3f(b,b,b);
        glVertex2f(starX[k*2], starY[k*2]);
    }
    glEnd();
    glPointSize(2.0);
}

//solar light

void solarLight(double x, double y)
{
    // pole
    glColor3ub(255,0,0);
    glBegin(GL_POLYGON);
        glVertex2d(x, y);
        glVertex2d(x+2, y);
        glVertex2d(x+2, y+30);
        glVertex2d(x, y+30);
    glEnd();

    // lamp head
    glColor3ub(50,50,50);
    glBegin(GL_POLYGON);
        glVertex2d(x-3, y+30);
        glVertex2d(x+5, y+30);
        glVertex2d(x+5, y+35);
        glVertex2d(x-3, y+35);
    glEnd();

    // light (ON/OFF)
    if(isNight)
        glColor3ub(255,255,150); // glowing light
    else
        glColor3ub(100,100,100);

    glBegin(GL_TRIANGLES);
        glVertex2d(x-10, y+30);
        glVertex2d(x+10, y+30);
        glVertex2d(x, y+50);
    glEnd();
}

//Walikin path
void walkingPath()
{
    glColor3ub(245,245,245);

    // main path
    DDA(-200, -20, 200, -20);

    // side border lines
    glColor3ub(200,200,200);
    DDA(-200, -25, 200, -25);
    DDA(-200, -15, 200, -15);
}

//human
void drawHuman(float x, float y)
{
    glPushMatrix();
    glTranslatef(x, y, 0);

    // Body
    glColor3ub(0,0,0);
    glRecti(-3, 10, 3, 30);

    // Head
    glColor3ub(255,220,177);
    glBegin(GL_TRIANGLE_FAN);
        for(int i=0;i<=360;i++)
        {
            float a = i * 3.1416 / 180;
            glVertex2f(cos(a)*5, 35 + sin(a)*5);
        }
    glEnd();

    // Arms
    glColor3ub(0,0,0);
    glBegin(GL_LINES);
        glVertex2i(-3,25); glVertex2i(-8,18);
        glVertex2i(3,25);  glVertex2i(8,18);
    glEnd();

    // Legs
    glBegin(GL_LINES);
        glVertex2i(-2,10); glVertex2i(-5,0);
        glVertex2i(2,10);  glVertex2i(5,0);
    glEnd();

    glPopMatrix();
}


//Gear

void gear(double x, double y)
{
    glPushMatrix();
    glTranslatef(x, y, 0);
    glRotatef(gearAngle, 0, 0, 1);

    //Outer circle
    glColor3ub(255,215,0);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(0,0);
    for(int i=0;i<=360;i++)
    {
        double angle = (i * 3.1416) / 180;
        glVertex2d(cos(angle)*10, sin(angle)*10);
    }
    glEnd();

    //Spokes (rotation visible hobe)
    glColor3ub(0,0,0);
    glBegin(GL_LINES);

    for(int i=0;i<8;i++) // 8 ta line
    {
        double angle = (i * 3.1416) / 4;

        glVertex2f(0,0);
        glVertex2d(cos(angle)*10, sin(angle)*10);
    }

    glEnd();

    glPopMatrix();
}

//SIMPLE CAR
void drawCar()
{
    glPushMatrix();
    glTranslatef(carX, -10, 0); // field position

    //body
    glColor3ub(200,0,0);
    glBegin(GL_POLYGON);
        glVertex2i(-15,10);
        glVertex2i(25,10);
        glVertex2i(30,20);
        glVertex2i(-20,20);
    glEnd();

    //window
    glColor3ub(173,216,230);
    glBegin(GL_POLYGON);
        glVertex2i(-5,20);
        glVertex2i(10,20);
        glVertex2i(8,28);
        glVertex2i(-2,28);
    glEnd();

    // wheels (midpoint circle use)
    glColor3ub(0,0,0);
    drawCircle(-10, 10, 4);
    drawCircle(20, 10, 4);

    glPopMatrix();
}


//breserhum
void drawLineBresenham(int x1, int y1, int x2, int y2)
{
    int dx = abs(x2 - x1);
    int dy = abs(y2 - y1);

    int sx = (x1 < x2) ? 1 : -1;
    int sy = (y1 < y2) ? 1 : -1;

    int err = dx - dy;

    glBegin(GL_POINTS);

    while (true)
    {
        glVertex2i(x1, y1);

        if (x1 == x2 && y1 == y2)
            break;

        int e2 = 2 * err;

        if (e2 > -dy)
        {
            err -= dy;
            x1 += sx;
        }
        if (e2 < dx)
        {
            err += dx;
            y1 += sy;
        }
    }

    glEnd();
}

//Fence
void fence(int x)
{
    glColor3ub(184,134,11);

    // Vertical lines (stick)
    drawLineBresenham(190-x,130,190-x,70);
    drawLineBresenham(187-x,130,187-x,70);

    // Top triangle style
    drawLineBresenham(187-x,130,190-x,130);
    drawLineBresenham(187-x,130,188-x,140);
    drawLineBresenham(188-x,140,190-x,130);
}

// rain
void drawRain()
{
    if(!isRaining) return;

    glColor3ub(173,216,230);

    glBegin(GL_LINES);
    for(int i=0;i<200;i++)
    {
        glVertex2f(rainX[i], rainY[i]);
        glVertex2f(rainX[i], rainY[i]-10);

        rainY[i] -= 4;

        if(rainY[i] < -200)
            rainY[i] = 300;
    }
    glEnd();
}


// dram man
void drawMan()
{
    glPushMatrix();
    glTranslatef(manX, -20, 0);   // path position

    // Body
    glColor3ub(0,0,0);
    glRecti(-3, 10, 3, 30);

    // Head (simple circle)
    glColor3ub(255,220,177);
    glBegin(GL_TRIANGLE_FAN);
        for(int i=0;i<=360;i++)
        {
            float a = i * 3.1416 / 180;
            glVertex2f(cos(a)*5, 35 + sin(a)*5);
        }
    glEnd();

    // Legs (walking effect)
    glBegin(GL_LINES);
        glVertex2i(-2,10);
        glVertex2i(-5,0);

        glVertex2i(2,10);
        glVertex2i(5,0);
    glEnd();

    // Arms
    glBegin(GL_LINES);
        glVertex2i(-3,25);
        glVertex2i(-8,18);

        glVertex2i(3,25);
        glVertex2i(8,18);
    glEnd();

    glPopMatrix();
}

// hen and chicks (under front tree)
void hen(float x, float y, float scale)
{
    // body (blue)
    glColor3ub(40,90,200);
    glBegin(GL_TRIANGLE_FAN);
        for(int i=0;i<=360;i++)
        {
            float a = i * 3.1416 / 180;
            glVertex2f(x + cos(a)*6*scale, y + sin(a)*4*scale);
        }
    glEnd();

    // head
    glColor3ub(160,82,45);
    glBegin(GL_TRIANGLE_FAN);
        for(int i=0;i<=360;i++)
        {
            float a = i * 3.1416 / 180;
            glVertex2f(x+6*scale + cos(a)*2.5f*scale, y+3*scale + sin(a)*2.5f*scale);
        }
    glEnd();

    // beak
    glColor3ub(255,140,0);
    glBegin(GL_TRIANGLES);
        glVertex2f(x+8*scale,y+3*scale);
        glVertex2f(x+11*scale,y+2*scale);
        glVertex2f(x+8*scale,y+1*scale);
    glEnd();

    // comb
    glColor3ub(220,20,60);
    glBegin(GL_TRIANGLES);
        glVertex2f(x+5*scale,y+5*scale);
        glVertex2f(x+6*scale,y+7*scale);
        glVertex2f(x+7*scale,y+5*scale);
    glEnd();

    // legs
    glColor3ub(255,140,0);
    glBegin(GL_LINES);
        glVertex2f(x-2*scale,y-4*scale); glVertex2f(x-2*scale,y-8*scale);
        glVertex2f(x+2*scale,y-4*scale); glVertex2f(x+2*scale,y-8*scale);
    glEnd();
}

void chick(float x, float y, float scale)
{
    // body
    glColor3ub(255,215,0);
    glBegin(GL_TRIANGLE_FAN);
        for(int i=0;i<=360;i++)
        {
            float a = i * 3.1416 / 180;
            glVertex2f(x + cos(a)*3*scale, y + sin(a)*2.2f*scale);
        }
    glEnd();

    // head
    glBegin(GL_TRIANGLE_FAN);
        for(int i=0;i<=360;i++)
        {
            float a = i * 3.1416 / 180;
            glVertex2f(x+3*scale + cos(a)*1.5f*scale, y+1.5f*scale + sin(a)*1.5f*scale);
        }
    glEnd();

    // beak
    glColor3ub(255,140,0);
    glBegin(GL_TRIANGLES);
        glVertex2f(x+4.2f*scale,y+1.5f*scale);
        glVertex2f(x+6*scale,y+1*scale);
        glVertex2f(x+4.2f*scale,y+0.5f*scale);
    glEnd();

    // legs
    glBegin(GL_LINES);
        glVertex2f(x-1*scale,y-2*scale); glVertex2f(x-1*scale,y-4*scale);
        glVertex2f(x+1*scale,y-2*scale); glVertex2f(x+1*scale,y-4*scale);
    glEnd();
}

void henFamily()
{
    // mother hen made bigger, chicks made smaller and spread out so
    // they stay clearly visible around her instead of overlapping
    hen(-150, 42, 1.5f);
    chick(-134, 34, 0.6f);
    chick(-126, 40, 0.6f);
    chick(-138, 28, 0.6f);
    chick(-122, 32, 0.6f);
}

//flying birds (sky)
void drawBird(float x, float y, float phase)
{
    glColor3ub(0,0,0);
    float flap = sin((birdX + phase) * 0.15f) * 4.0f;  // wing-flap motion

    glBegin(GL_LINE_STRIP);
        glVertex2f(x-8, y + flap*0.5f);
        glVertex2f(x-4, y + flap);
        glVertex2f(x,   y);
        glVertex2f(x+4, y + flap);
        glVertex2f(x+8, y + flap*0.5f);
    glEnd();
}

void birdFlock()
{
    drawBird(birdX,        270, 0);
    drawBird(birdX+30,     280, 40);
    drawBird(birdX+60,     265, 80);
    drawBird(birdX-30,     260, 120);
    drawBird(birdX-60,     275, 160);
}

//fireflies (night only)
float ffX[15], ffY[15], ffBright[15], ffTimer[15];

void initFireflies()
{
    for(int k=0;k<15;k++)
    {
        ffX[k] = (rand()%380) - 190;
        ffY[k] = (rand()%70) - 10;
        ffBright[k] = (rand()%100)/100.0f;
        ffTimer[k] = (rand()%40)/10.0f;
    }
}

void updateFireflies()
{
    for(int k=0;k<15;k++)
    {
        ffTimer[k] -= 0.01f;
        if(ffTimer[k] < 0)
        {
            ffBright[k] = 0.0f;
            if(ffTimer[k] < -1.5f)
            {
                ffTimer[k] = (rand()%30)/10.0f + 0.5f;
                ffX[k] = (rand()%380) - 190;
                ffY[k] = (rand()%70) - 10;
                ffBright[k] = 1.0f;
            }
        }
        else
        {
            ffBright[k] = ffTimer[k] > 0.5f ? 1.0f : ffTimer[k]*2.0f;
            ffX[k] += sin(starBlink*0.01f + k) * 0.05f;
            ffY[k] += cos(starBlink*0.008f + k*1.5f) * 0.03f;
        }
    }
}

void drawFireflies()
{
    if(!isNight) return;
    if(isRaining) return;   // fireflies don't come out in the rain

    glPointSize(3.5f);
    glBegin(GL_POINTS);
    for(int k=0;k<15;k++)
    {
        glColor4f(0.70f,1.0f,0.40f, ffBright[k]);
        glVertex2f(ffX[k], ffY[k]);
    }
    glEnd();

    // soft glow around bright fireflies
    for(int k=0;k<15;k++)
    {
        if(ffBright[k] > 0.5f)
        {
            glColor4f(0.60f,0.90f,0.30f, ffBright[k]*0.15f);
            double gx = ffX[k], gy = ffY[k];
            glBegin(GL_TRIANGLE_FAN);
                glVertex2d(gx,gy);
                for(int a=0;a<=360;a+=20)
                {
                    double ang = a*3.1416/180;
                    glVertex2d(gx+cos(ang)*4, gy+sin(ang)*4);
                }
            glEnd();
        }
    }
    glPointSize(2.0);
}

// grass & stones (moved onto the field,
//clear of the river water)
void drawEdgeGrass()
{
    glLineWidth(1.2f);
    for(int gx=-200; gx<200; gx+=4)
    {
        float h = (float)((gx*17+43)%10) + 5.0f;
        float lean = sin(gx*0.35f) * 3.0f;

        // darker back blades
        glColor3ub(25,140,25);
        glBegin(GL_LINES);
            glVertex2f((float)gx, -40);
            glVertex2f(gx + lean*0.5f, -40 + h*0.7f);
        glEnd();

        // brighter front blades
        glColor3ub(50,190,50);
        glBegin(GL_LINES);
            glVertex2f(gx+1.0f, -40);
            glVertex2f(gx + lean + 1.0f, -40 + h);
        glEnd();
    }
    glLineWidth(1.0f);
}

float stoneX[7] = {-165,-95,-30,45,110,150,5};
float stoneY[7] = {-32,-36,-30,-35,-31,-37,-28};
float stoneR[7] = {6,4.5,7,3.5,5,3,4};

void drawStones()
{
    for(int k=0;k<7;k++)
    {
        float rr = stoneR[k];

        // stone shadow
        glColor3ub(80,78,74);
        drawEllipseShape(stoneX[k]+1, stoneY[k]-0.5, rr*1.05, rr*0.65);

        // stone body
        glColor3ub(128,124,116);
        drawEllipseShape(stoneX[k], stoneY[k], rr, rr*0.65);

        // stone highlight
        glColor3ub(168,164,156);
        drawEllipseShape(stoneX[k]-rr*0.25, stoneY[k]+rr*0.15, rr*0.35, rr*0.20);
    }
}

// sandy foreground (near riverbank)
// Brings the river to the middle of the scene: green/tree bank above the water,
// sandy bank with grass tufts and stones below it (like the reference image).
float sandStoneX[9] = {-180,-140,-90,-40,10,60,110,150,180};
float sandStoneY[9] = {-150,-175,-140,-165,-185,-145,-170,-190,-150};
float sandStoneR[9] = {6,4,7,3.5,5,4.5,6.5,3,5};

void drawSandGround()
{
    // sandy base with a soft gradient
    glBegin(GL_QUADS);
        glColor3ub(216,187,127);
        glVertex2i(-200,-100);
        glVertex2i(200,-100);
        glColor3ub(200,171,110);
        glVertex2i(200,-200);
        glVertex2i(-200,-200);
    glEnd();

    // sand texture dots
    glColor3ub(190,160,100);
    glPointSize(1.5f);
    glBegin(GL_POINTS);
    for(int gx=-195; gx<200; gx+=4)
        for(int gy=-195; gy<-105; gy+=4)
            if((gx*11 + gy*7)%9==0)
                glVertex2f((float)gx,(float)gy);
    glEnd();
}
//================ 3D BAMBOO PLATFORM =================//

void drawBambooPlatform()
{
    // Platform position on sandy ground
    float x = 55;
    float y = -145;

    // Make the whole platform smaller
    glPushMatrix();
    glTranslatef(x,y,0);
    glScalef(0.65f,0.65f,1.0f);
    glTranslatef(-x,-y,0);


    // ---------- Ground Shadow ----------
    if(!isNight)
{
    glColor4f(0.0f,0.0f,0.0f,0.25f);
    drawEllipseShape(x+55,y-8,65,10);
}


    // ---------- Platform Top ----------
    glColor3ub(170,115,45);

    glBegin(GL_POLYGON);
        glVertex2f(x,y);
        glVertex2f(x+110,y);
        glVertex2f(x+125,y+12);
        glVertex2f(x+15,y+12);
    glEnd();


    // ---------- Front Side ----------
    glColor3ub(125,80,30);

    glBegin(GL_POLYGON);
        glVertex2f(x,y);
        glVertex2f(x+110,y);
        glVertex2f(x+110,y-10);
        glVertex2f(x,y-10);
    glEnd();


    // ---------- Right 3D Side ----------
    glColor3ub(100,65,25);

    glBegin(GL_POLYGON);
        glVertex2f(x+110,y);
        glVertex2f(x+125,y+12);
        glVertex2f(x+125,y+2);
        glVertex2f(x+110,y-10);
    glEnd();


    // ---------- Bamboo Floor Lines ----------
    glColor3ub(210,155,65);

    glLineWidth(3.0f);

    for(int i=10;i<110;i+=15)
    {
        glBegin(GL_LINES);

            glVertex2f(x+i,y);
            glVertex2f(x+i+15,y+11);

        glEnd();
    }

    glLineWidth(1.0f);


    // ---------- Bamboo Legs ----------
    glColor3ub(115,75,25);


    // Left front leg
    glBegin(GL_POLYGON);

        glVertex2f(x+12,y-10);
        glVertex2f(x+19,y-10);
        glVertex2f(x+19,y-65);
        glVertex2f(x+12,y-65);

    glEnd();


    // Right front leg
    glBegin(GL_POLYGON);

        glVertex2f(x+95,y-10);
        glVertex2f(x+102,y-10);
        glVertex2f(x+102,y-65);
        glVertex2f(x+95,y-65);

    glEnd();


    // Middle leg
    glBegin(GL_POLYGON);

        glVertex2f(x+53,y-10);
        glVertex2f(x+60,y-10);
        glVertex2f(x+60,y-65);
        glVertex2f(x+53,y-65);

    glEnd();


    // ---------- Back Supports ----------
    glColor3ub(145,95,30);

    glBegin(GL_LINES);

        glVertex2f(x+15,y+8);
        glVertex2f(x+28,y-55);

        glVertex2f(x+108,y+8);
        glVertex2f(x+92,y-55);

    glEnd();


    // ---------- Railing ----------
    glColor3ub(175,115,35);

    glLineWidth(4.0f);


    // Left post
    glBegin(GL_LINES);

        glVertex2f(x+8,y+5);
        glVertex2f(x+8,y+45);

    glEnd();


    // Right post
    glBegin(GL_LINES);

        glVertex2f(x+112,y+8);
        glVertex2f(x+112,y+48);

    glEnd();


    // Back railing
    glBegin(GL_LINES);

        glVertex2f(x+8,y+45);
        glVertex2f(x+112,y+48);

    glEnd();


    // Middle railing posts
    glBegin(GL_LINES);

        glVertex2f(x+40,y+7);
        glVertex2f(x+40,y+46);

        glVertex2f(x+75,y+9);
        glVertex2f(x+75,y+47);

    glEnd();


    glLineWidth(1.0f);

    // End of scaling
    glPopMatrix();
}
// ================= TENT =================
void drawTent(float x, float y, float s)
{
    // ---------- Tent Shadow ----------
    glColor4f(0.0f, 0.0f, 0.0f, 0.20f);
    drawEllipseShape(x+45*s, y-5*s, 55*s, 8*s);

    // ---------- Left Tent Side ----------
    glColor3ub(190, 60, 50);

    glBegin(GL_POLYGON);
        glVertex2f(x, y);
        glVertex2f(x+45*s, y+55*s);
        glVertex2f(x+45*s, y);
    glEnd();

    // ---------- Main Tent Front ----------
    glColor3ub(230, 90, 70);

    glBegin(GL_POLYGON);
        glVertex2f(x+45*s, y);
        glVertex2f(x+45*s, y+55*s);
        glVertex2f(x+90*s, y);
    glEnd();

    // ---------- Right 3D Side ----------
    glColor3ub(155, 45, 40);

    glBegin(GL_POLYGON);
        glVertex2f(x+90*s, y);
        glVertex2f(x+45*s, y+55*s);
        glVertex2f(x+105*s, y+8*s);
        glVertex2f(x+105*s, y);
    glEnd();

    // ---------- Tent Door ----------
    glColor3ub(90, 55, 40);

    glBegin(GL_POLYGON);
        glVertex2f(x+32*s, y);
        glVertex2f(x+45*s, y+25*s);
        glVertex2f(x+58*s, y);
    glEnd();

    // ---------- Tent Pole ----------
    glColor3ub(110, 70, 35);

    glLineWidth(3.0f);

    glBegin(GL_LINES);
        glVertex2f(x+45*s, y+55*s);
        glVertex2f(x+45*s, y+68*s);
    glEnd();

    glLineWidth(1.0f);

    // ---------- Small Flag ----------
    glColor3ub(255, 210, 40);

    glBegin(GL_TRIANGLES);
        glVertex2f(x+45*s, y+68*s);
        glVertex2f(x+62*s, y+63*s);
        glVertex2f(x+45*s, y+58*s);
    glEnd();
}

// ================= BONFIRE =================
void drawBonfire(float x, float y, float s)
{
    // ---------- Ground Shadow ----------
    glColor4f(0.0f, 0.0f, 0.0f, 0.25f);
    drawEllipseShape(x, y-5*s, 38*s, 7*s);

    // ---------- Firewood - Back Log ----------
    glColor3ub(90, 50, 25);

    glLineWidth(8.0f);

    glBegin(GL_LINES);
        glVertex2f(x-25*s, y);
        glVertex2f(x+22*s, y+15*s);
    glEnd();

    // ---------- Firewood - Front Log ----------
    glBegin(GL_LINES);
        glVertex2f(x-22*s, y+14*s);
        glVertex2f(x+25*s, y);
    glEnd();

    glLineWidth(1.0f);

    // ---------- Log Ends ----------
    glColor3ub(55, 35, 20);

    drawDisc(x-25*s, y, 5*s);
    drawDisc(x+25*s, y, 5*s);

    // ==================================================
    // FIRE - ONLY AT NIGHT AND NOT DURING RAIN
    // ==================================================

    if(isNight && !isRaining)
    {
        // Animation time
        float t = glutGet(GLUT_ELAPSED_TIME) * 0.01f;

        float flicker = sin(t) * 3.0f;

        // ---------- Outer Flame ----------
        glColor3ub(255, 90, 10);

        glBegin(GL_TRIANGLES);

            glVertex2f(x, y+8*s);

            glVertex2f(x-20*s, y+38*s+flicker*s);

            glVertex2f(x-5*s, y+65*s);

            glVertex2f(x, y+8*s);

            glVertex2f(x+5*s, y+65*s);

            glVertex2f(x+20*s, y+38*s-flicker*s);

        glEnd();

        // ---------- Middle Flame ----------
        glColor3ub(255, 170, 20);

        glBegin(GL_TRIANGLES);

            glVertex2f(x, y+10*s);
            glVertex2f(x-13*s, y+42*s);
            glVertex2f(x, y+58*s+flicker*s);

            glVertex2f(x, y+10*s);
            glVertex2f(x, y+58*s+flicker*s);
            glVertex2f(x+13*s, y+42*s);

        glEnd();

        // ---------- Inner Flame ----------
        glColor3ub(255, 235, 80);

        glBegin(GL_TRIANGLES);

            glVertex2f(x, y+12*s);
            glVertex2f(x-7*s, y+38*s);
            glVertex2f(x, y+50*s-flicker*s);

            glVertex2f(x, y+12*s);
            glVertex2f(x, y+50*s-flicker*s);
            glVertex2f(x+7*s, y+38*s);

        glEnd();
    }
}
void grassTuft(float x, float y, float h)
{
    glColor3ub(20,120,20);
    glBegin(GL_LINES);
        glVertex2f(x,y);        glVertex2f(x,y+h);
        glVertex2f(x,y+h*0.6f); glVertex2f(x-h*0.4f,y+h);
        glVertex2f(x,y+h*0.6f); glVertex2f(x+h*0.4f,y+h);
    glEnd();
}

// 3D grass and flowers on sandy ground
void drawSandGrassTufts()
{
    glLineWidth(2.0f);

    for(int gx=-190; gx<195; gx+=25)
    {
        float gy = -110 - (float)((gx*7)%25);
        float h = 8.0f + (float)((gx*13)%6);

        // ---------- 3D GRASS ----------
        glColor3ub(20,120,20);

        // Main grass blade
        glBegin(GL_LINES);
            glVertex2f(gx,gy);
            glVertex2f(gx,gy+h);

            glVertex2f(gx,gy+h*0.45f);
            glVertex2f(gx-5,gy+h);

            glVertex2f(gx,gy+h*0.45f);
            glVertex2f(gx+5,gy+h);
        glEnd();

        // Dark side for 3D depth
        glColor3ub(10,80,15);

        glBegin(GL_LINES);
            glVertex2f(gx-2,gy);
            glVertex2f(gx-4,gy+h*0.8f);

            glVertex2f(gx+2,gy);
            glVertex2f(gx+5,gy+h*0.7f);
        glEnd();


        // ---------- FLOWER WITH GRASS ----------
        float fx = gx + 2;
        float fy = gy + h + 3;

        // Flower stem
        glColor3ub(20,100,20);

        glBegin(GL_LINES);
            glVertex2f(gx,gy+h*0.4f);
            glVertex2f(fx,fy);
        glEnd();


        // Flower petals
        glColor3ub(255,80,120);

        drawDisc(fx-3,fy,2.5);
        drawDisc(fx+3,fy,2.5);
        drawDisc(fx,fy+3,2.5);
        drawDisc(fx,fy-3,2.5);

        // Flower center
        glColor3ub(255,215,0);
        drawDisc(fx,fy,1.8);
    // ---------- EXTRA GRASS + FLOWERS ON LEFT SIDE ----------
    for(int gx=-195; gx<=-100; gx+=12)
    {
        float gy = -120 - (float)((gx*5)%30);
        float h = 10.0f + (float)((gx*7)%7);

        // Grass
        glColor3ub(15,130,20);

        glBegin(GL_LINES);
            glVertex2f(gx,gy);
            glVertex2f(gx-2,gy+h);

            glVertex2f(gx,gy);
            glVertex2f(gx+4,gy+h*0.8f);

            glVertex2f(gx,gy+h*0.3f);
            glVertex2f(gx+6,gy+h);
        glEnd();

        // Flower stem
        glColor3ub(20,100,20);

        glBegin(GL_LINES);
            glVertex2f(gx,gy+h*0.4f);
            glVertex2f(gx+2,gy+h+3);
        glEnd();

        // Flower
        float fx = gx+2;
        float fy = gy+h+3;

        glColor3ub(255,80,120);

        drawDisc(fx-3,fy,2.5);
        drawDisc(fx+3,fy,2.5);
        drawDisc(fx,fy+3,2.5);
        drawDisc(fx,fy-3,2.5);

        // Center
        glColor3ub(255,215,0);
        drawDisc(fx,fy,1.8);
    }
    }

    glLineWidth(1.0f);
}
void drawBucket(float x, float y)
{
    if(!isNight)
{
    glColor4f(0.0f,0.0f,0.0f,0.25f);
    drawEllipseShape(x+1,y-2,9,3);
}
    // ---------- Bucket Body ----------
    glColor3ub(70,130,180);

    glBegin(GL_POLYGON);
        glVertex2f(x-8,y+12);
        glVertex2f(x+8,y+12);
        glVertex2f(x+6,y-5);
        glVertex2f(x-6,y-5);
    glEnd();

    // ---------- 3D Right Side ----------
    glColor3ub(45,90,140);

    glBegin(GL_POLYGON);
        glVertex2f(x+8,y+12);
        glVertex2f(x+12,y+9);
        glVertex2f(x+10,y-3);
        glVertex2f(x+6,y-5);
    glEnd();

    // ---------- Bucket Top ----------
    glColor3ub(100,160,200);
    drawEllipseShape(x,y+12,8,3);

    // ---------- Water inside ----------
    glColor3ub(30,144,255);
    drawEllipseShape(x,y+12,6,2);

    // ---------- Bucket Handle ----------
    glColor3ub(80,80,80);
    glLineWidth(2.0f);

    glBegin(GL_LINE_STRIP);
        glVertex2f(x-7,y+13);
        glVertex2f(x-6,y+22);
        glVertex2f(x,y+27);
        glVertex2f(x+6,y+22);
        glVertex2f(x+7,y+13);
    glEnd();

    glLineWidth(1.0f);

    // ---------- Highlight ----------
    glColor3ub(170,210,230);

    glBegin(GL_LINE_STRIP);
        glVertex2f(x-5,y+8);
        glVertex2f(x-4,y);
    glEnd();
}

void drawSandStones()
{
    for(int k=0;k<9;k++)
    {
        float rr = sandStoneR[k];

        glColor3ub(90,88,84);
        drawEllipseShape(sandStoneX[k]+1, sandStoneY[k]-0.5, rr*1.05, rr*0.65);

        glColor3ub(140,136,128);
        drawEllipseShape(sandStoneX[k], sandStoneY[k], rr, rr*0.65);

        glColor3ub(180,176,168);
        drawEllipseShape(sandStoneX[k]-rr*0.25, sandStoneY[k]+rr*0.15, rr*0.35, rr*0.20);
    }
}

//====================== BUSH ======================//

void drawBush(float x, float y, float scale)
{
    glPushMatrix();

    glTranslatef(x, y, 0);
    glScalef(scale, scale, 1);

    // Shadow (Day only)
    if(!isNight)
    {
        glColor4f(0.0f,0.0f,0.0f,0.18f);

        drawEllipseShape(0,-5,12,4);
    }

    // Bush leaves
    glColor3ub(30,140,30);

    drawEllipseShape(-8,0,8,6);

    drawEllipseShape(0,5,9,7);

    drawEllipseShape(8,0,8,6);

    drawEllipseShape(0,-2,10,6);

    glPopMatrix();
}
//====================== DAY SHADOW ======================//

void drawDayShadow()
{

    if(isNight)
        return;


    // Big tree shadow
    glColor4f(0.0f,0.0f,0.0f,0.25f);

    drawEllipseShape(-150,35,55,12);


    // House 1 shadow
    glColor4f(0.0f,0.0f,0.0f,0.20f);

    drawEllipseShape(-90,32,55,10);


    // House 2 shadow
    glColor4f(0.0f,0.0f,0.0f,0.20f);

    drawEllipseShape(-20,20,45,10);


    // Tubewell shadow
    glColor4f(0.0f,0.0f,0.0f,0.18f);

    drawEllipseShape(125,5,25,7);

}
//================ RICE PADDY FIELD ================//

void drawRicePaddyField()
{
    // Paddy field base
    glColor3ub(95,150,45);
    glBegin(GL_QUADS);
        glVertex2i(40,100);
        glVertex2i(200,100);
        glVertex2i(200,40);
        glVertex2i(40,40);
    glEnd();

    // Paddy rows
    glColor3ub(55,120,35);

    glLineWidth(2.0f);

    for(int x=45; x<200; x+=12)
    {
        glBegin(GL_LINES);

            glVertex2i(x,42);
            glVertex2i(x,95);

        glEnd();
    }

    // Rice plants
    glColor3ub(35,110,25);

    for(int x=48; x<200; x+=12)
    {
        for(int y=45; y<95; y+=12)
        {
            glBegin(GL_LINES);

                glVertex2i(x,y);
                glVertex2i(x-3,y+7);

                glVertex2i(x,y);
                glVertex2i(x+3,y+8);

            glEnd();
        }
    }

    glLineWidth(1.0f);
}
/// =================3D COW =================
void drawCow(float x, float y, float s)
{
    glPushMatrix();

    glTranslatef(x, y, 0);
    glScalef(s, s, 1.0f);

    // ================= BODY =================
    glColor3ub(125,75,35);
    drawEllipseShape(0, 20, 42, 19);

    // Body highlight
    glColor3ub(155,95,45);
    drawEllipseShape(-5, 23, 30, 13);

    // Black body spot
    glColor3ub(60,40,25);
    drawEllipseShape(-8, 27, 12, 7);


    // ================= NECK =================
    // Slightly inside body so NO GAP
    glColor3ub(125,75,35);
    drawEllipseShape(35, 32, 14, 18);


    // ================= HEAD =================
    // Head overlaps neck
    glColor3ub(125,75,35);
    drawEllipseShape(51, 45, 19, 15);


    // ================= FACE =================
    // White face patch
    glColor3ub(245,235,205);
    drawEllipseShape(57, 46, 12, 10);

    // Muzzle
    glColor3ub(180,130,85);
    drawEllipseShape(64, 42, 10, 6);

    // Nose
    glColor3ub(55,40,30);
    drawEllipseShape(66, 43, 2.5, 2);

    // Mouth
    glColor3ub(30,20,15);
    glLineWidth(1.5f);

    glBegin(GL_LINES);
        glVertex2f(61,39);
        glVertex2f(67,39);
    glEnd();


    // ================= EYE =================
    glColor3ub(30,20,15);
    drawEllipseShape(57, 51, 2.5, 2.5);

    // Eye shine
    glColor3ub(255,255,255);
    drawEllipseShape(57.7, 51.7, 0.7, 0.7);


    // ================= HORNS =================
    glColor3ub(220,190,120);

    glBegin(GL_TRIANGLES);
        glVertex2f(44,56);
        glVertex2f(39,67);
        glVertex2f(48,59);
    glEnd();

    glBegin(GL_TRIANGLES);
        glVertex2f(60,57);
        glVertex2f(66,66);
        glVertex2f(65,56);
    glEnd();


    // ================= EAR =================
    glColor3ub(105,60,30);

    glBegin(GL_TRIANGLES);
        glVertex2f(43,53);
        glVertex2f(32,58);
        glVertex2f(42,48);
    glEnd();

    glBegin(GL_TRIANGLES);
        glVertex2f(63,53);
        glVertex2f(73,57);
        glVertex2f(64,48);
    glEnd();


    // ================= FRONT LEGS =================
    // Start INSIDE body to remove gap
    glColor3ub(100,60,30);

    // Front leg 1
    glBegin(GL_POLYGON);
        glVertex2f(25,10);
        glVertex2f(34,10);
        glVertex2f(34,-18);
        glVertex2f(25,-18);
    glEnd();

    // Front leg 2
    glBegin(GL_POLYGON);
        glVertex2f(36,10);
        glVertex2f(45,10);
        glVertex2f(45,-18);
        glVertex2f(36,-18);
    glEnd();


    // ================= FRONT HOOVES =================
    glColor3ub(45,35,25);

    glBegin(GL_POLYGON);
        glVertex2f(24,-18);
        glVertex2f(35,-18);
        glVertex2f(35,-21);
        glVertex2f(24,-21);
    glEnd();

    glBegin(GL_POLYGON);
        glVertex2f(35,-18);
        glVertex2f(46,-18);
        glVertex2f(46,-21);
        glVertex2f(35,-21);
    glEnd();


    // ================= BACK LEGS =================
    glColor3ub(95,55,28);

    glBegin(GL_POLYGON);
        glVertex2f(-28,10);
        glVertex2f(-19,10);
        glVertex2f(-19,-18);
        glVertex2f(-28,-18);
    glEnd();

    glBegin(GL_POLYGON);
        glVertex2f(-17,10);
        glVertex2f(-8,10);
        glVertex2f(-8,-18);
        glVertex2f(-17,-18);
    glEnd();


    // ================= BACK HOOVES =================
    glColor3ub(45,35,25);

    glBegin(GL_POLYGON);
        glVertex2f(-29,-18);
        glVertex2f(-18,-18);
        glVertex2f(-18,-21);
        glVertex2f(-29,-21);
    glEnd();

    glBegin(GL_POLYGON);
        glVertex2f(-18,-18);
        glVertex2f(-7,-18);
        glVertex2f(-7,-21);
        glVertex2f(-18,-21);
    glEnd();


    // ================= TAIL =================
    glColor3ub(105,60,30);

    glLineWidth(4.0f);

    glBegin(GL_LINES);
        glVertex2f(-39,25);
        glVertex2f(-55,15);
    glEnd();

    // Tail hair
    glColor3ub(45,30,20);

    glBegin(GL_TRIANGLES);
        glVertex2f(-54,16);
        glVertex2f(-61,10);
        glVertex2f(-57,19);
    glEnd();


    glLineWidth(1.0f);

    glPopMatrix();
}
//river ripples (flow direction)
float waterOffset = 0.0f;

void drawRiverRipples()
{
    glColor4f(1.0f,1.0f,1.0f,0.5f);
    for(float ry=-65; ry>-95; ry-=15)
    {
        glBegin(GL_LINE_STRIP);
        for(int xx=-200; xx<=200; xx+=8)
        {
            float yy = ry + sin((xx+waterOffset)*0.05f)*3.0f;
            glVertex2f((float)xx, yy);
        }
        glEnd();

        // second, fainter wave layer for depth
        glColor4f(1.0f,1.0f,1.0f,0.25f);
        glBegin(GL_LINE_STRIP);
        for(int xx=-200; xx<=200; xx+=8)
        {
            float yy = ry + 6 + sin((xx+waterOffset*0.7f)*0.08f)*2.0f;
            glVertex2f((float)xx, yy);
        }
        glEnd();
        glColor4f(1.0f,1.0f,1.0f,0.5f);
    }
}
//====================== FISH ANIMATION ======================//

void drawFish(float x, float y, float scale)
{
    glPushMatrix();

    glTranslatef(x,y,0);
    glScalef(scale,scale,1);


    // Fish body
    glColor3ub(255,140,0);

    glBegin(GL_TRIANGLE_FAN);
        for(int i=0;i<=360;i++)
        {
            float a=i*3.1416/180;
            glVertex2f(cos(a)*8,sin(a)*4);
        }
    glEnd();


    // Tail
    glColor3ub(255,200,0);

    glBegin(GL_TRIANGLES);
        glVertex2f(-7,0);
        glVertex2f(-15,6);
        glVertex2f(-15,-6);
    glEnd();


    // Eye
    glColor3ub(0,0,0);

    drawCircle(5,2,1);


    glPopMatrix();
}



void fishAnimation()
{
    if(isRaining)
        return;


    drawFish(fishX1,-70,1.0);
    drawFish(fishX2,-80,0.7);
    drawFish(fishX3,-60,0.8);


    fishX1 +=0.08;
    fishX2 -=0.06;
    fishX3 +=0.05;


    if(fishX1>220)
        fishX1=-220;


    if(fishX2<-220)
        fishX2=220;


    if(fishX3>220)
        fishX3=-220;

}

//====================== ELECTRIC POLE ======================//

void drawElectricPole(float x, float y)
{
    // Pole
    glColor3ub(90,90,90);

    glBegin(GL_POLYGON);
    glVertex2f(x,y-30);
    glVertex2f(x+5,y-30);
    glVertex2f(x+5,y+100);
    glVertex2f(x,y+100);
    glEnd();


    // Top cross bar
    glColor3ub(70,70,70);

    glBegin(GL_LINES);
        glVertex2f(x-35,y+80);
        glVertex2f(x+40,y+80);

        glVertex2f(x-30,y+95);
        glVertex2f(x+35,y+95);
    glEnd();


   // Electric wires
glColor3ub(0,0,0);

glBegin(GL_LINE_STRIP);
    glVertex2f(x-15,y+80);
    glVertex2f(x-50,y+65);
    glVertex2f(x-90,y+75);
glEnd();


glBegin(GL_LINE_STRIP);
    glVertex2f(x+35,y+80);
    glVertex2f(x+85,y+65);
    glVertex2f(x+135,y+75);
glEnd();



    // Bulb
    if(isNight)
    {
        // glow
        glColor4f(1.0f,1.0f,0.3f,0.25f);
        drawDisc(x+2,y+70,12);


        // light
        glColor3ub(255,255,100);
        drawDisc(x+2,y+70,5);
    }
    else
    {
        // off bulb
        glColor3ub(80,80,80);
        drawDisc(x+2,y+70,4);
    }

}

//Boat scelaing control

//KEYBOARD ONLY BOAT
void keyboard(unsigned char key, int x, int y)
{
    switch(key)
    {
        case '+':
            scaleFactor += 0.05f;
            break;

        case '-':
            scaleFactor -= 0.05f;
            if(scaleFactor < 0.2f) scaleFactor = 0.2f;
            break;

        case 'r':
            scaleFactor = 1.0f;
            break;

        case 's':
            isRaining = true;
            break;

        case 't':
            isRaining = false;
            break;

        // CAR CONTROL
        case 'a':   // left
            carX -= 5;
            break;

        case 'd':   // right
            carX += 5;
            break;

        case 'h':   // car horn
            playOnceSound("C:\\Users\\This PC\\Documents\\lab\\horn.wav", "hornSound");
            break;

        // MANUAL DAY / NIGHT CONTROL
        case 'D':
            isNight = false;
            sx = -150;
            cycleStart = glutGet(GLUT_ELAPSED_TIME);
            break;

        case 'N':
            isNight = true;
            mx = 220;
            cycleStart = glutGet(GLUT_ELAPSED_TIME);
            break;
    }
}

void display()
{

    glClear(GL_COLOR_BUFFER_BIT);
//sky
if(isNight)
{
    glColor3ub(10,10,40);   // dark night sky
}
else
{
    glColor3ub(135,206,250); // day sky
}

glRecti(-200,300,200,100);

if(isNight)
{
    // twinkling star field (hidden while raining — a rainy night sky is cloud-covered)
    if(!isRaining)
        drawStarsTwinkle();
}
else
{
    // birds only fly during the day
    birdFlock();
}




//field
    glBegin(GL_POLYGON);
        glColor3ub(0,100,0);//green

        glVertex2i(-200,100);
        glVertex2i(-100,160);
        glVertex2i(0,100);
        glVertex2i(50,70);
        glVertex2i(100,180);
        glVertex2i(200,100);

        glColor3ub(255,215,0);//gold
        glVertex2i(200,-200);
        glVertex2i(-200,-200);

        glColor3ub(255,215,0);//gold
        glVertex2i(-200,100);



    glEnd();

// Rice paddy field in the background
drawRicePaddyField();


//windmill call
    windmill(150, 100);

//SUN
if(!isNight)
{
    // soft glow halo behind the sun (perfect round circles, core on top)
    glColor4f(1.0f,0.95f,0.50f,0.15f);
    drawDisc(sx,250,20);

    glColor4f(1.0f,0.92f,0.30f,0.25f);
    drawDisc(sx,250,15);

    glColor3ub(255,215,0);
    drawDisc(sx,250,9);

    // rotating sun rays
    glPushMatrix();
    glTranslatef(sx,250,0);
    glRotatef(solarAngle, 0, 0, 1);
    glColor3ub(255,230,60);
    glLineWidth(1.5f);
    glBegin(GL_LINES);
    for(int k=0;k<12;k++)
    {
        double a = (k*3.1416)/6.0;
        glVertex2f(cos(a)*12, sin(a)*12);
        glVertex2f(cos(a)*18, sin(a)*18);
    }
    glEnd();
    glLineWidth(1.0f);
    glPopMatrix();

}
else
{
    // moon hides behind rain clouds — no moon while raining
    if(!isRaining)
        moon(mx,250);
}
//fence
    int x=0;
    for(int i=0;i<39;i++)
    {
        fence(x);
        x+=10;
    }

    glColor3ub(184,134,11);

    glRecti(-200,120,200,115);
    glRecti(-200,100,200,95);
    glRecti(-200,85,200,80);


    solarLight(-180, 120);
solarLight(-120, 120);
solarLight(-60, 120);
solarLight(0, 120);
solarLight(60, 120);
solarLight(120, 120);
// Day shadows
drawDayShadow();
//TREE
    glColor3ub(139,69,19);//
    glRecti(-20,200,-13,140);
    glColor3ub(0,100,0);
    sun(-30,190);
    sun(0,190);
    sun(-10,210);
    sun(-30,175);
    sun(-0,170);
    glBegin(GL_POLYGON); // Main Tree // first part
        glColor3ub(139,69,19);//
        glVertex2i(-170,160);
        glVertex2i(-168,120);
        glColor3ub(139,69,19);//
        glVertex2i(-178,40);
        glVertex2i(-145,40);
        glColor3ub(139,69,19);//
        glVertex2i(-153,120);
        glVertex2i(-150,160);
        glVertex2i(-170,160);
    glEnd();
    glBegin(GL_POLYGON);  // Main Tree // second part
        glColor3ub(139,69,19);//
        glVertex2i(-153,100);
        glVertex2i(-100,200);
        glVertex2i(-95,200);
        glVertex2i(-153,80);
        glVertex2i(-153,100);
    glEnd();
    glBegin(GL_POLYGON);  // Main Tree // third part
        glColor3ub(139,69,19);//
        glVertex2i(-170,160);
        glVertex2i(-185,210);
        glVertex2i(-190,210);
        glVertex2i(-168,90);
        glVertex2i(-170,160);

    glEnd();
    glBegin(GL_POLYGON);  // Main Tree // fourth part
        glColor3ub(139,69,19);//
        glVertex2i(-160,160);
        glVertex2i(-150,210);
        glVertex2i(-140,210);
        glVertex2i(-150,160);
        glVertex2i(-160,160);


    glEnd();
    glColor3ub(0,128,0);//leaf
        sun(-95,200);
        sun(-80,180);
        sun(-110,180);
        sun(-120,200);

        sun(-150,200);
        sun(-130,180);
        sun(-125,220);
        sun(-140,230);

        sun(-190,210);
        sun(-180,200);
        sun(-175,225);
        sun(-195,190);

        // Apples on tree
drawApple(-95,200);
drawApple(-110,180);
drawApple(-130,200);
drawApple(-175,210);
drawApple(-140,230);
// Bush
drawBush(-125,40,1.0f);
drawBush(-90,35,0.8f);
drawBush(-45,38,0.9f);
drawBush(15,35,0.8f);

    // hen and chicks under the front tree (day only)
    if(!isNight)
        henFamily();


//TUBEWELL
    glBegin(GL_POLYGON);  // First part

        glColor3ub(0,100,0);//
        glVertex2i(115,65);
        glVertex2i(95,5);
        glVertex2i(145,5);
        glVertex2i(165,65);
        glVertex2i(115,65);

    glEnd();
    glBegin(GL_POLYGON);  // second part

        glColor3ub(143,188,143);//
        glVertex2i(120,58);
        glVertex2i(104,13);
        glVertex2i(140,12);
        glVertex2i(155,58);
        glVertex2i(120,58);

    glEnd();
    glColor3ub(0,0,0);// third part
        glRecti(95,5,145,-6);

    glBegin(GL_POLYGON);  // fourth  part
        glColor3ub(0,0,0);//
        glVertex2i(165,65);
        glVertex2i(166,55);
        glVertex2i(145,-6);
        glVertex2i(145,5);
        glVertex2i(165,65);
    glEnd();
    glBegin(GL_POLYGON);  // tubewell 1st part
        glColor3ub(184,134,11);
        glVertex2i(120,85);
        glVertex2i(120,30);
        glVertex2i(125,28);
        glVertex2i(130,30);
        glVertex2i(130,85);
        glVertex2i(125,87);
        glVertex2i(120,85);
    glEnd();
    glBegin(GL_POLYGON);  // tubewell second part
        glColor3ub(255,215,0);//golden rod
        glVertex2i(120,85);
        glVertex2i(125,80);
        glVertex2i(130,85);
        glVertex2i(125,87);
        glVertex2i(120,85);

    glEnd();
    glColor3ub(205,133,63);//golden rod // tubewell third part
    glRecti(123,100,126,85);

    glBegin(GL_POLYGON);  // tubewell fourth part
        glColor3ub(139,69,19);//saddle brown
        glVertex2i(126,100);
        glVertex2i(128,102);
        glVertex2i(128,110);
        glVertex2i(126,113);
        glVertex2i(124,111);
        glVertex2i(100,80);
        glVertex2i(90,70);
        glVertex2i(90,65);
        glVertex2i(100,73);
        glVertex2i(126,100);
    glEnd();
    glBegin(GL_POLYGON);  // tubewell 5th part
        glColor3ub(210,105,30);//golden rod
        glVertex2i(130,70);
        glVertex2i(140,70);
        glVertex2i(140,50);
        glVertex2i(136,50);
        glVertex2i(136,60);
        glVertex2i(130,60);
        glVertex2i(130,70);

    glEnd();
    glColor3ub(210,105,30);//golden rod //tubewell last part
    glRecti(123,29,127,20);
    glColor3ub(139,69,19);//saddle brown
    glRecti(118,22,132,14);
    // 3D bucket beside tubewell
    drawBucket(160, 8);
    // Electric pole
    drawElectricPole(80,55);
    //HOUSE one
    glBegin(GL_POLYGON);  // first Part
        glColor3ub(128,0,0);//gray
        glVertex2i(-58,115);
        glVertex2i(-75,145);
        glVertex2i(-115,150);//point
        glVertex2i(-90,100);
        glVertex2i(-62,100);
        glVertex2i(-58,115);

    glEnd();
    glBegin(GL_POLYGON);  // second Part
        glColor3ub(120,0,0);//maroon
        glVertex2i(-115,150);
        glVertex2i(-130,100);
        glVertex2i(-120,100);//point
        glVertex2i(-108,137);//point
        glVertex2i(-115,150);
    glEnd();
    glBegin(GL_POLYGON);  // third Part
        glColor3ub(46,139,87);//
        glVertex2i(-108,137);
        glVertex2i(-120,100);
        glVertex2i(-120,45);
        glVertex2i(-90,40);//point
        glVertex2i(-90,100);
        glVertex2i(-108,137);
    glEnd();


    glBegin(GL_POLYGON);  // fourth Part
        glColor3ub(143,188,143);//
        glVertex2i(-90,40);
        glVertex2i(-60,45);
        glVertex2i(-60,100);
        glVertex2i(-90,100);

    glEnd();
    glColor3ub(120,0,0);//maroon // Door One
        glRecti(-75,80,-65,40);
    glColor3ub(120,0,0);//maroon // Door One
        glRecti(-110,90,-100,70);
    glBegin(GL_POLYGON);  // third Part (lower part 1)
        glColor3ub(0,0,0);//
        glVertex2i(-90,40);
        glVertex2i(-123,45);
        glVertex2i(-123,35);
        glVertex2i(-90,30);
        glVertex2i(-90,40);

    glEnd();
    glBegin(GL_POLYGON);  // third Part (lower part 2)
        glColor3ub(0,0,0);//
        glVertex2i(-90,40);
        glVertex2i(-55,45);
        glVertex2i(-55,35);
        glVertex2i(-90,30);
        glVertex2i(-90,40);


    glEnd();




//HOUSE  two
    glBegin(GL_POLYGON);  // First part

        glColor3ub(25,25,112);//midnight blue
        glVertex2i(-50,140);
        glVertex2i(0,149);
        glVertex2i(-12,88);
        glVertex2i(-65,89);
        glVertex2i(-50,140);
    glEnd();





    glBegin(GL_POLYGON);  // Second Part
    glColor3ub(70,130,180);//midnight blue
        glVertex2i(-60,90);
        glVertex2i(-60,30);
        glVertex2i(-10,25);
        glVertex2i(-10,95);
    glEnd();


//Door
    glColor3ub(25,25,112);//midnight blue
    glRecti(-45,70,-30,27);

//
    glBegin(GL_POLYGON);   // Third part
    glColor3ub(95,158,160);//midnight blue
        glVertex2i(-10,25);
        glVertex2i(18,35);
        glVertex2i(18,100);
        glVertex2i(0,148);
        glVertex2i(-10,100);
        glVertex2i(-10,25);

    glEnd();

    glBegin(GL_POLYGON);
    glColor3ub(25,25,112);//midnight blue
        glVertex2i(-1,150);
        glVertex2i(20,100);
        glVertex2i(17,90);
        glVertex2i(-4,140);
        glVertex2i(-1,150);

    glEnd();


    glBegin(GL_POLYGON);  // door
    glColor3ub(25,25,112);//midnight blue
        glVertex2i(0,70);
        glVertex2i(10,73);
        glVertex2i(10,32);
        glVertex2i(0,29);
        glVertex2i(0,70);


    glEnd();
    glBegin(GL_POLYGON);  // (lower part 1)
        glColor3ub(0,0,0);//
        glVertex2i(-10,25);
        glVertex2i(-10,15);
        glVertex2i(20,27);
        glVertex2i(20,37);
        glVertex2i(-10,25);


    glEnd();
    glBegin(GL_POLYGON);  // (lower part 2)
        glColor3ub(0,0,0);//
        glVertex2i(-10,25);
        glVertex2i(-62,30);
        glVertex2i(-62,20);
        glVertex2i(-10,15);
        glVertex2i(-10,25);




    glEnd();

//man
//drawMan();
//drawHuman(manX, -20);
//drawHuman(womanX, -25);
//drawHuman(childX, -30);
if(!isNight)
{
    drawHuman(manX, -20);
    drawHuman(womanX, -25);
    drawHuman(childX, -30);
}

walkingPath();
drawCar();

// Cow
drawCow(45,45,0.40f);
//RIVER
glBegin(GL_POLYGON);

if(isNight)
{
    // NIGHT RIVER (dark + reflection feel)
    glColor3ub(0,0,50);
}
else
{
    // DAY RIVER (blue)
    glColor3ub(30,144,255);
}

glVertex2i(-200,-50);
glVertex2i(200,-30);

glVertex2i(200,-100);
glVertex2i(-200,-100);
glVertex2i(-200,-50);

glEnd();
    glBegin(GL_POLYGON); // border
        glColor3ub(128,128,0);
        glVertex2i(-200,-45);
        glVertex2i(200,-25);
        glVertex2i(200,-30);
        glVertex2i(-200,-50 );
        glVertex2i(-200,-45);
    glEnd();

    // grass and stones on the field, between the path and the river (kept off the water)
    drawEdgeGrass();
    drawStones();

    // flowing water ripples (direction shown by animated offset)
    drawRiverRipples();

    // animated fish in river
     fishAnimation();
    // sandy near bank below the river, with its own grass tufts and stones
    drawSandGround();
    drawSandGrassTufts();
    drawSandStones();
    drawTent(-200, -145, 1.5f);
    drawBonfire(-10, -185, 0.70f);
    drawBambooPlatform();

//CLOUD
	glPushMatrix();
	if(isNight)
        glColor3ub(120,120,150);   // cooler, dimmer clouds at night
    else
        glColor3ub(220,220,220);   // bright clouds by day
    glTranslatef(tx,0,0);
    cloud(0,250);
    cloud(15,245);
    cloud(10,240);
    cloud(-2,243);



    cloud(-80,250);
    cloud(-95,245);
    cloud(-90,240);
    cloud(-90,243);
    cloud(-75,243);

    glPopMatrix();
    //cloud motion
    tx+=.01;
    if(tx>200)
    tx=-200;

    // fireflies over the field at night
    drawFireflies();

//BOAT
    glPushMatrix();
	glColor3f(0.0f, 0.0f, 0.0f);//Black
    glTranslatef(bx,0,0);
    glScalef(scaleFactor, scaleFactor, 1.0);

    // compress + lift the boat so it still sits nicely inside the
    // now-narrower river band instead of dipping into the sand
    glPushMatrix();
    glTranslatef(0, 5, 0);
    glScalef(0.5f, 0.5f, 1.0f);

    glBegin(GL_POLYGON);
        glVertex2i(-180,-70);
        glVertex2i(-165,-100);
        glVertex2i(-150,-120);
        glVertex2i(-150,-100);
        glVertex2i(-180,-70);
    glEnd();
    glBegin(GL_POLYGON);
        glVertex2i(-150,-100);
        glVertex2i(-150,-120);
        glVertex2i(-120,-125);
        glVertex2i(-90,-120);
        glVertex2i(-85,-100);
        glVertex2i(-150,-100);
    glEnd();
    glBegin(GL_POLYGON);
        glVertex2i(-85,-100);
        glVertex2i(-90,-120);
        glVertex2i(-75,-105);
        glVertex2i(-60,-70);
        glVertex2i(-85,-100);
    glEnd();
    glColor3ub(211,211,211);


    //BOAT FLAG
    glBegin(GL_POLYGON);
        glColor3ub(173,216,230);
        glVertex2i(-57,-40);
        glVertex2i(-50,-10);
        glVertex2i(-49,10);
        glVertex2i(-50,30);
        glVertex2i(-55,45);
        glVertex2i(-63,57);
        glVertex2i(-73,68); // end
        glVertex2i(-105,45);
        glVertex2i(-50,-10);


    glEnd();
    glBegin(GL_POLYGON);
        glColor3ub(173,216,230);

        glVertex2i(-68,-70);
        glVertex2i(-57,-40);
        glVertex2i(-85,10);
        glVertex2i(-68,-70);
    glEnd();
    glBegin(GL_POLYGON);
        glColor3ub(173,216,230);
        glVertex2i(-85,-100);
        glVertex2i(-68,-70);
        glVertex2i(-80,-10);
        glVertex2i(-85,-100);

    glEnd();

    glColor3ub(139,69,19);
    glRecti(-88,80,-86,-100);  // Boat stand
    glBegin(GL_POLYGON);
        glColor3f(0.55,0.27,0.0745);//wood color
        glVertex2i(-85,-100);
        glVertex2i(-87,-80);
        glVertex2i(-93,-62);
        glVertex2i(-97,-55);
        glVertex2i(-105,-50);
        glVertex2i(-120,-48);
        glVertex2i(-120,-100);
        glVertex2i(-85,-100);

    glEnd();

    glBegin(GL_POLYGON);
        glColor3f(0.55,0.27,0.0745);//wood color
        glVertex2i(-150,-100);
        glVertex2i(-148,-80);
        glVertex2i(-142,-62);
        glVertex2i(-138,-55);
        glVertex2i(-130,-50);
        glVertex2i(-115,-48);
        glVertex2i(-115,-100);
        glVertex2i(-150,-100);


    glEnd();

//BOAT LINE
    glBegin(GL_LINE_STRIP);
        glColor3f(0.0f, 0.0f, 0.0f);//Black
        glVertex2i(-142,-62);
        glVertex2i(-73,68);
        glVertex2i(-73,63);

        glVertex2i(-142,-62);
        glVertex2i(-105,45);
    glEnd();
    glBegin(GL_LINE_STRIP);
        glColor3f(0.0f, 0.0f, 0.0f);//Black
        glVertex2i(-148,-80);
        glVertex2i(-87,-80);
    glEnd();
    glBegin(GL_LINE_STRIP);
        glColor3f(0.0f, 0.0f, 0.0f);//Black
        glVertex2i(-142,-62);
        glVertex2i(-93,-62);
    glEnd();
    glBegin(GL_LINE_STRIP);
        glColor3f(0.0f, 0.0f, 0.0f);//Black
        glVertex2i(-115,-48);
        glVertex2i(-115,-100);
    glEnd();
    glBegin(GL_LINE_STRIP);
        glColor3f(0.0f, 0.0f, 0.0f);//Black
        glVertex2i(-130,-50);
        glVertex2i(-130,-100);
    glEnd();
    glBegin(GL_LINE_STRIP);
        glColor3f(0.0f, 0.0f, 0.0f);//Black
        glVertex2i(-100,-52);
        glVertex2i(-100,-100);
    glEnd();

    //gear function call
    gear(-120,-90);

    glPopMatrix();

    //rain conditon

if(isNight && isRaining)
{
    glColor3ub(80, 80, 120); // dark rain mood
}
drawRain();

//solar angle (rotates slowly)

solarAngle += 0.05;

    glPopMatrix();



    glutPostRedisplay();
    glColor3ub(255,255,255);//
    glRecti(-210,310,-200,-210);
    glRecti(200,310,210,-210);

    //Boat movement
bx += 0.03;
if(bx > 270)
    bx = -180;

//Gear rotation
gearAngle += 0.05;
if(gearAngle > 360)
    gearAngle -= 360;

//star blink animation
starBlink++;

// update twinkle brightness for each star
for(int k=0;k<10;k++)
{
    starBright[k] = 0.5f + 0.5f * sin(starBlink*0.015f + k*1.3f);
}

// update fireflies (night only, cheap to always tick)
updateFireflies();

// water flow offset (gives the river a moving-current look)
waterOffset += 0.15f;
if(waterOffset > 2000.0f)
    waterOffset = 0.0f;

//DAY/NIGHT cycle (switches every 10 seconds, real time)
{
    int elapsed = glutGet(GLUT_ELAPSED_TIME) - cycleStart;
    float t = elapsed / (float)CYCLE_MS;
    if(t > 1.0f) t = 1.0f;

    if(!isNight)
        sx = -150 + t * 350.0f;   // sun sweeps across during its 10s phase
    else
        mx = 220 - t * 440.0f;    // moon sweeps across during its 10s phase

    if(elapsed >= CYCLE_MS)
    {
        cycleStart = glutGet(GLUT_ELAPSED_TIME);
        if(!isNight)
        {
            isNight = true;   // sun's phase is over → night starts
            mx = 220;
        }
        else
        {
            isNight = false;  // night's phase is over → day starts
            sx = -150;
        }
    }
}

//man Movement
manX += 0.02;
if(manX > 200) manX = -200;

// woman (slow)
womanX += 0.03;
if(womanX > 200) womanX = -200;

// child (fast)
childX += 0.04;
if(childX > 200) childX = -200;

//bird Movement
birdX += 0.05;
if(birdX > 260) birdX = -260;

// keep the river sound looping (waveaudio driver doesn't support "repeat")
checkLoopSound("riverSound");
//
    glFlush();
}



int main(int argc,char *argv[])
{
    glutInit(&argc,argv);
    glutInitWindowSize(1200,800);
    glutInitWindowPosition(10,10);
    glutInitDisplayMode(GLUT_RGB | GLUT_SINGLE);
    glutCreateWindow(" village scenery ");

    init();
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    for(int k=0;k<10;k++) starBright[k] = 1.0f;
    initFireflies();
    glutDisplayFunc(display);

    glutKeyboardFunc(keyboard); // boat control + Rain

    glutMainLoop();
    return 0;
}
