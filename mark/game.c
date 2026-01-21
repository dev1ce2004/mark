#include "game.h"
#include "util.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

static const char* SCORE_FILE = "mark_scores.txt";

/* ------------------ score helpers ------------------ */

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

        // format: name,kills
        char* comma = strchr(line, ',');
        if (!comma) continue;
        *comma = 0;

        const char* name = line;
        const char* killsStr = comma + 1;

        if (name[0] == 0) continue;
        int k = atoi(killsStr);
        if (k < 0) k = 0;

        // add/update
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

/* ------------------ gameplay helpers ------------------ */

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
    if (g->p.dashing) return false;            // dash invuln
    if (g->p.iFrameTimer > 0.0f) return false; // i-frames
    return true;
}

static float difficulty_scale(int kills)
{
    float s = 1.0f + (float)kills * 0.0125f;
    if (s > 1.9f) s = 1.9f;
    return s;
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
        e->windup = 0.0f;
        break;
    }
}

static void spawn_enemy(Game* g)
{
    for (int i = 0; i < MAX_ENEMIES; i++) {
        if (g->enemies[i].alive) continue;

        Enemy* e = &g->enemies[i];

        int roll = rand() % 100;
        EnemyType t = ENEMY_GRUNT;

        if (g->kills < 5) {
            if (roll < 85) t = ENEMY_GRUNT;
            else if (roll < 97) t = ENEMY_RUNNER;
            else t = ENEMY_ARCHER;
        }
        else if (g->kills < 15) {
            if (roll < 55) t = ENEMY_GRUNT;
            else if (roll < 80) t = ENEMY_RUNNER;
            else if (roll < 95) t = ENEMY_ARCHER;
            else t = ENEMY_TANK;
        }
        else {
            if (roll < 40) t = ENEMY_GRUNT;
            else if (roll < 65) t = ENEMY_RUNNER;
            else if (roll < 85) t = ENEMY_ARCHER;
            else t = ENEMY_TANK;
        }

        init_enemy_by_type(e, t);

        float s = difficulty_scale(g->kills);
        e->speed *= s;

        if (e->type == ENEMY_ARCHER) {
            e->shootCooldown /= (0.85f + 0.15f * s);
            if (e->shootCooldown < 0.55f) e->shootCooldown = 0.55f;
        }

        int edge = rand() % 4;
        if (edge == 0) { e->x = -e->size;  e->y = (float)(rand() % (int)g->worldH); }
        if (edge == 1) { e->x = g->worldW; e->y = (float)(rand() % (int)g->worldH); }
        if (edge == 2) { e->x = (float)(rand() % (int)g->worldW); e->y = -e->size; }
        if (edge == 3) { e->x = (float)(rand() % (int)g->worldW); e->y = g->worldH; }

        float base = 1.0f;
        float faster = (g->kills > 20) ? 0.75f : (g->kills > 10 ? 0.85f : 1.0f);
        g->spawnTimer = base * faster;
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

static void center_panel(SDL_Renderer* r, float w, float h, float worldW, float worldH)
{
    float x = (worldW - w) * 0.5f;
    float y = (worldH - h) * 0.5f;

    SDL_SetRenderDrawColor(r, 0, 0, 0, 120);
    draw_rect(r, x + 6, y + 8, w, h);

    SDL_SetRenderDrawColor(r, 18, 18, 22, 245);
    draw_rect(r, x, y, w, h);

    SDL_SetRenderDrawColor(r, 255, 255, 255, 40);
    draw_frame(r, x, y, w, h);
}

static void hud(Game* g, SDL_Renderer* r)
{
    float W = g->worldW;

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

/* ------------- event handling (text input) ------------- */

void Game_HandleEvent(Game* g, const SDL_Event* e)
{
    // only accept typing in menu
    if (g->state != GAME_MENU) return;

    if (e->type == SDL_EVENT_TEXT_INPUT) {
        if (g->nameLocked) return; // name locked after first start (ready player one stari arcade inspo)
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
            // quick clear
            g->nameLen = 0;
            g->playerName[0] = 0;
        }
    }
}

/* ------------------ init + update + render ------------------ */

void Game_Init(Game* g, float worldW, float worldH)
{
    *g = (Game){ 0 };
    g->worldW = worldW;
    g->worldH = worldH;
    g->state = GAME_MENU;

    SDL_strlcpy(g->playerName, "PLAYER", NAME_MAX);
    g->nameLen = (int)strlen(g->playerName);
    g->nameLocked = false;

    load_scores(g);
    g->lastRunKills = 0;

    g->p = (Player){
        .x = worldW / 2.0f,
        .y = worldH / 2.0f,
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

    //SDL_StartTextInput();
    g->state = GAME_MENU;
}

void Game_Update(Game* g, const bool* keys, float dt)
{
    bool enterNow = keys[SDL_SCANCODE_RETURN] || keys[SDL_SCANCODE_KP_ENTER];
    bool rNow = keys[SDL_SCANCODE_R];

    if (g->state == GAME_MENU) {
        if (pressed(enterNow, &g->prevEnter)) {
            g->nameLocked = true;

            reset_run(g);
            g->state = GAME_PLAY;
        }
        else g->prevEnter = enterNow;

        return;
    }

    if (g->state == GAME_DEAD) {
        if (pressed(enterNow, &g->prevEnter)) {
            reset_run(g);
            g->state = GAME_MENU;
        }
        else g->prevEnter = enterNow;
        return;
    }

    
    if (pressed(enterNow, &g->prevEnter)) {
        g->state = GAME_MENU;
        return;
    }

    Player* p = &g->p;

    if (p->iFrameTimer > 0.0f) p->iFrameTimer -= dt;
    if (p->fireTimer > 0.0f) p->fireTimer -= dt;

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

    p->dx = p->dy = 0.0f;
    if (keys[SDL_SCANCODE_W]) p->dy -= 1.0f;
    if (keys[SDL_SCANCODE_S]) p->dy += 1.0f;
    if (keys[SDL_SCANCODE_A]) p->dx -= 1.0f;
    if (keys[SDL_SCANCODE_D]) p->dx += 1.0f;

    bool spaceNow = keys[SDL_SCANCODE_SPACE];
    if (!p->dashing && p->cooldownTimer <= 0.0f &&
        spaceNow && (p->dx != 0.0f || p->dy != 0.0f)) {
        p->dashing = true;
        p->dashTimer = p->dashTime;
        p->cooldownTimer = p->dashCooldown;
    }

    float len = SDL_sqrtf(p->dx * p->dx + p->dy * p->dy);
    if (len > 0.0f) { p->dx /= len; p->dy /= len; }

    float moveSpeed = p->dashing ? p->dashSpeed : p->speed;
    p->x += p->dx * moveSpeed * dt;
    p->y += p->dy * moveSpeed * dt;

    if (p->dashing) {
        p->dashTimer -= dt;
        if (p->dashTimer <= 0.0f) p->dashing = false;
    }
    else if (p->cooldownTimer > 0.0f) {
        p->cooldownTimer -= dt;
    }

    p->x = clampf(p->x, 0.0f, g->worldW - p->size);
    p->y = clampf(p->y, 0.0f, g->worldH - p->size);

    float mx, my;
    SDL_GetMouseState(&mx, &my);

    float pcx = p->x + p->size * 0.5f;
    float pcy = p->y + p->size * 0.5f;

    float aimx = mx - pcx;
    float aimy = my - pcy;
    float alen = SDL_sqrtf(aimx * aimx + aimy * aimy);
    if (alen > 0.0f) { aimx /= alen; aimy /= alen; }

    Uint32 mb = SDL_GetMouseState(NULL, NULL);
    bool shootNow = (mb & SDL_BUTTON_LMASK) || keys[SDL_SCANCODE_LCTRL];
    if (shootNow && p->fireTimer <= 0.0f && alen > 0.0f) {
        fire_bullet(g, aimx, aimy);
    }

    g->spawnTimer -= dt;
    if (g->spawnTimer <= 0.0f) spawn_enemy(g);

    for (int i = 0; i < MAX_BULLETS; i++) {
        Bullet* b = &g->bullets[i];
        if (!b->alive) continue;

        b->x += b->vx * dt;
        b->y += b->vy * dt;

        if (b->x < -50 || b->x > g->worldW + 50 || b->y < -50 || b->y > g->worldH + 50)
            b->alive = false;
    }

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

                    // submit score on d
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

    g->hit = false;
    g->hitDmg = 14;

    for (int i = 0; i < MAX_ENEMIES; i++) {
        Enemy* en = &g->enemies[i];
        if (!en->alive) continue;

        if (en->type == ENEMY_ARCHER) {
            float ecx = en->x + en->size * 0.5f;
            float ecy = en->y + en->size * 0.5f;

            float toPx = pcx - ecx;
            float toPy = pcy - ecy;
            float dist = SDL_sqrtf(toPx * toPx + toPy * toPy);
            float nx = 0.0f, ny = 0.0f;
            if (dist > 0.0f) { nx = toPx / dist; ny = toPy / dist; }

            en->strafeTimer -= dt;
            if (en->strafeTimer <= 0.0f) {
                en->strafeDir = -en->strafeDir;
                en->strafeTimer = 0.6f + (rand() % 60) / 100.0f;
            }

            float desired = 240.0f;
            float moveX = 0.0f, moveY = 0.0f;

            if (dist < desired - 30.0f) {
                moveX -= nx; moveY -= ny;
            }
            else if (dist > desired + 60.0f) {
                moveX += nx * 0.55f; moveY += ny * 0.55f;
            }

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
                    float s = difficulty_scale(g->kills);
                    en->windup = 0.22f - 0.06f * (s - 1.0f);
                    if (en->windup < 0.14f) en->windup = 0.14f;
                }
            }

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

            continue;
        }

        float ex = en->x - p->x;
        float ey = en->y - p->y;
        float d = SDL_sqrtf(ex * ex + ey * ey);
        if (d > 0.0f) { ex /= d; ey /= d; }

        en->x -= ex * en->speed * dt;
        en->y -= ey * en->speed * dt;

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

    update_dmgnums(g, dt);
}

void Game_Render(Game* g, SDL_Renderer* renderer)
{
    // MENU
    if (g->state == GAME_MENU) {
        float pw = 600, ph = 380;
        center_panel(renderer, pw, ph, g->worldW, g->worldH);

        int cx = (int)(g->worldW * 0.5f);
        int top = (int)(g->worldH * 0.5f - ph * 0.5f);

        SDL_SetRenderDrawColor(renderer, 245, 245, 245, 255);
        dbg_center(renderer, cx, top + 55, "MARK");

        // name line
        SDL_SetRenderDrawColor(renderer, 220, 220, 220, 255);

        char nameLine[128];
        if (!g->nameLocked) {
            SDL_snprintf(nameLine, (int)sizeof(nameLine), "NAME: %s_", g->playerName);
        }
        else {
            SDL_snprintf(nameLine, (int)sizeof(nameLine), "NAME: %s", g->playerName);
        }
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
        else SDL_SetRenderDrawColor(renderer, 200, 60, 60, 255);

        if (e->type == ENEMY_ARCHER && e->windup > 0.0f)
            SDL_SetRenderDrawColor(renderer, 190, 255, 190, 255);

        SDL_FRect er = { e->x, e->y, e->size, e->size };
        SDL_RenderFillRect(renderer, &er);
    }

    SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);
    for (int i = 0; i < MAX_BULLETS; i++) {
        if (!g->bullets[i].alive) continue;
        SDL_FRect br = { g->bullets[i].x - 2, g->bullets[i].y - 2, 4, 4 };
        SDL_RenderFillRect(renderer, &br);
    }

    SDL_SetRenderDrawColor(renderer, 170, 255, 170, 255);
    for (int i = 0; i < MAX_EBULLETS; i++) {
        if (!g->ebullets[i].alive) continue;
        SDL_FRect br = { g->ebullets[i].x - 2, g->ebullets[i].y - 2, 4, 4 };
        SDL_RenderFillRect(renderer, &br);
    }

    if (g->p.dashing) SDL_SetRenderDrawColor(renderer, 255, 80, 80, 255);
    else if (g->p.iFrameTimer > 0.0f) SDL_SetRenderDrawColor(renderer, 180, 220, 255, 255);
    else SDL_SetRenderDrawColor(renderer, 80, 200, 255, 255);

    SDL_FRect pr = { g->p.x, g->p.y, g->p.size, g->p.size };
    SDL_RenderFillRect(renderer, &pr);

    for (int i = 0; i < MAX_DMG_NUMS; i++) {
        if (!g->dmgnums[i].alive) continue;

        if (g->dmgnums[i].value < 0) SDL_SetRenderDrawColor(renderer, 255, 120, 120, 255);
        else                         SDL_SetRenderDrawColor(renderer, 240, 240, 240, 255);

        char buf[32];
        SDL_snprintf(buf, (int)sizeof(buf), "%d", g->dmgnums[i].value);
        int w = (int)strlen(buf) * 8;

        SDL_RenderDebugText(renderer,
            (int)g->dmgnums[i].x - w / 2,
            (int)g->dmgnums[i].y,
            buf
        );
    }

    hud(g, renderer);

    if (g->state == GAME_DEAD) {
        float pw = 560, ph = 240;
        center_panel(renderer, pw, ph, g->worldW, g->worldH);

        int cx = (int)(g->worldW * 0.5f);
        int top = (int)(g->worldH * 0.5f - ph * 0.5f);

        SDL_SetRenderDrawColor(renderer, 245, 245, 245, 255);
        dbg_center(renderer, cx, top + 60, "YOU DIED");

        SDL_SetRenderDrawColor(renderer, 220, 220, 220, 255);
        dbg_centerf(renderer, cx, top + 105, "Kills: %d", g->kills);
        dbg_center(renderer, cx, top + 155, "Press ENTER to return to menu");
    }
}
