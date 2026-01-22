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
    ENEMY_WIZARD = 4,
    ENEMY_BOSS = 5
} EnemyType;

typedef enum {
    BOSS_NONE = 0,
    BOSS_MINI1_BRUTE,
    BOSS_MINI2_WARDEN,
    BOSS_MINI3_HUNTER,
    BOSS_MARKED
} BossKind;

typedef enum {
    BOSSF_PHASE1 = 0,
    BOSSF_PHASE2 = 1,
    BOSSF_PHASE3 = 2
} BossPhase;

typedef struct {
    float x, y;
    float size;
    float speed;
    bool alive;

    EnemyType type;
    int hp;
    int touchDmg;

    // shooters / casters
    float shootCooldown;
    float shootTimer;

    // movement style
    float strafeDir;
    float strafeTimer;

    // telegraph / charge / cast windup
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
    int reserve;

    float fireCooldown;
    float fireTimer;

    float reloadTime;
    float reloadTimer;
    bool reloading;

    // i-frames / damage gating
    float iFrameTime;
    float iFrameTimer;
} Player;

typedef struct {
    char name[NAME_MAX];
    int bestKills;
} ScoreEntry;

typedef struct {
    bool active;
    BossKind kind;

    float x, y;
    float size;

    int hp, hpMax;
    BossPhase phase;

    // generic timers
    float t0;
    float t1;

    // attack controls
    float atkTimer;
    float windup;

    // popup / overlay message
    float msgTimer;
    char  msg[64];

    // lock spawns while boss active
    bool lockSpawns;

    // kill triggers (next thresholds)
    int nextMini1;
    int nextMini2;
    int nextMini3;
    int nextMarked;
} Boss;

typedef struct {
    // world/camera
    float worldW, worldH;

    float screenW, screenH;
    float camX, camY;
    float camTX, camTY;

    GameState state;

    // gameplay entities
    Player p;
    Enemy enemies[MAX_ENEMIES];
    Bullet bullets[MAX_BULLETS];
    EnemyBullet ebullets[MAX_EBULLETS];
    DamageNum dmgnums[MAX_DMG_NUMS];

    // spawns/difficulty
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

    // name + scores
    bool nameLocked;
    char playerName[NAME_MAX];
    int nameLen;

    ScoreEntry scores[SCORE_MAX];
    int scoreCount;

    // boss system
    Boss boss;

    // helper counters for caps
    int aliveArchers;
    int aliveWizards;

    // input edge detection
    bool prevEnter;
    bool prevR;
} Game;

void Game_Init(Game* g, float screenW, float screenH);
void Game_HandleEvent(Game* g, const SDL_Event* e);
void Game_Update(Game* g, const bool* keys, float dt);
void Game_Render(Game* g, SDL_Renderer* renderer);
