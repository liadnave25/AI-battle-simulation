#include "glut.h"
#include <vector>
#include <string>
#include <cstdio>
#include <cmath>
#include <algorithm>
#include <ctime>
#include <map> 

#include "Definitions.h"
#include "Grid.h"
#include "Units.h"
#include "Renderer.h"
#include "SecurityMap.h"
#include "Visibility.h"
#include "Combat.h"
#include "Pathfinding.h"
#include "Globals.h"
#include "Warrior.h" 

// AI Includes
#include "Orders.h"
#include "AIEvents.h"
#include "EventBus.h"
#include "Commander.h"
#include "StateMachine.h"

using namespace Definitions;

// World state
Models::Grid              g_grid;
std::vector<Models::Unit*> g_units;
Simulation::SecurityMap   g_smap;
static AI::Visibility::BArray g_vis;

// View modes
static bool g_showSecurity = false; 
static bool g_showVisibility = false; 
static Team g_visTeam = Team::Blue; 

// Add a small helper for printing team name
static inline const char* teamTag(Team t) {
    return (t == Team::Blue) ? "Blue" : "Orange";
}

// HUD (to console)
static float g_fps = 0.0f;
static long  g_cntRock = 0, g_cntTree = 0, g_cntWater = 0, g_cntDepot = 0;
static int   g_blueCount = 0, g_orangeCount = 0;

Combat::System g_combat;
static int g_targetRow = -1, g_targetCol = -1;

// Commander + helpers 
static AI::Commander g_commanderBlue(Definitions::Team::Blue);
static AI::Commander g_commanderOrange(Definitions::Team::Orange);
static int  g_frameCounter = 0;
static bool g_commanderEnabled = false; 

// Random spawn flags/ids (debug)
static bool g_randomizeWarriorsSpawn = true;
static int  g_randBlueWarriorId = -1;
static int  g_randOrangeWarriorId = -1;

// Anchors
static bool g_blueAnchorValid = false;
static bool g_orangeAnchorValid = false;
static int  g_orangeAnchorR = -1, g_orangeAnchorC = -1;
static bool g_gameOver = false;
static Definitions::Team g_winningTeam = Definitions::Team::Blue;




// Helpers

