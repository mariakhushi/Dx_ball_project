#include <windows.h> 
#include <GL/glut.h>
#include <vector>
#include <string>
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <sstream>
#include <algorithm>
using namespace std;

// ---------------- GLOBAL VARIABLES ---------------- //
const int WINDOW_W = 1000;
const int WINDOW_H = 800;

enum GameState { MENU, PLAYING, PAUSED, GAMEOVER, WIN };
GameState state = MENU;

int startTime = 0;
int elapsedSeconds = 0;

const float PADDLE_BASE_W = 120.0f; // <-- original width
float paddleW = PADDLE_BASE_W;
const float PADDLE_H = 12.0f;
float paddleX = WINDOW_W / 2.0f - PADDLE_BASE_W / 2.0f;
const float PADDLE_Y = 50.0f;
float paddleSpeed = 8.0f;

struct Ball { float x, y, dx, dy, r; } ball;

struct Brick {
    float x, y, w, h;
    int hp;
    float cr, cg, cb;
};
vector<Brick> bricks;
int rows = 5, cols = 9;
float brickW, brickH, brickStartY = 550.0f;

int scoreVal = 0;
int lives = 3;

enum PerkType { PERK_NONE, PERK_EXTRA_LIFE, PERK_FAST_BALL, PERK_WIDE_PADDLE };
struct Perk { float x, y, size; PerkType type; bool active; };
vector<Perk> perks;

int randBetween(int a, int b) { return a + rand() % (b - a + 1); }

int menuIndex = 0;
vector<string> menuItems = {"Start Game", "Resume", "Exit"};

// ---------------- DRAW UTILITIES ---------------- //
void drawBitmapText(float x, float y, const string &s) {
    glRasterPos2f(x, y);
    for (char c : s) glutBitmapCharacter(GLUT_BITMAP_HELVETICA_18, c);
}
void drawRect(float x, float y, float w, float h) {
    glBegin(GL_QUADS);
    glVertex2f(x, y);
    glVertex2f(x + w, y);
    glVertex2f(x + w, y + h);
    glVertex2f(x, y + h);
    glEnd();
}

// ---------------- GAME LOGIC ---------------- //
void applyPerk(PerkType t) {
    if (t == PERK_EXTRA_LIFE) {
        lives++;
    } else if (t == PERK_FAST_BALL) {
        ball.dx *= 1.35f; ball.dy *= 1.35f;
    } else if (t == PERK_WIDE_PADDLE) {
        // widen and cap (no timer)
        paddleW *= 1.4f;
        if (paddleW > WINDOW_W * 0.8f) paddleW = WINDOW_W * 0.8f;
        // keep paddle inside screen
        if (paddleX + paddleW > WINDOW_W) paddleX = WINDOW_W - paddleW;
        if (paddleX < 0) paddleX = 0;
    }
}

void spawnPerk(float x, float y) {
    int chance = randBetween(1, 100);
    if (chance <= 25) {
        Perk p;
        p.x = x; p.y = y; p.size = 18.0f;
        int pick = randBetween(1, 3);
        if (pick == 1) p.type = PERK_EXTRA_LIFE;
        else if (pick == 2) p.type = PERK_FAST_BALL;
        else p.type = PERK_WIDE_PADDLE;
        p.active = true;
        perks.push_back(p);
    }
}

void initGame() {
    srand((unsigned)time(nullptr));
    paddleW = PADDLE_BASE_W;
    paddleX = WINDOW_W / 2.0f - paddleW / 2.0f;

    ball.r = 8.0f;
    ball.x = paddleX + paddleW / 2.0f;
    ball.y = PADDLE_Y + PADDLE_H + ball.r + 2.0f;
    ball.dx = 4.0f;
    ball.dy = -4.0f;

    scoreVal = 0; lives = 3; elapsedSeconds = 0;
    startTime = glutGet(GLUT_ELAPSED_TIME);
    perks.clear();

    bricks.clear();
    brickW = (WINDOW_W - 100.0f) / cols;
    brickH = 30.0f;
    float sx = 50.0f;
    float y = brickStartY;
    for (int r = 0; r < rows; ++r) {
        float x = sx;
        for (int c = 0; c < cols; ++c) {
            Brick b;
            b.x = x; b.y = y;
            b.w = brickW - 6; b.h = brickH - 6;
            b.hp = 1 + (r % 2);
            b.cr = randBetween(50,255)/255.0f;
            b.cg = randBetween(50,255)/255.0f;
            b.cb = randBetween(50,255)/255.0f;
            bricks.push_back(b);
            x += brickW;
        }
        y -= brickH + 4;
    }
}

void resetLevel() { initGame(); state = PLAYING; }

bool rectIntersect(float rx, float ry, float rw, float rh, float cx, float cy, float cr) {
    float nearestX = max(rx, min(cx, rx + rw));
    float nearestY = max(ry, min(cy, ry + rh));
    float dx = cx - nearestX, dy = cy - nearestY;
    return (dx*dx + dy*dy) <= cr*cr;
}

