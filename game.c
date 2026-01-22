#include "game.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const char* SCORE_FILE = "mark_scores.txt";

static const float SPAWN_MIN = 0.65f;
static const float SPEED_CAP_GRUNT = 190.0f;
static const float SPEED_CAP_RUNNER = 320.0f;
static const float SPEED_CAP_TANK = 125.0f;
static const float SPEED_CAP_ARCHER = 150.0f;
static const float SPEED_CAP_WIZARD = 135.0f;
static const float ARCHER_COOLDOWN_MIN = 0.75f;

static void trim_newline(char* s)
{
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r')) {
        s[n - 1] = 0;
        n--;
    }
}

static int find_score_index(Game* g, const char* name)
{
    for (int i = 0; i < g->scoreCount; i++) {
        if (strcmp(g->scores[i].name, name) == 0) return i;
    }
    return -1;
}

static void sort_scores(Game* g)
{
    for (int i = 0; i < g->scoreCount; i++) {
        for (int j = 0; j < g->scoreCount - 1; j++) {
            if (g->scores[j].bestKills < g->scores[j + 1].bestKills) {
                ScoreEntry tmp = g->scores[j];
                g->scores[j] = g->scores[j + 1];
                g->scores[j + 1] = tmp;
            }
        }
    }
}

static void load_scores(Game* g)
{
    g->scoreCount = 0;

    FILE* f = fopen(SCORE_FILE, "r");
    if (!f) return;

    char line[128];
    while (fgets(line, (int)sizeof(line), f)) {
        trim_newline(line);
        if (line[0] == 0) continue;

        char* comma = strchr(line, ',');
        if (!comma) continue;
        *comma = 0;

        const char* name = line;
        const char* killsStr = comma + 1;

        if (name[0] == 0) continue;
        int k = atoi(killsStr);
        if (k < 0) k = 0;

        int idx = find_score_index(g, name);
        if (idx >= 0) {
            if (k > g->scores[idx].bestKills) g->scores[idx].bestKills = k;
        }
        else {
            if (g->scoreCount < SCORE_MAX) {
                SDL_strlcpy(g->scores[g->scoreCount].name, name, NAME_MAX);
                g->scores[g->scoreCount].bestKills = k;
                g->scoreCount++;
            }
        }
    }

    fclose(f);
    sort_scores(g);
}

static void save_scores(Game* g)
{
    FILE* f = fopen(SCORE_FILE, "w");
    if (!f) return;

    for (int i = 0; i < g->scoreCount; i++) {
        fprintf(f, "%s,%d\n", g->scores[i].name, g->scores[i].bestKills);
    }
    fclose(f);
}

static void submit_score(Game* g, const char* name, int kills)
{
    if (!name || name[0] == 0) return;

    int idx = find_score_index(g, name);
    if (idx >= 0) {
        if (kills > g->scores[idx].bestKills) g->scores[idx].bestKills = kills;
    }
    else if (g->scoreCount < SCORE_MAX) {
        SDL_strlcpy(g->scores[g->scoreCount].name, name, NAME_MAX);
        g->scores[g->scoreCount].bestKills = kills;
        g->scoreCount++;
    }

    sort_scores(g);
    save_scores(g);
}

static int best_for_name(Game* g, const char* name)
{
    int idx = find_score_index(g, name);
    return (idx >= 0) ? g->scores[idx].bestKills : 0;
}

static void spawn_dmg(Game* g, float x, float y, int value)
{
    for (int i = 0; i < MAX_DMG_NUMS; i++) {
        if (!g->dmgnums[i].alive) {
            g->dmgnums[i].alive = true;
            g->dmgnums[i].x = x;
            g->dmgnums[i].y = y;
            g->dmgnums[i].value = value;
            g->dmgnums[i].t = 0.75f;
            return;
        }
    }
}

static void update_dmgnums(Game* g, float dt)
{
    for (int i = 0; i < MAX_DMG_NUMS; i++) {
        if (!g->dmgnums[i].alive) continue;
        g->dmgnums[i].t -= dt;
        g->dmgnums[i].y -= 45.0f * dt;
        if (g->dmgnums[i].t <= 0.0f)
            g->dmgnums[i].alive = false;
    }
}

static bool player_can_take_damage(Game* g)
{
    if (g->p.dashing) return false;
    if (g->p.iFrameTimer > 0.0f) return false;
    return true;
}

static float difficulty_scale(int kills)
{
    float s;
    if (kills <= 80) s = 1.0f + 0.010f * (float)kills;
    else            s = 1.8f + 0.0020f * (float)(kills - 80);
    if (s > 2.0f) s = 2.0f;
    return s;
}

static void set_camera_instant(Game* g)
{
    float cx = g->p.x + g->p.size * 0.5f;
    float cy = g->p.y + g->p.size * 0.5f;

    float tx = cx - g->screenW * 0.5f;
    float ty = cy - g->screenH * 0.5f;

    if (tx < 0.0f) tx = 0.0f;
    if (ty < 0.0f) ty = 0.0f;
    if (tx > g->worldW - g->screenW) tx = g->worldW - g->screenW;
    if (ty > g->worldH - g->screenH) ty = g->worldH - g->screenH;

    if (g->worldW <= g->screenW) tx = 0.0f;
    if (g->worldH <= g->screenH) ty = 0.0f;

    g->camTX = tx;
    g->camTY = ty;
    g->camX = tx;
    g->camY = ty;
}

static void update_camera(Game* g, float dt)
{
    float cx = g->p.x + g->p.size * 0.5f;
    float cy = g->p.y + g->p.size * 0.5f;

    float tx = cx - g->screenW * 0.5f;
    float ty = cy - g->screenH * 0.5f;

    if (tx < 0.0f) tx = 0.0f;
    if (ty < 0.0f) ty = 0.0f;
    if (tx > g->worldW - g->screenW) tx = g->worldW - g->screenW;
    if (ty > g->worldH - g->screenH) ty = g->worldH - g->screenH;

    if (g->worldW <= g->screenW) tx = 0.0f;
    if (g->worldH <= g->screenH) ty = 0.0f;

    g->camTX = tx;
    g->camTY = ty;

    float k = 10.0f;
    float a = 1.0f - SDL_expf(-k * dt);

    g->camX += (g->camTX - g->camX) * a;
    g->camY += (g->camTY - g->camY) * a;
}

/* ---------------- BOSS HELPERS ---------------- */

static void boss_msg(Game* g, const char* txt, float t)
{
    SDL_strlcpy(g->boss.msg, txt, (size_t)sizeof(g->boss.msg));
    g->boss.msgTimer = t;
}

static void boss_clear(Game* g)
{
    g->boss.active = false;
    g->boss.kind = BOSS_NONE;
    g->boss.lockSpawns = false;
    g->boss.msgTimer = 0.0f;
    g->boss.msg[0] = 0;
}

static void boss_init_triggers(Game* g)
{
    g->boss.nextMini1 = 30;
    g->boss.nextMini2 = 65;
    g->boss.nextMini3 = 100;
    g->boss.nextMarked = 150;
}