static void drawTeamHealthBars()
{
    const int W = glutGet(GLUT_WINDOW_WIDTH);
    const int H = glutGet(GLUT_WINDOW_HEIGHT);
    if (W <= 0 || H <= 0) return;

    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_VIEWPORT_BIT | GL_TRANSFORM_BIT);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, (double)W, 0.0, (double)H, -1.0, 1.0); 

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    auto drawQuad = [](float x, float y, float w, float h, float r, float g, float b, float a = 1.0f) {
        glColor4f(r, g, b, a);
        glBegin(GL_QUADS);
        glVertex2f(x, y);
        glVertex2f(x + w, y);
        glVertex2f(x + w, y + h); 
        glVertex2f(x, y + h);
        glEnd();
        };

    const float BAR_WIDTH = 120.0f;
    const float BAR_HEIGHT = 18.0f;
    const float BAR_MARGIN_Y = 5.0f;
    const float ROLE_BOX_SIZE = BAR_HEIGHT;
    const float ROLE_BOX_MARGIN = 4.0f;
    const float PADDING_X = 10.0f;
    const float PADDING_Y = 10.0f;

    int blueIdx = 0;
    for (const auto* u : g_units) { 
        if (u->team != Team::Blue) continue;

        float x_role = PADDING_X;
        float x_bar = x_role + ROLE_BOX_SIZE + ROLE_BOX_MARGIN;
        float y = (H - PADDING_Y) - (blueIdx * (BAR_HEIGHT + BAR_MARGIN_Y)) - BAR_HEIGHT; 

        float healthPercent = u->hpNorm();

        drawQuad(x_bar, y, BAR_WIDTH, BAR_HEIGHT, 0.2f, 0.2f, 0.2f, 0.8f);
        drawQuad(x_bar, y, BAR_WIDTH * healthPercent, BAR_HEIGHT, 0.1f, 0.8f, 0.1f, 1.0f);



        drawQuad(x_role, y, ROLE_BOX_SIZE, BAR_HEIGHT, 0.7f, 0.7f, 0.7f, 0.8f);

        char letter = '?';
        switch (u->role) { 
        case Definitions::Role::Commander: letter = 'C'; break;
        case Definitions::Role::Warrior: letter = 'W'; break;
        case Definitions::Role::Medic: letter = 'M'; break;
        case Definitions::Role::Supplier: letter = 'P'; break;
        }

        glColor4f(0.0f, 0.0f, 0.0f, 1.0f);

        float x_pos = x_role + (ROLE_BOX_SIZE - 9) / 2.0f;
        float y_pos = y + (BAR_HEIGHT - 15) / 2.0f;
        glRasterPos2f(x_pos, y_pos + 1.0f);

        glutBitmapCharacter(GLUT_BITMAP_9_BY_15, letter);

        blueIdx++;
    }

    int orangeIdx = 0;
    for (const auto* u : g_units) { 
        if (u->team != Team::Orange) continue;

        float x_role = W - PADDING_X - ROLE_BOX_SIZE;
        float x_bar = x_role - ROLE_BOX_MARGIN - BAR_WIDTH;
        float y = (H - PADDING_Y) - (orangeIdx * (BAR_HEIGHT + BAR_MARGIN_Y)) - BAR_HEIGHT;

        float healthPercent = u->hpNorm();

        drawQuad(x_bar, y, BAR_WIDTH, BAR_HEIGHT, 0.2f, 0.2f, 0.2f, 0.8f);
        drawQuad(x_bar, y, BAR_WIDTH * healthPercent, BAR_HEIGHT, 1.0f, 0.5f, 0.0f, 1.0f);



        drawQuad(x_role, y, ROLE_BOX_SIZE, BAR_HEIGHT, 0.7f, 0.7f, 0.7f, 0.8f);

        char letter = '?';
        switch (u->role) { 
        case Definitions::Role::Commander: letter = 'C'; break;
        case Definitions::Role::Warrior: letter = 'W'; break;
        case Definitions::Role::Medic: letter = 'M'; break;
        case Definitions::Role::Supplier: letter = 'P'; break;
        }

        glColor4f(0.0f, 0.0f, 0.0f, 1.0f);

        float x_pos = x_role + (ROLE_BOX_SIZE - 9) / 2.0f;
        float y_pos = y + (BAR_HEIGHT - 15) / 2.0f;
        glRasterPos2f(x_pos, y_pos + 1.0f);

        glutBitmapCharacter(GLUT_BITMAP_9_BY_15, letter);


        orangeIdx++;
    }

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();

    glPopAttrib();
}

static void drawGameOverMessage()
{
    if (!g_gameOver) return; 

    const int W = glutGet(GLUT_WINDOW_WIDTH);
    const int H = glutGet(GLUT_WINDOW_HEIGHT);

    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_VIEWPORT_BIT | GL_TRANSFORM_BIT);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, (double)W, 0.0, (double)H, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    std::string winMsg;
    if (g_winningTeam == Definitions::Team::Blue) {
        winMsg = "BLUE TEAM IS THE WINNER"; 
        glColor4f(0.2f, 0.6f, 1.0f, 1.0f); 
    }
    else {
        winMsg = "ORANGE TEAM IS THE WINNER"; 
        glColor4f(1.0f, 0.55f, 0.1f, 1.0f);
    }
    std::string restartMsg = "FOR A NEW GAME PRESS N, FOR EXIT PRESS E";

    float winMsgWidth = winMsg.length() * 9.0f;
    float restartMsgWidth = restartMsg.length() * 9.0f;

    float x_win = (W - winMsgWidth) / 2.0f;
    float y_win = H - 40.0f; 

    float x_restart = (W - restartMsgWidth) / 2.0f;
    float y_restart = H - 65.0f; 

    glRasterPos2f(x_win, y_win);
    for (const char* c = winMsg.c_str(); *c != '\0'; ++c) {
        glutBitmapCharacter(GLUT_BITMAP_9_BY_15, *c);
    }

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    glRasterPos2f(x_restart, y_restart);
    for (const char* c = restartMsg.c_str(); *c != '\0'; ++c) {
        glutBitmapCharacter(GLUT_BITMAP_9_BY_15, *c);
    }

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glPopAttrib();
}