void updatePerks() {
    for (auto &p : perks) {
        if (!p.active) continue;
        p.y -= 3.0f;
        if (p.y <= PADDLE_Y + PADDLE_H && p.y + p.size >= PADDLE_Y &&
            p.x + p.size >= paddleX && p.x <= paddleX + paddleW) {
            applyPerk(p.type);
            p.active = false;
        } else if (p.y + p.size < 0) {
            p.active = false;
        }
    }
}

void updateBall() {
    float t = (glutGet(GLUT_ELAPSED_TIME) - startTime) / 10000.0f;
    float speedBoost = 1.0f + t * 0.15f;
    ball.x += ball.dx * speedBoost;
    ball.y += ball.dy * speedBoost;

    // walls
    if (ball.x - ball.r <= 0) { ball.x = ball.r; ball.dx = -ball.dx; }
    if (ball.x + ball.r >= WINDOW_W) { ball.x = WINDOW_W - ball.r; ball.dx = -ball.dx; }
    if (ball.y + ball.r >= WINDOW_H) { ball.y = WINDOW_H - ball.r; ball.dy = -ball.dy; }

    // paddle
    if (ball.y - ball.r <= PADDLE_Y + PADDLE_H && ball.y - ball.r >= PADDLE_Y &&
        ball.x >= paddleX && ball.x <= paddleX + paddleW) {
        float hitPos = (ball.x - (paddleX + paddleW/2.0f)) / (paddleW/2.0f);
        float angle = hitPos * (3.14159f/3.0f);
        float speed = sqrt(ball.dx*ball.dx + ball.dy*ball.dy);
        ball.dx = speed * sin(angle);
        ball.dy = fabs(speed * cos(angle));
        ball.y = PADDLE_Y + PADDLE_H + ball.r + 1;
    }

    // fell below: lose life -> reset paddle width immediately, then respawn ball
    if (ball.y + ball.r < 0) {
        lives--;
        if (lives <= 0) {
            state = GAMEOVER;
        } else {
            // reset paddle size to original
            paddleW = PADDLE_BASE_W;
            // keep paddle in bounds
            if (paddleX + paddleW > WINDOW_W) paddleX = WINDOW_W - paddleW;
            if (paddleX < 0) paddleX = 0;

            // respawn ball over the (possibly resized) paddle
            ball.x = paddleX + paddleW / 2.0f;
            ball.y = PADDLE_Y + PADDLE_H + ball.r + 2.0f;
            ball.dx = 4.0f * ((rand()%2)==0 ? 1 : -1);
            ball.dy = -4.0f;
        }
    }

    // bricks
    for (auto &b : bricks) {
        if (b.hp <= 0) continue;
        if (rectIntersect(b.x, b.y, b.w, b.h, ball.x, ball.y, ball.r)) {
            ball.dy = -ball.dy;
            b.hp--;
            if (b.hp <= 0) {
                scoreVal += 100;
                spawnPerk(b.x + b.w/2.0f, b.y);
            } else scoreVal += 50;
            break;
        }
    }
}


void checkWinCondition() {
    bool any = false;
    for (auto &b : bricks) if (b.hp > 0) { any = true; break; }
    if (!any) { state = WIN; }
}

// ---------------- DISPLAY ---------------- //
void displayHUD() {
    glColor3f(1,1,1);
    stringstream ss;
    ss << "Score: " << scoreVal; drawBitmapText(10, WINDOW_H - 20, ss.str());
    ss.str(""); ss << "Lives: " << lives; drawBitmapText(150, WINDOW_H - 20, ss.str());
    ss.str(""); ss << "Time: " << elapsedSeconds << "s"; drawBitmapText(260, WINDOW_H - 20, ss.str());
}

void displayMenu() {
    glColor3f(1,1,1);
    drawBitmapText(420, 650, "DX-Ball (GLUT) - Menu");
    for (size_t i=0;i<menuItems.size();++i) {
        if ((int)i == menuIndex) glColor3f(1,0.8f,0.2f);
        else glColor3f(1,1,1);
        drawBitmapText(450, 580 - (int)i*40, menuItems[i]);
    }
    drawBitmapText(150, 120, "Controls: Left/Right arrows or mouse to move paddle. Space = Pause. Esc = Menu");
}

