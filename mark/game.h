#pragma once
#include <SDL3/SDL.h>
#include <stdbool.h>

#define MAX_ENEMIES   32
#define MAX_BULLETS   128
#define MAX_DMG_NUMS  64
#define MAX_EBULLETS  128

#define NAME_MAX      16
#define SCORE_MAX     32
#define LEADER_TOP    5

typedef enum {
    GAME_MENU = 0,
    GAME_PLAY = 1,
    GAME_DEAD = 2,
} GameState;

typedef enum {
    ENEMY_GRUNT = 0,
    ENEMY_RUNNER = 1,
    ENEMY_TANK = 2,
    ENEMY_ARCHER = 3,
} EnemyType;

typedef struct {
    float x, y;
    float size;
    float speed;
    bool alive;

    EnemyType type;
    int hp;
    int touchDmg;

    // archer logic
    float shootCooldown;
    float shootTimer;
    float strafeDir;
    float strafeTimer;
    float windup;
} Enemy;

typedef struct {
    float x, y;
    float vx, vy;
    float r;
    bool alive;
} Bullet;

typedef struct {
    float x, y;
    float vx, vy;
    float r;
    int dmg;
    bool alive;
} EnemyBullet;

typedef struct {
    float x, y;
    int value;
    float t;
    bool alive;
} DamageNum;

typedef struct {
    float x, y;
    float size;
    float speed;

    float dashSpeed;
    float dashTime;
    float dashCooldown;
    float dashTimer;
    float cooldownTimer;

    float dx, dy;
    bool dashing;

    int hp, hpMax;

    int mag, magMax;
    int reserve; // -1 inf 

    float fireCooldown;
    float fireTimer;

    float reloadTime;
    float reloadTimer;
    bool reloading;

    // i-frames
    float iFrameTime;
    float iFrameTimer;
} Player;

typedef struct {
    char name[NAME_MAX];
    int bestKills;
} ScoreEntry;

typedef struct {
    float worldW, worldH;

    GameState state;

    Player p;
    Enemy enemies[MAX_ENEMIES];
    Bullet bullets[MAX_BULLETS];
    EnemyBullet ebullets[MAX_EBULLETS];
    DamageNum dmgnums[MAX_DMG_NUMS];

    float spawnTimer;
    bool hit;
    int hitDmg;

    int kills;
    int lastRunKills;

    // regen
    float timeSinceHit;
    float regenDelay;
    float regenRate;
    float regenAcc;

    // arcade name input + scoreovi
    bool nameLocked;                
    char playerName[NAME_MAX];      
    int nameLen;

    ScoreEntry scores[SCORE_MAX];
    int scoreCount;

    bool prevEnter;
    bool prevR;
} Game;

void Game_Init(Game* g, float worldW, float worldH);
void Game_HandleEvent(Game* g, const SDL_Event* e);
void Game_Update(Game* g, const bool* keys, float dt);
void Game_Render(Game* g, SDL_Renderer* renderer);