static void drawStartHint()
{
    if (g_gameOver || g_commanderEnabled) return;

    const int W = glutGet(GLUT_WINDOW_WIDTH);
    const int H = glutGet(GLUT_WINDOW_HEIGHT);

    glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_VIEWPORT_BIT | GL_TRANSFORM_BIT);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    glOrtho(0.0, (double)W, 0.0, (double)H, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    const std::string msg = "PRESS K TO START THE MATCH";
    const float msgW = msg.length() * 9.0f;   
    const float x = (W - msgW) / 2.0f;
    const float y = H - 40.0f;                

    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);      
    glRasterPos2f(x, y);
    for (const char* c = msg.c_str(); *c; ++c)
        glutBitmapCharacter(GLUT_BITMAP_9_BY_15, *c);

    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glPopMatrix();
    glPopAttrib();
}


static void computeMapCounts()
{
    g_cntRock = g_cntTree = g_cntWater = g_cntDepot = 0;
    for (int r = 0; r < GRID_SIZE; ++r)
        for (int c = 0; c < GRID_SIZE; ++c) {
            int v = g_grid.at(r, c);
            if (v == ROCK)               ++g_cntRock;
            else if (v == TREE)          ++g_cntTree;
            else if (v == WATER)         ++g_cntWater;
            else if (v == DEPOT_AMMO || v == DEPOT_MED) ++g_cntDepot;
        }
}

static void computeUnitCounts()
{
    g_blueCount = g_orangeCount = 0;
    for (const auto* u : g_units) {
        if (!u->isAlive) continue;
        if (u->team == Team::Blue) ++g_blueCount; else ++g_orangeCount; 
    }
}

static void rebuildVisibility()
{
    if (!g_showVisibility) { AI::Visibility::Clear(g_vis); return; }
    AI::Visibility::BuildTeamVisibility(g_grid, g_units, g_visTeam, SIGHT_RANGE, g_vis);
}

static void drawTargetCross()
{
    if (g_targetRow < 0 || g_targetCol < 0) return;
    const float x = g_targetCol * CELL_PX + CELL_PX * 0.5f;
    const float y = g_targetRow * CELL_PX + CELL_PX * 0.5f;
    const float half = CELL_PX * 0.45f;

    glPushAttrib(GL_ENABLE_BIT | GL_LINE_BIT | GL_COLOR_BUFFER_BIT);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
    glLineWidth(2.f);

    glColor3f(1.f, 0.f, 1.f);
    glBegin(GL_LINES);
    glVertex2f(x - half, y); glVertex2f(x + half, y);
    glVertex2f(x, y - half); glVertex2f(x, y + half);
    glEnd();

    glPopAttrib();
}

static void drawTeamRings()
{
    const float GLYPH_W_FACTOR = 0.42f;
    const float GLYPH_H_FACTOR = 0.55f;
    const float MARGIN_PX = 2.5f;

    glPushAttrib(GL_ENABLE_BIT | GL_LINE_BIT | GL_COLOR_BUFFER_BIT);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glLineWidth(1.0f);

    auto drawRing = [](float x, float y, float rad) {
        glBegin(GL_LINE_LOOP);
        for (int i = 0; i < 48; ++i) {
            float a = (2.f * 3.14159265f) * (i / 48.f);
            glVertex2f(x + rad * std::cos(a), y + rad * std::sin(a));
        }
        glEnd();
        };

    for (const auto* u : g_units) {
        if (!u->isAlive) continue;
        const float cx = u->col * CELL_PX + CELL_PX * 0.5f;
        const float cy = u->row * CELL_PX + CELL_PX * 0.5f;
        const float halfW = (CELL_PX * GLYPH_W_FACTOR) * 0.5f;
        const float halfH = (CELL_PX * GLYPH_H_FACTOR) * 0.5f;
        const float glyphHalfDiag = std::sqrt(halfW * halfW + halfH * halfH);
        const float rad = glyphHalfDiag + MARGIN_PX;
        if (u->team == Team::Blue) glColor4f(0.20f, 0.60f, 1.00f, 0.95f);
        else                      glColor4f(1.00f, 0.55f, 0.10f, 0.95f);
        drawRing(cx, cy, rad);
    }

    glPopAttrib();
}