static void boss_spawn(Game* g, BossKind kind)
{
    g->boss.active = true;
    g->boss.kind = kind;
    g->boss.phase = BOSSF_PHASE1;

    g->boss.size = 72.0f;
    g->boss.hpMax = 350;
    g->boss.hp = g->boss.hpMax;

    g->boss.t0 = 0.0f;
    g->boss.t1 = 0.0f;
    g->boss.atkTimer = 0.0f;
    g->boss.windup = 0.0f;

    g->boss.lockSpawns = true;

    // spawn boss near player but not on top
    float px = g->p.x;
    float py = g->p.y;
    float ox = (rand() % 2) ? 420.0f : -420.0f;
    float oy = (rand() % 2) ? 240.0f : -240.0f;

    g->boss.x = clampf(px + ox, 0.0f, g->worldW - g->boss.size);
    g->boss.y = clampf(py + oy, 0.0f, g->worldH - g->boss.size);

    if (kind == BOSS_MINI1_BRUTE) {
        g->boss.size = 64.0f;
        g->boss.hpMax = 420;
        g->boss.hp = g->boss.hpMax;
        boss_msg(g, "MINIBOSS: BRUTE", 1.1f);
    }
    else if (kind == BOSS_MINI2_WARDEN) {
        g->boss.size = 58.0f;
        g->boss.hpMax = 360;
        g->boss.hp = g->boss.hpMax;
        boss_msg(g, "MINIBOSS: WARDEN", 1.1f);
    }
    else if (kind == BOSS_MINI3_HUNTER) {
        g->boss.size = 52.0f;
        g->boss.hpMax = 340;
        g->boss.hp = g->boss.hpMax;
        boss_msg(g, "MINIBOSS: HUNTER", 1.1f);
    }
    else if (kind == BOSS_MARKED) {
        g->boss.size = 96.0f;
        g->boss.hpMax = 900;
        g->boss.hp = g->boss.hpMax;
        boss_msg(g, "THE MARKED HAS ARRIVED", 1.2f);
    }
}

/* ---------------- ENEMY HELPERS ---------------- */

static void spawn_enemy_bullet(Game* g, float x, float y, float dirx, float diry, int dmg)
{
    for (int i = 0; i < MAX_EBULLETS; i++) {
        if (g->ebullets[i].alive) continue;

        EnemyBullet* b = &g->ebullets[i];
        b->alive = true;
        b->x = x;
        b->y = y;
        b->r = 3.5f;
        b->dmg = dmg;

        float speed = 470.0f;
        b->vx = dirx * speed;
        b->vy = diry * speed;
        return;
    }
}

static void init_enemy_by_type(Enemy* e, EnemyType t)
{
    e->type = t;
    e->alive = true;

    e->shootTimer = 0.0f;
    e->shootCooldown = 0.0f;
    e->strafeDir = 0.0f;
    e->strafeTimer = 0.0f;
    e->windup = 0.0f;

    switch (t) {
    default:
    case ENEMY_GRUNT:
        e->size = 28.0f;
        e->speed = 125.0f;
        e->hp = 50;
        e->touchDmg = 14;
        break;

    case ENEMY_RUNNER:
        e->size = 18.0f;
        e->speed = 245.0f;
        e->hp = 25;
        e->touchDmg = 12;
        break;

    case ENEMY_TANK:
        e->size = 44.0f;
        e->speed = 88.0f;
        e->hp = 150;
        e->touchDmg = 24;
        break;

    case ENEMY_ARCHER:
        e->size = 24.0f;
        e->speed = 105.0f;
        e->hp = 45;
        e->touchDmg = 14;
        e->shootCooldown = 1.35f;
        e->shootTimer = 0.55f;
        e->strafeDir = (rand() % 2) ? 1.0f : -1.0f;
        e->strafeTimer = 0.6f + (rand() % 60) / 100.0f;
        break;

    case ENEMY_WIZARD:
        e->size = 26.0f;
        e->speed = 95.0f;
        e->hp = 70;
        e->touchDmg = 16;
        e->shootCooldown = 2.10f;
        e->shootTimer = 0.70f;
        e->windup = 0.0f;
        break;

    case ENEMY_BOSS:
        // not used for normal enemies
        e->size = 80.0f;
        e->speed = 0.0f;
        e->hp = 500;
        e->touchDmg = 18;
        break;
    }
}

static int count_on_screen_type(Game* g, EnemyType t)
{
    int c = 0;
    float sx0 = g->camX - 120.0f;
    float sx1 = g->camX + g->screenW + 120.0f;
    float sy0 = g->camY - 120.0f;
    float sy1 = g->camY + g->screenH + 120.0f;

    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy* e = &g->enemies[i];
        if (!e->alive) continue;
        if (e->type != t) continue;
        if (e->x + e->size < sx0 || e->x > sx1 || e->y + e->size < sy0 || e->y > sy1) continue;
        c++;
    }
    return c;
}

static int archer_cap(Game* g)
{
    if (g->kills < 80) return 3;
    if (g->kills < 140) return 4;
    return 5;
}

static int wizard_cap(Game* g)
{
    if (g->kills < 80) return 1;
    if (g->kills < 140) return 2;
    return 3;
}


static void spawn_enemy(Game* g)
{
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (g->enemies[i].alive) continue;

        Enemy* e = &g->enemies[i];

        int roll = rand() % 100;
        EnemyType t = ENEMY_GRUNT;

        // mix (same feel, a bit of wizard later)
        if (g->kills < 5) {
            if (roll < 85) t = ENEMY_GRUNT;
            else if (roll < 97) t = ENEMY_RUNNER;
            else t = ENEMY_ARCHER;
        }
        else if (g->kills < 15) {
            if (roll < 55) t = ENEMY_GRUNT;
            else if (roll < 80) t = ENEMY_RUNNER;
            else if (roll < 92) t = ENEMY_ARCHER;
            else t = ENEMY_TANK;
        }
        else if (g->kills < 60) {
            if (roll < 42) t = ENEMY_GRUNT;
            else if (roll < 63) t = ENEMY_RUNNER;
            else if (roll < 82) t = ENEMY_ARCHER;
            else if (roll < 92) t = ENEMY_WIZARD;
            else t = ENEMY_TANK;
        }
        else {
            if (roll < 38) t = ENEMY_GRUNT;
            else if (roll < 56) t = ENEMY_RUNNER;
            else if (roll < 78) t = ENEMY_ARCHER;
            else if (roll < 84) t = ENEMY_WIZARD;  // ↓ manje wizarda
            else t = ENEMY_TANK;
        }

        // ---- SOFT CAPS (prevents archer swarm but keeps difficulty) ----
        if (t == ENEMY_ARCHER) {
            int ons = count_on_screen_type(g, ENEMY_ARCHER);
            if (ons >= archer_cap(g)) t = ENEMY_GRUNT; // keep spawn pressure
        }
        if (t == ENEMY_WIZARD) {
            int ons = count_on_screen_type(g, ENEMY_WIZARD);
            if (ons >= wizard_cap(g)) t = ENEMY_GRUNT;
        }

        init_enemy_by_type(e, t);

        float s = difficulty_scale(g->kills);
        e->speed *= s;

        if (e->type == ENEMY_GRUNT && e->speed > SPEED_CAP_GRUNT)  e->speed = SPEED_CAP_GRUNT;
        if (e->type == ENEMY_RUNNER && e->speed > SPEED_CAP_RUNNER) e->speed = SPEED_CAP_RUNNER;
        if (e->type == ENEMY_TANK && e->speed > SPEED_CAP_TANK)   e->speed = SPEED_CAP_TANK;
        if (e->type == ENEMY_ARCHER && e->speed > SPEED_CAP_ARCHER) e->speed = SPEED_CAP_ARCHER;
        if (e->type == ENEMY_WIZARD && e->speed > SPEED_CAP_WIZARD) e->speed = SPEED_CAP_WIZARD;

        if (e->type == ENEMY_ARCHER) {
            // little scaling, not spammy
            e->shootCooldown /= (0.85f + 0.15f * s);
            if (e->shootCooldown < ARCHER_COOLDOWN_MIN) e->shootCooldown = ARCHER_COOLDOWN_MIN;

            // desync shots
            e->shootTimer += (float)(rand() % 40) / 100.0f;
        }

        float margin = 120.0f;

        float left = g->camX - margin;
        float right = g->camX + g->screenW + margin;
        float top = g->camY - margin;
        float bottom = g->camY + g->screenH + margin;

        int edge = rand() % 4;
        if (edge == 0) { e->x = left;  e->y = top + (float)(rand() % (int)(g->screenH + 2 * margin)); }
        if (edge == 1) { e->x = right; e->y = top + (float)(rand() % (int)(g->screenH + 2 * margin)); }
        if (edge == 2) { e->x = left + (float)(rand() % (int)(g->screenW + 2 * margin)); e->y = top; }
        if (edge == 3) { e->x = left + (float)(rand() % (int)(g->screenW + 2 * margin)); e->y = bottom; }

        e->x = clampf(e->x, 0.0f, g->worldW - e->size);
        e->y = clampf(e->y, 0.0f, g->worldH - e->size);

        float base = 1.0f;
        float faster = (g->kills > 20) ? 0.75f : (g->kills > 10 ? 0.85f : 1.0f);
        g->spawnTimer = base * faster;
        if (g->spawnTimer < SPAWN_MIN) g->spawnTimer = SPAWN_MIN;

        return;
    }
}