void displayGame() {
    glColor3f(0.2f,0.7f,0.9f);
    drawRect(paddleX, PADDLE_Y, paddleW, PADDLE_H);

    glColor3f(1.0f,0.6f,0.0f);
    glBegin(GL_TRIANGLE_FAN);
    glVertex2f(ball.x, ball.y);
    for (int i=0;i<=20;i++) {
        float a = i/20.0f * 2.0f * 3.1415926f;
        glVertex2f(ball.x + cos(a)*ball.r, ball.y + sin(a)*ball.r);
    }
    glEnd();

    for (auto &b : bricks) {
        if (b.hp <= 0) continue;
        glColor3f(b.cr, b.cg, b.cb);
        drawRect(b.x, b.y, b.w, b.h);
    }

    for (auto &p : perks) {
        if (!p.active) continue;
        if (p.type == PERK_EXTRA_LIFE) glColor3f(0.2f,0.9f,0.2f);
        else if (p.type == PERK_FAST_BALL) glColor3f(0.9f,0.2f,0.2f);
        else if (p.type == PERK_WIDE_PADDLE) glColor3f(0.2f,0.2f,0.9f);
        drawRect(p.x - p.size/2.0f, p.y - p.size/2.0f, p.size, p.size);
    }

    displayHUD();
}

// ---------------- GLUT CALLBACKS ---------------- //
void display() {
    glClear(GL_COLOR_BUFFER_BIT);
    if (state == MENU) displayMenu();
    else if (state == PLAYING || state == PAUSED) {
        displayGame();
        if (state == PAUSED) {
            glColor3f(1,1,0);
            drawBitmapText(WINDOW_W/2 - 40, WINDOW_H/2, "PAUSED");
        }
    } else if (state == GAMEOVER) {
        glColor3f(1,0.2f,0.2f);
        drawBitmapText(WINDOW_W/2 - 60, WINDOW_H/2 + 20, "GAME OVER");
        stringstream ss; ss << "Final Score: " << scoreVal;
        drawBitmapText(WINDOW_W/2 - 70, WINDOW_H/2 - 10, ss.str());
        drawBitmapText(WINDOW_W/2 - 110, WINDOW_H/2 - 50, "Press Enter to return to Menu");
    } else if (state == WIN) {
        glColor3f(0.2f,1,0.2f);
        drawBitmapText(WINDOW_W/2 - 50, WINDOW_H/2 + 20, "YOU WIN!");
        stringstream ss; ss << "Time: " << elapsedSeconds << "s Score: " << scoreVal;
        drawBitmapText(WINDOW_W/2 - 120, WINDOW_H/2 - 10, ss.str());
        drawBitmapText(WINDOW_W/2 - 110, WINDOW_H/2 - 50, "Press Enter to return to Menu");
    }
    glutSwapBuffers();
}

void timerFunc(int v) {
    if (state == PLAYING) {
        updateBall();
        updatePerks();
        checkWinCondition();
        elapsedSeconds = (glutGet(GLUT_ELAPSED_TIME) - startTime) / 1000;
    }
    glutPostRedisplay();
    glutTimerFunc(16, timerFunc, 0);
}

// ---------------- INPUT ---------------- //
void keyboard(unsigned char key, int, int) {
    if (state == MENU) {
        if (key == 13) {
            if (menuItems[menuIndex] == "Start Game") { resetLevel(); startTime = glutGet(GLUT_ELAPSED_TIME); }
            else if (menuItems[menuIndex] == "Resume") state = PLAYING;
            else if (menuItems[menuIndex] == "Exit") exit(0);
        }
        if (key == 27) exit(0);
    } else if (state == PLAYING) {
        if (key == ' ') state = PAUSED;
        else if (key == 27) state = MENU;
    } else if (state == PAUSED) {
        if (key == ' ') state = PLAYING;
        else if (key == 27) state = MENU;
    } else if (state == GAMEOVER || state == WIN) {
        if (key == 13) state = MENU;
        else if (key == 27) exit(0);
    }
}

void specialKey(int key, int, int) {
    if (state == MENU) {
        if (key == GLUT_KEY_UP) menuIndex = max(0, menuIndex - 1);
        else if (key == GLUT_KEY_DOWN) menuIndex = min((int)menuItems.size()-1, menuIndex + 1);
    } 
}

void mouseMotion(int x, int) {
    if (state == PLAYING) {
        paddleX = (float)x - paddleW/2.0f;
        if (paddleX < 0) paddleX = 0;
        if (paddleX + paddleW > WINDOW_W) paddleX = WINDOW_W - paddleW;
    }
}

void reshape(int w, int h) {
    glutReshapeWindow(WINDOW_W, WINDOW_H);
    glViewport(0,0,WINDOW_W,WINDOW_H);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0, WINDOW_W, 0, WINDOW_H, -1, 1);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

// ---------------- MAIN ---------------- //
int main(int argc, char** argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA);
    glutInitWindowSize(WINDOW_W, WINDOW_H);
    glutCreateWindow("DX-Ball (GLUT) - Maria Srijony");

    glClearColor(0.05f,0.05f,0.1f,1.0f);
    initGame();

    glutDisplayFunc(display);
    glutReshapeFunc(reshape);
    glutKeyboardFunc(keyboard);
    glutSpecialFunc(specialKey);
    glutPassiveMotionFunc(mouseMotion);
    glutTimerFunc(16, timerFunc, 0);
    state = MENU;
    glutMainLoop();
    return 0;
}