static void DebugOverlayDraw()
{
    drawTargetCross();
    drawTeamRings();
    g_combat.draw();
    drawTeamHealthBars();
    drawGameOverMessage();
    drawStartHint();
}

// Random & anchors respecting playfield
static inline bool inHalf(Definitions::Team t, int c) {
    return (t == Definitions::Team::Blue) ? (c < GRID_SIZE / 2) : (c >= GRID_SIZE / 2);
}

static bool isSpawnableCell(int r, int c)
{
    if (r < 0 || r >= GRID_SIZE || c < 0 || c >= GRID_SIZE) return false;
    if (!AI::Pathfinding::inPlayfield(r, c)) return false;
    if (!AI::Pathfinding::IsWalkableForMovement(g_grid.at(r, c))) return false;
    if (AI::Pathfinding::IsOccupied(r, c)) return false;
    return true;
}

static bool randomFreeCellInHalf(Definitions::Team team, int& outR, int& outC)
{
    const int tries = 500;
    for (int k = 0; k < tries; ++k) {
        int r = std::rand() % GRID_SIZE;
        int c = std::rand() % GRID_SIZE;
        if (!AI::Pathfinding::inPlayfield(r, c)) continue;
        if (!inHalf(team, c)) continue;
        if (!AI::Pathfinding::IsWalkableForMovement(g_grid.at(r, c))) continue;
        if (AI::Pathfinding::IsOccupied(r, c)) continue;
        outR = r; outC = c;
        return true;
    }
    return false;
}

static void randomizeAllWarriorsInTeams()
{
    if (!g_randomizeWarriorsSpawn) return;
    g_randBlueWarriorId = -1;
    g_randOrangeWarriorId = -1;

    for (auto* u : g_units) {
        if (!u->isAlive || u->role != Role::Warrior) continue;
        int r, c;
        if (randomFreeCellInHalf(u->team, r, c)) {
            u->row = r; u->col = c; u->isMoving = false;
            if (u->team == Team::Blue && g_randBlueWarriorId == -1)   g_randBlueWarriorId = u->id;
            if (u->team == Team::Orange && g_randOrangeWarriorId == -1) g_randOrangeWarriorId = u->id;
        }
    }
}

static bool selectAnchorForTeam(Definitions::Team team, int& outR, int& outC)
{
    const int tries = AI::Commander::ANCHOR_RETRIES;
    const float safeMax = AI::Commander::SAFE_RISK_MAX;
    int bestR = -1, bestC = -1; float bestRisk = 1e9f;

    for (int k = 0; k < tries; ++k) {
        int r = std::rand() % GRID_SIZE;
        int c = std::rand() % GRID_SIZE;
        if (!isSpawnableCell(r, c)) continue;
        if (!inHalf(team, c)) continue;
        float risk = g_smap.at(r, c);
        if (risk <= safeMax && risk < bestRisk) {
            bestRisk = risk; bestR = r; bestC = c;
            if (bestRisk <= 0.05f) break;
        }
    }
    if (bestR < 0) {
        for (int k = 0; k < tries; ++k) {
            int r = std::rand() % GRID_SIZE;
            int c = std::rand() % GRID_SIZE;
            if (!isSpawnableCell(r, c)) continue;
            if (!inHalf(team, c)) continue;
            float risk = g_smap.at(r, c);
            if (risk < bestRisk) { bestRisk = risk; bestR = r; bestC = c; }
        }
    }
    if (bestR >= 0) { outR = bestR; outC = bestC; return true; }
    return false;
}