static void do_reload(Game* g)
{
    int need = g->p.magMax - g->p.mag;
    if (need <= 0) return;

    if (g->p.reserve < 0) {
        g->p.mag = g->p.magMax;
        return;
    }

    if (g->p.reserve <= 0) return;

    int take = (g->p.reserve < need) ? g->p.reserve : need;
    g->p.mag += take;
    g->p.reserve -= take;
}

static void start_reload(Game* g)
{
    if (g->p.reloading) return;
    if (g->p.mag >= g->p.magMax) return;
    if (g->p.reserve == 0) return;

    g->p.reloading = true;
    g->p.reloadTimer = g->p.reloadTime;
}

static void fire_bullet(Game* g, float dirx, float diry)
{
    if (g->p.reloading) return;
    if (g->p.mag <= 0) return;

    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!g->bullets[i].alive) {
            Bullet* b = &g->bullets[i];
            b->alive = true;

            float px = g->p.x + g->p.size * 0.5f;
            float py = g->p.y + g->p.size * 0.5f;

            b->x = px;
            b->y = py;
            b->r = 4.0f;

            float speed = 900.0f;
            b->vx = dirx * speed;
            b->vy = diry * speed;

            g->p.mag--;
            g->p.fireTimer = g->p.fireCooldown;

            if (g->p.mag == 0) start_reload(g);
            return;
        }
    }
}

static void center_panel(SDL_Renderer* r, float w, float h, float screenW, float screenH)
{
    float x = (screenW - w) * 0.5f;
    float y = (screenH - h) * 0.5f;

    SDL_SetRenderDrawColor(r, 0, 0, 0, 120);
    draw_rect(r, x + 6, y + 8, w, h);

    SDL_SetRenderDrawColor(r, 18, 18, 22, 245);
    draw_rect(r, x, y, w, h);

    SDL_SetRenderDrawColor(r, 255, 255, 255, 40);
    draw_frame(r, x, y, w, h);
}

static void dbg_center(SDL_Renderer* r, int cx, int y, const char* text)
{
    int w = (int)strlen(text) * 8;
    SDL_RenderDebugText(r, cx - (w / 2), y, text);
}

static void dbg_centerf(SDL_Renderer* r, int cx, int y, const char* fmt, int value)
{
    char buf[128];
    SDL_snprintf(buf, (int)sizeof(buf), fmt, value);
    dbg_center(r, cx, y, buf);
}

static void hud(Game* g, SDL_Renderer* r)
{
    float W = g->screenW;

    float panelW = 720.0f;
    float panelH = 64.0f;
    float x = (W - panelW) * 0.5f;
    float y = 16.0f;

    SDL_SetRenderDrawColor(r, 0, 0, 0, 150);
    draw_rect(r, x, y, panelW, panelH);

    SDL_SetRenderDrawColor(r, 255, 255, 255, 45);
    draw_frame(r, x, y, panelW, panelH);

    float barX = x + 18.0f;
    float barY = y + 22.0f;
    float barW = 220.0f;
    float barH = 18.0f;

    SDL_SetRenderDrawColor(r, 255, 255, 255, 35);
    draw_frame(r, barX - 2, barY - 2, barW + 4, barH + 4);

    SDL_SetRenderDrawColor(r, 30, 30, 34, 255);
    draw_rect(r, barX, barY, barW, barH);

    float t = (g->p.hpMax > 0) ? ((float)g->p.hp / (float)g->p.hpMax) : 0.0f;
    t = clampf(t, 0.0f, 1.0f);

    SDL_SetRenderDrawColor(r, 220, 80, 80, 255);
    draw_rect(r, barX, barY, barW * t, barH);

    int leftTx = (int)(x + 18);
    int midTx = (int)(x + panelW * 0.5f - 70);
    int rightTx = (int)(x + panelW - 300);

    SDL_SetRenderDrawColor(r, 235, 235, 235, 255);
    SDL_RenderDebugTextFormat(r, leftTx, (int)(y + 6), "HP %d/%d", g->p.hp, g->p.hpMax);
    SDL_RenderDebugTextFormat(r, midTx, (int)(y + 6), "KILLS %d", g->kills);

    if (g->p.reloading) {
        SDL_RenderDebugText(r, rightTx, (int)(y + 6), "RELOADING...");
    }
    else if (g->p.reserve < 0) {
        SDL_RenderDebugTextFormat(r, rightTx, (int)(y + 6), "AMMO %d/INF", g->p.mag);
    }
    else {
        SDL_RenderDebugTextFormat(r, rightTx, (int)(y + 6), "AMMO %d/%d", g->p.mag, g->p.reserve);
    }

    if (g->p.reloading) {
        float rx = x + panelW - 210.0f;
        float ry = y + 40.0f;
        float rw = 180.0f;
        float rh = 10.0f;

        SDL_SetRenderDrawColor(r, 255, 255, 255, 35);
        draw_frame(r, rx - 1, ry - 1, rw + 2, rh + 2);

        SDL_SetRenderDrawColor(r, 30, 30, 34, 255);
        draw_rect(r, rx, ry, rw, rh);

        float rt = 1.0f - (g->p.reloadTimer / g->p.reloadTime);
        rt = clampf(rt, 0.0f, 1.0f);

        SDL_SetRenderDrawColor(r, 240, 240, 240, 255);
        draw_rect(r, rx, ry, rw * rt, rh);
    }

    SDL_SetRenderDrawColor(r, 210, 210, 210, 255);
    SDL_RenderDebugText(r, (int)(W * 0.5f - 260), (int)(y + panelH + 8),
        "LMB/CTRL shoot  |  R reload  |  SPACE dash (invuln)  |  ENTER menu");
}