static void placeFirstByRole(Definitions::Team team, Definitions::Role role, int r, int c)
{
    for (auto* u : g_units) {
        if (!u->isAlive) continue;
        if (u->team == team && u->role == role) {
            if (!AI::Pathfinding::IsOccupied(r, c)) {
                u->row = r; u->col = c; u->isMoving = false;
            }
            break;
        }
    }
}

static void sanitizeWorldOutsidePlayfield()
{
    const int EMPTY_CELL = Definitions::Cell::EMPTY;
    for (int r = 0; r < GRID_SIZE; ++r) {
        for (int c = 0; c < GRID_SIZE; ++c) {
            if (AI::Pathfinding::inPlayfield(r, c)) continue;
            int v = g_grid.at(r, c);
            if (v == ROCK || v == TREE || v == DEPOT_AMMO || v == DEPOT_MED || v == WATER) {
                g_grid.set(r, c, EMPTY_CELL);
            }
        }
    }
}

static void autoEnemySightings(int frameCounter,
    const std::vector<Models::Unit*>& blueTeam,
    const std::vector<Models::Unit*>& orangeTeam)
{
    if (frameCounter % 15 != 0) return;

    auto reportLOS = [&](const std::vector<Models::Unit*>& mine, const std::vector<Models::Unit*>& theirs)
        {
            for (auto* me : mine) {
                if (!me || !me->isAlive) continue;
                for (auto* en : theirs) {
                    if (!en || !en->isAlive) continue;
                    int dr = en->row - me->row;
                    int dc = en->col - me->col;
                    if (dr * dr + dc * dc > SIGHT_RANGE * SIGHT_RANGE) continue;
                    if (!AI::Visibility::HasLineOfSight(g_grid, me->row, me->col, en->row, en->col)) continue;

                    AI::EventBus::instance().publish(AI::Message{
                        AI::EventType::EnemySighted,
                        me->id, -1, en->row, en->col, 0
                        });
                    break;
                }
            }
        };

    reportLOS(blueTeam, orangeTeam);
    reportLOS(orangeTeam, blueTeam);
}


// Build world
static void buildTestWorld()
{
    for (auto* u : g_units) {
        delete u; 
    }
    g_units.clear();

    g_grid = Models::Grid();
    sanitizeWorldOutsidePlayfield();

    const int rowBlue = GRID_SIZE / 6;
    const int startBlue = GRID_SIZE / 10;
    const int step = 3;

    g_units.reserve(10); 

    g_units.push_back(new Models::Unit(Team::Blue, Role::Commander, rowBlue, startBlue + step * 0));

    g_units.push_back(new Models::Warrior(Team::Blue, rowBlue, startBlue + step * 1));

    g_units.push_back(new Models::Unit(Team::Blue, Role::Medic, rowBlue, startBlue + step * 2));
    g_units.push_back(new Models::Unit(Team::Blue, Role::Supplier, rowBlue, startBlue + step * 3));

    g_units.push_back(new Models::Warrior(Team::Blue, rowBlue, startBlue + step * 4));

    const int rowOrange = GRID_SIZE - GRID_SIZE / 6;
    const int startOrange = GRID_SIZE - GRID_SIZE / 10;
    g_units.push_back(new Models::Unit(Team::Orange, Role::Commander, rowOrange, startOrange - step * 0));
    g_units.push_back(new Models::Warrior(Team::Orange, rowOrange, startOrange - step * 1));
    g_units.push_back(new Models::Unit(Team::Orange, Role::Medic, rowOrange, startOrange - step * 2));
    g_units.push_back(new Models::Unit(Team::Orange, Role::Supplier, rowOrange, startOrange - step * 3));
    g_units.push_back(new Models::Warrior(Team::Orange, rowOrange, startOrange - step * 4));

    int nextUnitId = 1;
    for (auto* u : g_units) {
        u->id = nextUnitId++;
        u->isMoving = false;
        u->isFighting = false;
        u->isAutonomous = false;
        u->isAlive = true;
        if (u->team == Team::Blue) {
            if (u->role == Role::Commander) g_commanderBlue.setUnitId(u->id);
        }
        else {
            if (u->role == Role::Commander) g_commanderOrange.setUnitId(u->id);
        }
    }

    randomizeAllWarriorsInTeams();
    computeMapCounts();
    computeUnitCounts();
    g_smap.RebuildSecurityMap(g_grid);
    rebuildVisibility();

    int ar, ac;
    g_blueAnchorValid = false;
    g_orangeAnchorValid = false;
    if (selectAnchorForTeam(Team::Blue, ar, ac)) {
        placeFirstByRole(Team::Blue, Role::Commander, ar, ac);
        g_commanderBlue.anchorR = ar; g_commanderBlue.anchorC = ac;
        g_blueAnchorValid = true;
    }
    if (selectAnchorForTeam(Team::Orange, ar, ac)) {
        placeFirstByRole(Team::Orange, Role::Commander, ar, ac);
        g_commanderOrange.anchorR = ar; g_commanderOrange.anchorC = ac;
        g_orangeAnchorValid = true;
    }

    g_targetRow = g_targetCol = -1;
    g_commanderEnabled = false;
    g_frameCounter = 0;

    g_combat.bindUnits(&g_units);

    g_commanderBlue.initSubscriptions();
    g_commanderOrange.initSubscriptions();

    g_gameOver = false;
}

// Display / Idle / Input
static void display()
{
    static int frames = 0, t0 = 0;
    int t = glutGet(GLUT_ELAPSED_TIME);
    frames++;
    if (t - t0 >= 500) {
        g_fps = frames * 1000.0f / float(t - t0);
        t0 = t; frames = 0;
    }

    char buf1[200], buf2[128], buf3[128], buf4[160], buf7[128], bufCmd[64];
    std::snprintf(buf1, sizeof(buf1),
        "FPS: %.1f  Grid: %dx%d Cell: %dpx Security:%s(RClk) Visibility:%s(V, team=%s)",
        g_fps, GRID_SIZE, GRID_SIZE, CELL_PX,
        g_showSecurity ? "ON" : "OFF",
        g_showVisibility ? "ON" : "OFF",
        (g_visTeam == Team::Blue ? "Blue" : "Orange"));
    std::snprintf(buf2, sizeof(buf2), "Cells ROCK:%ld TREE:%ld WATER:%ld DEPOTS:%ld",
        g_cntRock, g_cntTree, g_cntWater, g_cntDepot);
    std::snprintf(buf3, sizeof(buf3), "Units BLUE:%d ORANGE:%d", g_blueCount, g_orangeCount);
    std::snprintf(buf4, sizeof(buf4), "SMap max=%.2f samples=%d decay=%.3f",
        g_smap.maxValue(), SECURITY_SAMPLES, SECURITY_DECAY);
    std::snprintf(bufCmd, sizeof(bufCmd), "Commander: %s (K)", g_commanderEnabled ? "ON" : "OFF");

    std::vector<std::string> hud; hud.reserve(16);
    hud.push_back(buf1); hud.push_back(buf2); hud.push_back(buf3); hud.push_back(buf4);
    hud.push_back(bufCmd);

    static int lastPrintMs = 0;
    int nowMs = glutGet(GLUT_ELAPSED_TIME);
    if (nowMs - lastPrintMs > 1000) {
        printf("\n--- Frame %d ---\n", g_frameCounter);
        for (const auto& s : hud) std::printf("%s\n", s.c_str());
        fflush(stdout);
        lastPrintMs = nowMs;
    }

    if (g_showVisibility)
        Painting::RenderFrameWithVisibility_Overlay(g_grid, g_units, g_vis, hud, &DebugOverlayDraw);
    else if (g_showSecurity)
        Painting::RenderFrameWithSecurity_Overlay(g_grid, g_units, g_smap, hud, &DebugOverlayDraw);
    else
        Painting::RenderFrameWithHUD_Overlay(g_grid, g_units, hud, &DebugOverlayDraw);
}