static void draw_minimap(Game* g, SDL_Renderer* r)
{
    float pad = 16.0f;
    float mw = 150.0f, mh = 150.0f;

    float fogBottom = 45.0f;
    float x = g->screenW - mw - pad;
    float y = g->screenH - mh - pad - fogBottom;

    SDL_SetRenderDrawColor(r, 0, 0, 0, 150);
    draw_rect(r, x, y, mw, mh);

    SDL_SetRenderDrawColor(r, 255, 255, 255, 45);
    draw_frame(r, x, y, mw, mh);

    float px = x + (g->p.x / g->worldW) * mw;
    float py = y + (g->p.y / g->worldH) * mh;

    SDL_SetRenderDrawColor(r, 80, 200, 255, 255);
    draw_rect(r, px - 2, py - 2, 4, 4);

    SDL_SetRenderDrawColor(r, 200, 60, 60, 255);
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (!g->enemies[i].alive) continue;
        float ex = x + (g->enemies[i].x / g->worldW) * mw;
        float ey = y + (g->enemies[i].y / g->worldH) * mh;
        draw_rect(r, ex - 2, ey - 2, 3, 3);
    }
}

static void draw_fog(Game* g, SDL_Renderer* r)
{
    float fw = 55.0f;
    float fh = 45.0f;

    SDL_SetRenderDrawColor(r, 0, 0, 0, 45);
    draw_rect(r, 0.0f, 0.0f, fw, g->screenH);
    draw_rect(r, g->screenW - fw, 0.0f, fw, g->screenH);
    draw_rect(r, 0.0f, 0.0f, g->screenW, fh);
    draw_rect(r, 0.0f, g->screenH - fh, g->screenW, fh);
}

void Game_HandleEvent(Game* g, const SDL_Event* e)
{
    if (g->state != GAME_MENU) return;

    if (e->type == SDL_EVENT_TEXT_INPUT) {
        if (g->nameLocked) return;
        const char* txt = e->text.text;
        if (!txt) return;

        while (*txt && g->nameLen < NAME_MAX - 1) {
            unsigned char c = (unsigned char)*txt;
            if (c >= 32 && c <= 126) {
                g->playerName[g->nameLen++] = (char)c;
                g->playerName[g->nameLen] = 0;
            }
            txt++;
        }
    }
    else if (e->type == SDL_EVENT_KEY_DOWN) {
        if (g->nameLocked) return;

        if (e->key.scancode == SDL_SCANCODE_BACKSPACE) {
            if (g->nameLen > 0) {
                g->nameLen--;
                g->playerName[g->nameLen] = 0;
            }
        }
        else if (e->key.scancode == SDL_SCANCODE_ESCAPE) {
            g->nameLen = 0;
            g->playerName[0] = 0;
        }
    }
}

static void reset_run(Game* g)
{
    g->p.x = g->worldW * 0.5f;
    g->p.y = g->worldH * 0.5f;
    g->p.dx = g->p.dy = 0;

    g->p.dashing = false;
    g->p.dashTimer = 0;
    g->p.cooldownTimer = 0;

    g->timeSinceHit = 0.0f;
    g->regenDelay = 3.0f;
    g->regenRate = 3.0f;
    g->regenAcc = 0.0f;

    g->p.hp = g->p.hpMax;

    g->p.mag = g->p.magMax;
    g->p.reserve = -1;

    g->p.fireTimer = 0;
    g->p.reloading = false;
    g->p.reloadTimer = 0;

    g->p.iFrameTimer = 0.0f;

    for (int i = 0; i < MAX_ENEMIES; i++) g->enemies[i].alive = false;
    for (int i = 0; i < MAX_BULLETS; i++) g->bullets[i].alive = false;
    for (int i = 0; i < MAX_EBULLETS; i++) g->ebullets[i].alive = false;
    for (int i = 0; i < MAX_DMG_NUMS; i++) g->dmgnums[i].alive = false;

    g->spawnTimer = 0.35f;
    g->hit = false;
    g->hitDmg = 14;
    g->kills = 0;

    boss_clear(g);
    boss_init_triggers(g);

    set_camera_instant(g);
}

void Game_Init(Game* g, float screenW, float screenH)
{
    *g = (Game){ 0 };

    g->screenW = screenW;
    g->screenH = screenH;

    g->worldW = 2400.0f;
    g->worldH = 2400.0f;

    g->camX = 0.0f;
    g->camY = 0.0f;
    g->camTX = 0.0f;
    g->camTY = 0.0f;

    g->state = GAME_MENU;

    SDL_strlcpy(g->playerName, "MARK", NAME_MAX);
    g->nameLen = (int)strlen(g->playerName);
    g->nameLocked = false;

    load_scores(g);
    g->lastRunKills = 0;

    g->p = (Player){
        .x = g->worldW / 2.0f,
        .y = g->worldH / 2.0f,
        .size = 32.0f,
        .speed = 300.0f,
        .dashSpeed = 900.0f,
        .dashTime = 0.15f,
        .dashCooldown = 0.8f,
        .hpMax = 100,
        .hp = 100,
        .magMax = 18,
        .mag = 18,
        .reserve = -1,
        .fireCooldown = 0.10f,
        .fireTimer = 0.0f,
        .reloadTime = 0.55f,
        .reloadTimer = 0.0f,
        .reloading = false,
        .iFrameTime = 0.35f,
        .iFrameTimer = 0.0f
    };

    reset_run(g);
    g->state = GAME_MENU;
}

/* ===================== BOSS ATTACKS (MINI + MARKED) ===================== */

static void boss_take_damage(Game* g, int dmg)
{
    if (!g->boss.active) return;
    g->boss.hp -= dmg;
    if (g->boss.hp < 0) g->boss.hp = 0;

    // phase transitions for MARKED
    if (g->boss.kind == BOSS_MARKED) {
        float hp01 = (g->boss.hpMax > 0) ? ((float)g->boss.hp / (float)g->boss.hpMax) : 0.0f;
        if (hp01 <= 0.35f) g->boss.phase = BOSSF_PHASE3;
        else if (hp01 <= 0.70f) g->boss.phase = BOSSF_PHASE2;
        else g->boss.phase = BOSSF_PHASE1;
    }
}

static void boss_defeated(Game* g)

{   
    for (int i = 0; i < MAX_EBULLETS; i++) g->ebullets[i].alive = false;
    // unlock spawns, keep run going
    g->boss.lockSpawns = false;

    if (g->boss.kind == BOSS_MARKED) {
        boss_msg(g, "THE MARKED IS DOWN", 1.2f);
        // schedule next marked later for endless runs
        g->boss.nextMarked += 120;
    }
    else {
        boss_msg(g, "BOSS DEFEATED", 1.0f);
    }

    // clear boss entity (message stays via timer)
    g->boss.active = false;
    g->boss.kind = BOSS_NONE;
}

static void boss_update_triggers(Game* g)
{
    // do not trigger if already in boss fight
    if (g->boss.active) return;

    if (g->kills >= g->boss.nextMini1) {
        boss_spawn(g, BOSS_MINI1_BRUTE);
        g->boss.nextMini1 += 999999; // only once
    }
    else if (g->kills >= g->boss.nextMini2) {
        boss_spawn(g, BOSS_MINI2_WARDEN);
        g->boss.nextMini2 += 999999;
    }
    else if (g->kills >= g->boss.nextMini3) {
        boss_spawn(g, BOSS_MINI3_HUNTER);
        g->boss.nextMini3 += 999999;
    }
    else if (g->kills >= g->boss.nextMarked) {
        boss_spawn(g, BOSS_MARKED);
        // nextMarked gets bumped on defeat
    }
}

static void boss_move_toward(Game* g, float tx, float ty, float speed, float dt)
{
    float bx = g->boss.x;
    float by = g->boss.y;

    float dx = tx - bx;
    float dy = ty - by;
    float d = SDL_sqrtf(dx * dx + dy * dy);
    if (d > 0.0f) { dx /= d; dy /= d; }

    g->boss.x += dx * speed * dt;
    g->boss.y += dy * speed * dt;

    g->boss.x = clampf(g->boss.x, 0.0f, g->worldW - g->boss.size);
    g->boss.y = clampf(g->boss.y, 0.0f, g->worldH - g->boss.size);
}

static void boss_fire_radial(Game* g, int count, float speed, int dmg)
{
    float cx = g->boss.x + g->boss.size * 0.5f;
    float cy = g->boss.y + g->boss.size * 0.5f;

    for (int i = 0; i < count; i++) {
        float a = (float)i * (6.2831853f / (float)count);
        float dx = SDL_cosf(a);
        float dy = SDL_sinf(a);

        for (int j = 0; j < MAX_EBULLETS; j++) {
            if (g->ebullets[j].alive) continue;
            EnemyBullet* b = &g->ebullets[j];
            b->alive = true;
            b->x = cx;
            b->y = cy;
            b->r = 3.5f;
            b->dmg = dmg;
            b->vx = dx * speed;
            b->vy = dy * speed;
            break;
        }
    }
}
static void fire_radial_at(Game* g, float cx, float cy, int count, float speed, int dmg)
{
    for (int i = 0; i < count; i++) {
        float a = (float)i * (6.2831853f / (float)count);
        float dx = SDL_cosf(a);
        float dy = SDL_sinf(a);

        for (int j = 0; j < MAX_EBULLETS; j++) {
            if (g->ebullets[j].alive) continue;
            EnemyBullet* b = &g->ebullets[j];
            b->alive = true;
            b->x = cx;
            b->y = cy;
            b->r = 3.5f;
            b->dmg = dmg;
            b->vx = dx * speed;
            b->vy = dy * speed;
            break;
        }
    }
}

static void fire_cone_at(Game* g, float cx, float cy, float dirx, float diry, int count, float spreadRad, float speed, int dmg)
{
    // dir must be normalized
    float base = SDL_atan2f(diry, dirx);
    float step = (count <= 1) ? 0.0f : (spreadRad / (float)(count - 1));
    float start = base - spreadRad * 0.5f;

    for (int i = 0; i < count; i++) {
        float a = start + step * (float)i;
        float dx = SDL_cosf(a);
        float dy = SDL_sinf(a);

        for (int j = 0; j < MAX_EBULLETS; j++) {
            if (g->ebullets[j].alive) continue;
            EnemyBullet* b = &g->ebullets[j];
            b->alive = true;
            b->x = cx;
            b->y = cy;
            b->r = 3.5f;
            b->dmg = dmg;
            b->vx = dx * speed;
            b->vy = dy * speed;
            break;
        }
    }
}


/* ===================== ENEMY UPDATES (ARCHER + WIZARD) ===================== */

static void update_archer(Game* g, Enemy* en, float dt)
{
    Player* p = &g->p;

    float ecx = en->x + en->size * 0.5f;
    float ecy = en->y + en->size * 0.5f;

    float pcx = p->x + p->size * 0.5f;
    float pcy = p->y + p->size * 0.5f;

    float toPx = pcx - ecx;
    float toPy = pcy - ecy;
    float dist = SDL_sqrtf(toPx * toPx + toPy * toPy);

    float nx = 0.0f, ny = 0.0f;
    if (dist > 0.0f) { nx = toPx / dist; ny = toPy / dist; }

    // strafe changes
    en->strafeTimer -= dt;
    if (en->strafeTimer <= 0.0f) {
        en->strafeDir = -en->strafeDir;
        en->strafeTimer = 0.6f + (rand() % 60) / 100.0f;
    }

    float desired = 240.0f;
    float moveX = 0.0f, moveY = 0.0f;

    if (dist < desired - 30.0f) { moveX -= nx; moveY -= ny; }
    else if (dist > desired + 60.0f) { moveX += nx * 0.55f; moveY += ny * 0.55f; }

    float pxp = -ny;
    float pyp = nx;
    moveX += pxp * en->strafeDir * 0.75f;
    moveY += pyp * en->strafeDir * 0.75f;

    float ml = SDL_sqrtf(moveX * moveX + moveY * moveY);
    if (ml > 0.0f) { moveX /= ml; moveY /= ml; }

    en->x += moveX * en->speed * dt;
    en->y += moveY * en->speed * dt;

    en->x = clampf(en->x, 0.0f, g->worldW - en->size);
    en->y = clampf(en->y, 0.0f, g->worldH - en->size);

    // shooting
    if (en->windup > 0.0f) {
        en->windup -= dt;
        if (en->windup <= 0.0f) {
            spawn_enemy_bullet(g, ecx, ecy, nx, ny, 10);
            en->shootTimer = en->shootCooldown;
        }
    }
    else {
        en->shootTimer -= dt;
        if (en->shootTimer <= 0.0f) {
            en->windup = 0.18f; // short telegraph
        }
    }
}

// wizard = space control: drops a “sigil” using radial burst + slow orb
static void update_wizard(Game* g, Enemy* en, float dt)
{
    Player* p = &g->p;

    float ecx = en->x + en->size * 0.5f;
    float ecy = en->y + en->size * 0.5f;

    float pcx = p->x + p->size * 0.5f;
    float pcy = p->y + p->size * 0.5f;

    float dx = pcx - ecx;
    float dy = pcy - ecy;
    float dist = SDL_sqrtf(dx * dx + dy * dy);

    float nx = 0.0f, ny = 0.0f;
    if (dist > 0.0f) { nx = dx / dist; ny = dy / dist; }

    // --- BLINK (teleport) every few seconds to keep spacing ---
    en->strafeTimer -= dt; // reuse timer field as blink timer
    if (en->strafeTimer <= 0.0f) {
        // blink to a ring around player (readable but annoying)
        float ang = (float)(rand() % 628) / 100.0f; // 0..6.28
        float r = 320.0f + (float)(rand() % 90);   // 320..410
        float tx = pcx + SDL_cosf(ang) * r;
        float ty = pcy + SDL_sinf(ang) * r;

        en->x = clampf(tx - en->size * 0.5f, 0.0f, g->worldW - en->size);
        en->y = clampf(ty - en->size * 0.5f, 0.0f, g->worldH - en->size);

        en->strafeTimer = 2.6f + (float)(rand() % 80) / 100.0f; // 2.6..3.4
    }

    // --- CAST: windup then cone of slow shots (feels different from archer) ---
    if (en->windup > 0.0f) {
        en->windup -= dt;
        if (en->windup <= 0.0f) {
            // 5-way cone + tiny radial ring (signature “spell” feel)
            ecx = en->x + en->size * 0.5f;
            ecy = en->y + en->size * 0.5f;

            fire_cone_at(g, ecx, ecy, nx, ny, 5, 0.85f, 260.0f, 10);
            fire_radial_at(g, ecx, ecy, 6, 180.0f, 8);

            en->shootTimer = en->shootCooldown + (float)(rand() % 35) / 100.0f;
        }
        return;
    }

    en->shootTimer -= dt;
    if (en->shootTimer <= 0.0f) {
        en->windup = 0.28f; // visible telegraph
    }
}