static void idle()
{
    if (!g_gameOver) {
        g_combat.tickBullets(g_grid, g_smap);
    }

    std::vector<Models::Unit*> liveBlueUnits;
    std::vector<Models::Unit*> liveOrangeUnits;
    for (auto* u : g_units) {
        if (u->isAlive) {
            if (u->team == Team::Blue) {
                liveBlueUnits.push_back(u);
            }
            else {
                liveOrangeUnits.push_back(u);
            }
        }
    }

    struct TeamStatus {
        Models::Unit* commander = nullptr;
        bool commanderAlive = false;
        bool warriorsAlive = false;
        bool supportsAlive = false;
    };
    std::map<Team, TeamStatus> teamStatus;

    for (auto* u : liveBlueUnits) {
        TeamStatus& st = teamStatus[Team::Blue];
        if (u->role == Role::Commander) { st.commander = u; st.commanderAlive = true; }
        else if (u->role == Role::Warrior) { st.warriorsAlive = true; }
        else if (u->role == Role::Medic || u->role == Role::Supplier) { st.supportsAlive = true; }
    }
    for (auto* u : liveOrangeUnits) {
        TeamStatus& st = teamStatus[Team::Orange];
        if (u->role == Role::Commander) { st.commander = u; st.commanderAlive = true; }
        else if (u->role == Role::Warrior) { st.warriorsAlive = true; }
        else if (u->role == Role::Medic || u->role == Role::Supplier) { st.supportsAlive = true; }
    }

    if (!g_gameOver) {
        bool blueWarriorsExist = teamStatus[Team::Blue].warriorsAlive;
        bool orangeWarriorsExist = teamStatus[Team::Orange].warriorsAlive;

        if (!blueWarriorsExist && orangeWarriorsExist) {
            g_gameOver = true;
            g_winningTeam = Team::Orange; 
        }
        else if (blueWarriorsExist && !orangeWarriorsExist) {
            g_gameOver = true;
            g_winningTeam = Team::Blue; 
        }
    }


    if (!g_gameOver) {

        for (auto& kv : teamStatus) {
            Team team = kv.first;
            TeamStatus& status = kv.second;
            std::vector<Models::Unit*>& teamPtrs = (team == Team::Blue) ? liveBlueUnits : liveOrangeUnits;

            if (!status.commanderAlive && status.warriorsAlive) {
                bool changed = false;
                for (auto* u : teamPtrs) {
                    if (u->role == Role::Warrior && !u->isAutonomous) {
                        u->isAutonomous = true;
                        changed = true;
                    }
                }
                if (changed) printf("[CONTINGENCY/%s] Commander down! Warriors autonomous.\n", teamTag(team));
            }
            else if (!status.warriorsAlive && status.commanderAlive) {
                if (status.commander && !status.commander->isFighting) {
                    printf("[CONTINGENCY/%s] Warriors down! Commander fights.\n", teamTag(team));
                    status.commander->isFighting = true;
                }
            }
        }

        if (g_commanderEnabled) {
            autoEnemySightings(g_frameCounter, liveBlueUnits, liveOrangeUnits);
            g_commanderBlue.tick(g_grid, liveBlueUnits, liveOrangeUnits, g_frameCounter);
            g_commanderOrange.tick(g_grid, liveOrangeUnits, liveBlueUnits, g_frameCounter);
        }

        for (auto* u : g_units) {
            if (u->isAlive) {
                if (u->m_fsm) {
                    u->m_fsm->Update();
                }
                if (u->role == Definitions::Role::Warrior) {
                    Models::Warrior* warrior = static_cast<Models::Warrior*>(u);
                    warrior->CheckAndReportStatus();
                }
            }
        }

    }

    computeUnitCounts();
    ++g_frameCounter;
    glutPostRedisplay();
}