/* ===================== GAME UPDATE ===================== */

void Game_Update(Game* g, const bool* keys, float dt)
{
    bool enterNow = keys[SDL_SCANCODE_RETURN] || keys[SDL_SCANCODE_KP_ENTER];
    bool rNow = keys[SDL_SCANCODE_R];

    // MENU
    if (g->state == GAME_MENU) {
        if (pressed(enterNow, &g->prevEnter)) {
            g->nameLocked = true;
            reset_run(g);
            g->state = GAME_PLAY;
        }
        else g->prevEnter = enterNow;
        return;
    }

    // DEAD
    if (g->state == GAME_DEAD) {
        if (pressed(enterNow, &g->prevEnter)) {
            reset_run(g);
            g->state = GAME_MENU;
        }
        else g->prevEnter = enterNow;
        return;
    }

    // return to menu from gameplay
    if (pressed(enterNow, &g->prevEnter)) {
        g->state = GAME_MENU;
        return;
    }

    Player* p = &g->p;

    // timers
    if (p->iFrameTimer > 0.0f) p->iFrameTimer -= dt;
    if (p->fireTimer > 0.0f) p->fireTimer -= dt;

    // reload
    if (pressed(rNow, &g->prevR)) start_reload(g);
    else g->prevR = rNow;

    if (p->reloading) {
        p->reloadTimer -= dt;
        if (p->reloadTimer <= 0.0f) {
            p->reloading = false;
            p->reloadTimer = 0.0f;
            do_reload(g);
        }
    }

    // movement input
    p->dx = p->dy = 0.0f;
    if (keys[SDL_SCANCODE_W]) p->dy -= 1.0f;
    if (keys[SDL_SCANCODE_S]) p->dy += 1.0f;
    if (keys[SDL_SCANCODE_A]) p->dx -= 1.0f;
    if (keys[SDL_SCANCODE_D]) p->dx += 1.0f;

    // dash
    bool spaceNow = keys[SDL_SCANCODE_SPACE];
    if (!p->dashing && p->cooldownTimer <= 0.0f &&
        spaceNow && (p->dx != 0.0f || p->dy != 0.0f)) {
        p->dashing = true;
        p->dashTimer = p->dashTime;
        p->cooldownTimer = p->dashCooldown;
    }

    // normalize
    float len = SDL_sqrtf(p->dx * p->dx + p->dy * p->dy);
    if (len > 0.0f) { p->dx /= len; p->dy /= len; }

    // move
    float moveSpeed = p->dashing ? p->dashSpeed : p->speed;
    p->x += p->dx * moveSpeed * dt;
    p->y += p->dy * moveSpeed * dt;

    // timers
    if (p->dashing) {
        p->dashTimer -= dt;
        if (p->dashTimer <= 0.0f) p->dashing = false;
    }
    else if (p->cooldownTimer > 0.0f) {
        p->cooldownTimer -= dt;
    }

    // clamp
    p->x = clampf(p->x, 0.0f, g->worldW - p->size);
    p->y = clampf(p->y, 0.0f, g->worldH - p->size);

    // camera
    update_camera(g, dt);

    // mouse aim (world coords)
    float mx, my;
    SDL_GetMouseState(&mx, &my);
    float mxw = mx + g->camX;
    float myw = my + g->camY;

    float pcx = p->x + p->size * 0.5f;
    float pcy = p->y + p->size * 0.5f;

    float aimx = mxw - pcx;
    float aimy = myw - pcy;
    float alen = SDL_sqrtf(aimx * aimx + aimy * aimy);
    if (alen > 0.0f) { aimx /= alen; aimy /= alen; }

    // shoot (hold)
    Uint32 mb = SDL_GetMouseState(NULL, NULL);
    bool shootNow = (mb & SDL_BUTTON_LMASK) || keys[SDL_SCANCODE_LCTRL];
    if (shootNow && p->fireTimer <= 0.0f && alen > 0.0f) {
        fire_bullet(g, aimx, aimy);
    }

    // boss trigger checks
    boss_update_triggers(g);

    // spawn enemies (skip during boss fights)
    if (!g->boss.lockSpawns) {
        g->spawnTimer -= dt;
        if (g->spawnTimer <= 0.0f) spawn_enemy(g);
    }

    // bullets update
    for (int i = 0; i < MAX_BULLETS; i++) {
        Bullet* b = &g->bullets[i];
        if (!b->alive) continue;
        b->x += b->vx * dt;
        b->y += b->vy * dt;
        if (b->x < -50 || b->x > g->worldW + 50 || b->y < -50 || b->y > g->worldH + 50)
            b->alive = false;
    }

    // enemy bullets update + hit player
    for (int i = 0; i < MAX_EBULLETS; i++) {
        EnemyBullet* b = &g->ebullets[i];
        if (!b->alive) continue;

        b->x += b->vx * dt;
        b->y += b->vy * dt;

        if (b->x > p->x && b->x < p->x + p->size &&
            b->y > p->y && b->y < p->y + p->size)
        {
            if (player_can_take_damage(g)) {
                int dmg = b->dmg;
                p->hp -= dmg;

                p->iFrameTimer = p->iFrameTime;
                g->timeSinceHit = 0.0f;
                g->regenAcc = 0.0f;

                spawn_dmg(g, p->x + p->size * 0.5f, p->y - 6.0f, -dmg);

                if (p->hp <= 0) {
                    p->hp = 0;
                    g->state = GAME_DEAD;

                    g->lastRunKills = g->kills;
                    submit_score(g, g->playerName, g->kills);
                }
            }

            b->alive = false;
            continue;
        }

        if (b->x < -60 || b->x > g->worldW + 60 || b->y < -60 || b->y > g->worldH + 60)
            b->alive = false;
    }

    /* ---------------- ENEMIES UPDATE + COLLISIONS ---------------- */

    g->hit = false;
    g->hitDmg = 14;

    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy* en = &g->enemies[i];
        if (!en->alive) continue;

        if (en->type == ENEMY_ARCHER) {
            update_archer(g, en, dt);
        }
        else if (en->type == ENEMY_WIZARD) {
            update_wizard(g, en, dt);
        }
        else {
            // default chase behavior
            float ex = en->x - p->x;
            float ey = en->y - p->y;
            float d = SDL_sqrtf(ex * ex + ey * ey);
            if (d > 0.0f) { ex /= d; ey /= d; }

            en->x -= ex * en->speed * dt;
            en->y -= ey * en->speed * dt;

            en->x = clampf(en->x, 0.0f, g->worldW - en->size);
            en->y = clampf(en->y, 0.0f, g->worldH - en->size);
        }

        // overlap -> take damage tick and kill enemy (like before)
        bool overlap =
            en->x < p->x + p->size &&
            en->x + en->size > p->x &&
            en->y < p->y + p->size &&
            en->y + en->size > p->y;

        if (overlap) {
            g->hit = true;
            g->hitDmg = en->touchDmg;
            en->alive = false;
        }

        // bullet hit enemy
        for (int bi = 0; bi < MAX_BULLETS; bi++) {
            Bullet* b = &g->bullets[bi];
            if (!b->alive) continue;

            if (b->x > en->x && b->x < en->x + en->size &&
                b->y > en->y && b->y < en->y + en->size) {

                int dmg = 25;
                b->alive = false;

                en->hp -= dmg;
                spawn_dmg(g, en->x + en->size * 0.5f, en->y, dmg);

                if (en->hp <= 0) {
                    en->alive = false;
                    g->kills++;
                }
                break;
            }
        }
    }

    /* ---------------- BOSS UPDATE ---------------- */

    if (g->boss.active) {
        // simple hitbox for bullets
        float bx0 = g->boss.x;
        float by0 = g->boss.y;
        float bx1 = g->boss.x + g->boss.size;
        float by1 = g->boss.y + g->boss.size;

        for (int bi = 0; bi < MAX_BULLETS; bi++) {
            Bullet* b = &g->bullets[bi];
            if (!b->alive) continue;

            if (b->x > bx0 && b->x < bx1 && b->y > by0 && b->y < by1) {
                b->alive = false;
                boss_take_damage(g, 18);
                spawn_dmg(g, g->boss.x + g->boss.size * 0.5f, g->boss.y, 18);
            }
        }

        // behaviors by boss kind
        float pcx2 = p->x + p->size * 0.5f;
        float pcy2 = p->y + p->size * 0.5f;

        float bcx = g->boss.x + g->boss.size * 0.5f;
        float bcy = g->boss.y + g->boss.size * 0.5f;

        float dx = pcx2 - bcx;
        float dy = pcy2 - bcy;
        float dist = SDL_sqrtf(dx * dx + dy * dy);
        float nx = 0.0f, ny = 0.0f;
        if (dist > 0.0f) { nx = dx / dist; ny = dy / dist; }

        if (g->boss.kind == BOSS_MINI1_BRUTE) {
            // BRUTE: short charge bursts
            g->boss.t0 -= dt;
            if (g->boss.t0 <= 0.0f) {
                g->boss.t0 = 1.2f;      // time between charges
                g->boss.t1 = 0.28f;     // charge duration
            }

            if (g->boss.t1 > 0.0f) {
                g->boss.t1 -= dt;
                // charge toward player fast
                boss_move_toward(g, p->x, p->y, 420.0f, dt);
            }
            else {
                // slow drift
                boss_move_toward(g, p->x, p->y, 130.0f, dt);
            }

        }
        else if (g->boss.kind == BOSS_MINI2_WARDEN) {
            // WARDEN: teleport + pulse
            g->boss.t0 -= dt;
            if (g->boss.t0 <= 0.0f) {
                g->boss.t0 = 2.0f;

                // teleport around player
                float ang = (float)(rand() % 628) / 100.0f;
                float r = 300.0f + (float)(rand() % 90);
                float tx = (p->x + p->size * 0.5f) + SDL_cosf(ang) * r;
                float ty = (p->y + p->size * 0.5f) + SDL_sinf(ang) * r;

                g->boss.x = clampf(tx - g->boss.size * 0.5f, 0.0f, g->worldW - g->boss.size);
                g->boss.y = clampf(ty - g->boss.size * 0.5f, 0.0f, g->worldH - g->boss.size);

                // pulse after teleport
                boss_fire_radial(g, 14, 230.0f, 10);
            }

        }
        else if (g->boss.kind == BOSS_MINI3_HUNTER) {
            // HUNTER: strafe + 3-shot burst
            g->boss.t0 -= dt;
            if (g->boss.t0 <= 0.0f) {
                g->boss.t0 = 0.75f;
                g->boss.t1 = (rand() % 2) ? 1.0f : -1.0f;
            }

            float pxp = -ny;
            float pyp = nx;
            float tx = p->x + pxp * g->boss.t1 * 260.0f;
            float ty = p->y + pyp * g->boss.t1 * 260.0f;
            boss_move_toward(g, tx, ty, 200.0f, dt);

            g->boss.atkTimer -= dt;
            if (g->boss.atkTimer <= 0.0f) {
                // quick burst
                spawn_enemy_bullet(g, bcx, bcy, nx, ny, 12);
                spawn_enemy_bullet(g, bcx, bcy, nx, ny, 12);
                spawn_enemy_bullet(g, bcx, bcy, nx, ny, 12);
                g->boss.atkTimer = 1.35f;
            }

        }
        else if (g->boss.kind == BOSS_MARKED) {
            // THE MARKED: phases
            float speed = (g->boss.phase == BOSSF_PHASE1) ? 105.0f :
                (g->boss.phase == BOSSF_PHASE2) ? 120.0f : 135.0f;

            // keep medium distance
            float desired = 260.0f;
            float tx = p->x + (-nx) * desired;
            float ty = p->y + (-ny) * desired;
            boss_move_toward(g, tx, ty, speed, dt);

            g->boss.atkTimer -= dt;

            if (g->boss.atkTimer <= 0.0f) {
                if (g->boss.phase == BOSSF_PHASE1) {
                    // marks = radial slow ring
                    boss_fire_radial(g, 14, 235.0f, 10);
                    g->boss.atkTimer = 1.65f;
                }
                else if (g->boss.phase == BOSSF_PHASE2) {
                    // add more pressure
                    boss_fire_radial(g, 16, 255.0f, 11);
                    spawn_enemy_bullet(g, bcx, bcy, nx, ny, 14);
                    g->boss.atkTimer = 1.45f;
                }
                else {
                    // phase 3: echo / heavier
                    boss_fire_radial(g, 18, 270.0f, 12);
                    spawn_enemy_bullet(g, bcx, bcy, nx, ny, 16);
                    g->boss.atkTimer = 1.15f;
                }
            }
        }

        // player collision with boss
        bool bossOverlap =
            g->boss.x < p->x + p->size &&
            g->boss.x + g->boss.size > p->x &&
            g->boss.y < p->y + p->size &&
            g->boss.y + g->boss.size > p->y;

        if (bossOverlap && player_can_take_damage(g)) {
            int dmg = (g->boss.kind == BOSS_MARKED) ? 18 : 16;
            p->hp -= dmg;

            p->iFrameTimer = p->iFrameTime;
            g->timeSinceHit = 0.0f;
            g->regenAcc = 0.0f;
            spawn_dmg(g, p->x + p->size * 0.5f, p->y - 6.0f, -dmg);

            if (p->hp <= 0) {
                p->hp = 0;
                g->state = GAME_DEAD;

                g->lastRunKills = g->kills;
                submit_score(g, g->playerName, g->kills);
            }
        }

        if (g->boss.hp <= 0) {
            boss_defeated(g);
        }
    }

    /* ---------------- DAMAGE TICK FROM TOUCH ---------------- */

    static float damageTick = 0.0f;
    damageTick -= dt;

    if (g->hit && damageTick <= 0.0f) {
        damageTick = 0.25f;

        if (player_can_take_damage(g)) {
            int dmg = g->hitDmg;
            p->hp -= dmg;

            p->iFrameTimer = p->iFrameTime;
            g->timeSinceHit = 0.0f;
            g->regenAcc = 0.0f;

            spawn_dmg(g, p->x + p->size * 0.5f, p->y - 6.0f, -dmg);

            if (p->hp <= 0) {
                p->hp = 0;
                g->state = GAME_DEAD;

                g->lastRunKills = g->kills;
                submit_score(g, g->playerName, g->kills);
            }
        }
    }

    /* ---------------- REGEN ---------------- */

    g->timeSinceHit += dt;
    if (g->timeSinceHit >= g->regenDelay && p->hp > 0 && p->hp < p->hpMax) {
        g->regenAcc += g->regenRate * dt;
        int add = (int)g->regenAcc;
        if (add > 0) {
            p->hp += add;
            if (p->hp > p->hpMax) p->hp = p->hpMax;
            g->regenAcc -= (float)add;
        }
    }

    // boss message timer
    if (g->boss.msgTimer > 0.0f) g->boss.msgTimer -= dt;

    update_dmgnums(g, dt);
}

/* ===================== GAME RENDER ===================== */

void Game_Render(Game* g, SDL_Renderer* renderer)
{
    if (g->state == GAME_MENU) {
        float pw = 600, ph = 380;
        center_panel(renderer, pw, ph, g->screenW, g->screenH);

        int cx = (int)(g->screenW * 0.5f);
        int top = (int)(g->screenH * 0.5f - ph * 0.5f);

        SDL_SetRenderDrawColor(renderer, 245, 245, 245, 255);
        dbg_center(renderer, cx, top + 55, "MARK");

        SDL_SetRenderDrawColor(renderer, 220, 220, 220, 255);

        char nameLine[128];
        if (!g->nameLocked) SDL_snprintf(nameLine, (int)sizeof(nameLine), "NAME: %s_", g->playerName);
        else                SDL_snprintf(nameLine, (int)sizeof(nameLine), "NAME: %s", g->playerName);
        dbg_center(renderer, cx, top + 105, nameLine);

        int yourBest = best_for_name(g, g->playerName);

        char stat1[128], stat2[128];
        SDL_snprintf(stat1, (int)sizeof(stat1), "LAST RUN: %d", g->lastRunKills);
        SDL_snprintf(stat2, (int)sizeof(stat2), "YOUR BEST: %d", yourBest);

        dbg_center(renderer, cx, top + 135, stat1);
        dbg_center(renderer, cx, top + 160, stat2);

        dbg_center(renderer, cx, top + 195, "LEADERBOARD (TOP 5)");
        for (int i = 0; i < LEADER_TOP; i++) {
            if (i >= g->scoreCount) break;
            char row[128];
            SDL_snprintf(row, (int)sizeof(row), "%d) %s  -  %d",
                i + 1, g->scores[i].name, g->scores[i].bestKills);
            dbg_center(renderer, cx, top + 220 + i * 18, row);
        }

        dbg_center(renderer, cx, top + 320, "ENTER: Start");
        dbg_center(renderer, cx, top + 345, "Backspace: delete   Esc: clear");
        return;
    }

    // enemies
    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy* e = &g->enemies[i];
        if (!e->alive) continue;

        if (e->type == ENEMY_RUNNER) SDL_SetRenderDrawColor(renderer, 240, 110, 110, 255);
        else if (e->type == ENEMY_TANK) SDL_SetRenderDrawColor(renderer, 160, 50, 50, 255);
        else if (e->type == ENEMY_ARCHER) SDL_SetRenderDrawColor(renderer, 140, 220, 140, 255);
        else if (e->type == ENEMY_WIZARD) SDL_SetRenderDrawColor(renderer, 190, 160, 255, 255);
        else SDL_SetRenderDrawColor(renderer, 200, 60, 60, 255);

        SDL_FRect er = { e->x - g->camX, e->y - g->camY, e->size, e->size };
        SDL_RenderFillRect(renderer, &er);
    }

    // bullets
    SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!g->bullets[i].alive) continue;
        SDL_FRect br = { (g->bullets[i].x - g->camX) - 2, (g->bullets[i].y - g->camY) - 2, 4, 4 };
        SDL_RenderFillRect(renderer, &br);
    }

    SDL_SetRenderDrawColor(renderer, 170, 255, 170, 255);
    for (int i = 0; i < MAX_EBULLETS; i++) {
        if (!g->ebullets[i].alive) continue;
        SDL_FRect br = { (g->ebullets[i].x - g->camX) - 2, (g->ebullets[i].y - g->camY) - 2, 4, 4 };
        SDL_RenderFillRect(renderer, &br);
    }

    // player
    if (g->p.dashing) SDL_SetRenderDrawColor(renderer, 255, 80, 80, 255);
    else if (g->p.iFrameTimer > 0.0f) SDL_SetRenderDrawColor(renderer, 180, 220, 255, 255);
    else SDL_SetRenderDrawColor(renderer, 80, 200, 255, 255);

    SDL_FRect pr = { g->p.x - g->camX, g->p.y - g->camY, g->p.size, g->p.size };
    SDL_RenderFillRect(renderer, &pr);

    // boss
    if (g->boss.active) {
        if (g->boss.kind == BOSS_MARKED) SDL_SetRenderDrawColor(renderer, 235, 235, 245, 255);
        else SDL_SetRenderDrawColor(renderer, 210, 210, 210, 255);

        SDL_FRect br = { g->boss.x - g->camX, g->boss.y - g->camY, g->boss.size, g->boss.size };
        SDL_RenderFillRect(renderer, &br);

        // boss hp bar top-center
        float bw = 420.0f;
        float bh = 10.0f;
        float bx = (g->screenW - bw) * 0.5f;
        float by = 96.0f;

        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 140);
        draw_rect(renderer, bx, by, bw, bh);
        SDL_SetRenderDrawColor(renderer, 255, 255, 255, 45);
        draw_frame(renderer, bx, by, bw, bh);

        float t = (g->boss.hpMax > 0) ? ((float)g->boss.hp / (float)g->boss.hpMax) : 0.0f;
        t = clampf(t, 0.0f, 1.0f);

        SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
        draw_rect(renderer, bx, by, bw * t, bh);
    }

    // dmg nums
    for (int i = 0; i < MAX_DMG_NUMS; i++) {
        if (!g->dmgnums[i].alive) continue;

        if (g->dmgnums[i].value < 0) SDL_SetRenderDrawColor(renderer, 255, 120, 120, 255);
        else                         SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);

        char buf[32];
        SDL_snprintf(buf, (int)sizeof(buf), "%d", g->dmgnums[i].value);
        int w = (int)strlen(buf) * 8;

        SDL_RenderDebugText(renderer,
            (int)(g->dmgnums[i].x - g->camX) - w / 2,
            (int)(g->dmgnums[i].y - g->camY),
            buf
        );
    }

    draw_fog(g, renderer);
    draw_minimap(g, renderer);
    hud(g, renderer);

    // boss message overlay
    if (g->boss.msgTimer > 0.0f && g->boss.msg[0]) {
        float pw = 460, ph = 86;
        center_panel(renderer, pw, ph, g->screenW, g->screenH);

        int cx = (int)(g->screenW * 0.5f);
        int top = (int)(g->screenH * 0.5f - ph * 0.5f);

        SDL_SetRenderDrawColor(renderer, 210, 210, 210, 255);
        dbg_center(renderer, cx, top + 55, g->boss.msg);
    }

    // death overlay
    if (g->state == GAME_DEAD) {
        float pw = 560, ph = 240;
        center_panel(renderer, pw, ph, g->screenW, g->screenH);

        int cx = (int)(g->screenW * 0.5f);
        int top = (int)(g->screenH * 0.5f - ph * 0.5f);

        SDL_SetRenderDrawColor(renderer, 245, 245, 245, 255);
        dbg_center(renderer, cx, top + 60, "YOU DIED");

        SDL_SetRenderDrawColor(renderer, 220, 220, 220, 255);
        dbg_centerf(renderer, cx, top + 105, "Kills: %d", g->kills);
        dbg_center(renderer, cx, top + 155, "Press ENTER to return to menu");
    }
}