static void mouse(int button, int state, int x, int y)
{
    if (state != GLUT_DOWN) return;

    if (button == GLUT_RIGHT_BUTTON) {
        g_showSecurity = !g_showSecurity;
        if (g_showSecurity) g_showVisibility = false;
        rebuildVisibility();
        return;
    }

    if (button == GLUT_LEFT_BUTTON) {
        int winW = glutGet(GLUT_WINDOW_WIDTH);
        int winH = glutGet(GLUT_WINDOW_HEIGHT);
        if (winW <= 0 || winH <= 0) return;

        float fx = (float)x / (float)winW;
        float fy = (float)y / (float)winH;

        int col = (int)std::floor(fx * GRID_SIZE);
        int row = (int)std::floor((1.0f - fy) * GRID_SIZE);

        if (col < 0) col = 0; else if (col >= GRID_SIZE) col = GRID_SIZE - 1;
        if (row < 0) row = 0; else if (row >= GRID_SIZE) row = GRID_SIZE - 1;

        g_targetCol = col;
        g_targetRow = row;
        printf("Target set to (%d, %d)\n", row, col);
    }
}

static Models::Unit* findCommander(Definitions::Team team) {
    for (auto* u : g_units) {
        if (!u) continue;
        if (u->isAlive && u->role == Definitions::Role::Commander && u->team == team)
            return u;
    }
    return nullptr;
}


static void keyboard(unsigned char key, int , int )
{
    if (g_gameOver) {
        switch (key) {
        case 'n': case 'N':
            buildTestWorld(); 
            break;
        case 'e': case 'E':
            exit(0); 
            break;
        default: break; 
        }
        return;
    }

    switch (key) {
    case 'k': case 'K':
        g_commanderEnabled = !g_commanderEnabled;
        printf("Commander AI %s\n", g_commanderEnabled ? "ENABLED" : "DISABLED");
        break;

    case 'x': case 'X': { 
        auto* cmd = findCommander(Definitions::Team::Blue);
        if (cmd && cmd->isAlive) {
            cmd->isAlive = false;
  
            AI::EventBus::instance().publish(AI::Message{
                AI::EventType::CommanderDown, cmd->id, -1, -1, -1, (int)cmd->team
                });
            std::puts("[TEST] Blue commander was killed (CommanderDown sent).");

            for (auto* u : g_units) {
                if (u && u->isAlive && u->team == Definitions::Team::Blue) {
                    u->isAutonomous = true;
                    std::printf("[AUTO] Unit #%d (%c) is now autonomous (Blue).\n",
                        u->id, u->roleLetter());
                }
            }
        }
        else {
            std::puts("[TEST] No alive Blue commander to kill.");
        }
        break;
    }

    case 'o': case 'O': { 
        auto* cmd = findCommander(Definitions::Team::Orange);
        if (cmd && cmd->isAlive) {
            cmd->isAlive = false;
            AI::EventBus::instance().publish(AI::Message{
                AI::EventType::CommanderDown, cmd->id, -1, -1, -1, (int)cmd->team
                });
            std::puts("[TEST] Orange commander was killed (CommanderDown sent).");

            for (auto* u : g_units) {
                if (u && u->isAlive && u->team == Definitions::Team::Orange) {
                    u->isAutonomous = true;
                    std::printf("[AUTO] Unit #%d (%c) is now autonomous (Orange).\n",
                        u->id, u->roleLetter());
                }
            }
        }
        else {
            std::puts("[TEST] No alive Orange commander to kill.");
        }
        break;
    }


    default: break; 
    }
}


static void reshape(int w, int h)
{
    if (h == 0) h = 1;
    glViewport(0, 0, w, h);

    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    const int worldW = GRID_SIZE * CELL_PX;
    const int worldH = GRID_SIZE * CELL_PX;
    glOrtho(0.0, (double)worldW, 0.0, (double)worldH, -1.0, 1.0);

    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

int main(int argc, char** argv)
{
    std::srand((unsigned)std::time(nullptr));

    buildTestWorld();

    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGB | GLUT_DOUBLE);

    const int W = GRID_SIZE * CELL_PX;
    const int H = GRID_SIZE * CELL_PX;
    glutInitWindowSize(W, H);
    glutInitWindowPosition(100, 50);
    glutCreateWindow("Tactical AI Simulation");

    Painting::RenderInit(W, H);
    Painting::SetHudEnabled(false);

    glutDisplayFunc(display);
    glutIdleFunc(idle);
    glutMouseFunc(mouse);
    glutKeyboardFunc(keyboard);
    glutReshapeFunc(reshape);

    glutMainLoop();
    return 0;
}