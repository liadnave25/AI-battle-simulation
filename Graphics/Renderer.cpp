#include "Renderer.h"
#include "Definitions.h"
#include "Grid.h"
#include "Units.h"
#include "SecurityMap.h"
#include "Visibility.h"
#include "glut.h"

#include <vector>
#include <string>
#include <algorithm>

using namespace Definitions;

namespace Painting {
    static bool gHudEnabled = true;
    void SetHudEnabled(bool on) { gHudEnabled = on; }


    static int Wpx = 0, Hpx = 0;

    static void drawFilledRect(int x, int y, int w, int h, Color c)
    {
        glColor3f(c.r, c.g, c.b);
        glBegin(GL_QUADS);
        glVertex2i(x, y); glVertex2i(x + w, y);
        glVertex2i(x + w, y + h); glVertex2i(x, y + h);
        glEnd();
    }

    static void drawRectOutline(int x, int y, int w, int h, float r, float g, float b)
    {
        glColor3f(r, g, b);
        glBegin(GL_LINE_LOOP);
        glVertex2i(x, y); glVertex2i(x + w, y);
        glVertex2i(x + w, y + h); glVertex2i(x, y + h);
        glEnd();
    }

    static void drawBitmapTextLine(int x, int y, const std::string& s)
    {
        glRasterPos2i(x, y);
        for (unsigned char ch : s) glutBitmapCharacter(GLUT_BITMAP_9_BY_15, (int)ch);
    }

    inline int cellX(int c) { return c * CELL_PX; }
    inline int cellY(int r) { return r * CELL_PX; }

    static void drawCellIcon(const Models::Grid&, int r, int c, int v)
    {
        const int x = c * CELL_PX;
        const int y = r * CELL_PX;

        auto plusAt = [&](int cx, int cy, int len, Color col) {
            glColor3f(col.r, col.g, col.b);
            glBegin(GL_LINES);
            glVertex2i(cx - len, cy); glVertex2i(cx + len, cy);
            glVertex2i(cx, cy - len); glVertex2i(cx, cy + len);
            glEnd();
            };
        auto smallBox = [&](int x0, int y0, int w, int h, Color col, bool outline) {
            glColor3f(col.r, col.g, col.b);
            glBegin(GL_QUADS);
            glVertex2i(x0, y0); glVertex2i(x0 + w, y0);
            glVertex2i(x0 + w, y0 + h); glVertex2i(x0, y0 + h);
            glEnd();
            if (outline) {
                glColor3f(0, 0, 0);
                glBegin(GL_LINE_LOOP);
                glVertex2i(x0, y0); glVertex2i(x0 + w, y0);
                glVertex2i(x0 + w, y0 + h); glVertex2i(x0, y0 + h);
                glEnd();
            }
            };

        switch (v) {

        case TREE: {
            const int cx = x + CELL_PX / 2;
            const int cy = y + CELL_PX / 2;
            const float margin = 1.0f;                        
            const int   len = std::max(3, int((CELL_PX * 0.5f) - margin));

            glLineWidth(3.f);                                 
            plusAt(cx, cy, len, COLOR_TREE);
            glLineWidth(1.f);                                 
        } break;

        case ROCK: {
            const int margin = 1;                             
            const int sz = std::max(4, CELL_PX - 2 * margin); 
            const int x0 = x + (CELL_PX - sz) / 2;
            const int y0 = y + (CELL_PX - sz) / 2;

            smallBox(x0, y0, sz, sz, COLOR_ROCK, true);       
        } break;


        case WATER: {
            drawFilledRect(x, y, CELL_PX, CELL_PX, COLOR_WATER);
        } break;

        case DEPOT_AMMO:
        case DEPOT_MED: {
            const int pad = std::max(2, CELL_PX / 5);
            smallBox(x + pad, y + pad, CELL_PX - 2 * pad, CELL_PX - 2 * pad, COLOR_DEPOT, true);
        } break;

        default: {
            drawFilledRect(x, y, CELL_PX, CELL_PX, COLOR_BG);
        } break;
        }
    }

    static void drawUnits(const Models::Grid& , const std::vector<Models::Unit*>& units)
    {
        glPushAttrib(GL_ENABLE_BIT | GL_COLOR_BUFFER_BIT | GL_LINE_BIT | GL_POINT_BIT);
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);
        glDisable(GL_BLEND);

        for (const auto* u : units) {
            if (!u->isAlive) continue;

            char ch = '?';
            switch (u->role) {
            case Role::Commander: ch = 'C'; break;
            case Role::Warrior:   ch = 'W'; break;
            case Role::Medic:     ch = 'M'; break;
            case Role::Supplier:  ch = 'P'; break;
            default: break;
            }

            void* font = (CELL_PX >= 20) ? GLUT_BITMAP_HELVETICA_18
                : (CELL_PX >= 14) ? GLUT_BITMAP_9_BY_15
                : GLUT_BITMAP_8_BY_13;

            const int fw = (font == GLUT_BITMAP_HELVETICA_18 ? 10 :
                (font == GLUT_BITMAP_9_BY_15 ? 9 : 8));
            const int fh = (font == GLUT_BITMAP_HELVETICA_18 ? 18 :
                (font == GLUT_BITMAP_9_BY_15 ? 15 : 13));

            const int cellX0 = u->col * CELL_PX;
            const int cellY0 = u->row * CELL_PX;
            const int tx = cellX0 + (CELL_PX - fw) / 2;
            const int ty = cellY0 + (CELL_PX - fh) / 2;

            glColor3f(0.f, 0.f, 0.f);
            glRasterPos2i(tx, ty);
            glutBitmapCharacter(font, (int)ch);
        }

        glPopAttrib();
    }

    static void drawHUD(const std::vector<std::string>& lines)
    {
        int x = 8;
        int y = GRID_SIZE * CELL_PX - 16;
        glColor3f(0, 0, 0);
        for (const auto& s : lines) {
            drawBitmapTextLine(x, y, s);
            y -= 18;
            if (y < 10) break;
        }
    }

    void RenderInit(int windowW, int windowH)
    {
        Wpx = windowW; Hpx = windowH;
        glViewport(0, 0, Wpx, Hpx);

        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        gluOrtho2D(0.0, Wpx, 0.0, Hpx);

        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        glDisable(GL_DEPTH_TEST);
        glDisable(GL_CULL_FACE);

        glClearColor(COLOR_BG.r, COLOR_BG.g, COLOR_BG.b, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glutSwapBuffers();
    }

    void RenderFrame(const Models::Grid& grid, const std::vector<Models::Unit*>& units)
    {
        glClear(GL_COLOR_BUFFER_BIT);

        for (int r = 0; r < GRID_SIZE; ++r)
            for (int c = 0; c < GRID_SIZE; ++c)
                drawCellIcon(grid, r, c, grid.at(r, c));

        drawUnits(grid, units);

        glutSwapBuffers();
    }

    void RenderFrameWithHUD(const Models::Grid& grid,
        const std::vector<Models::Unit*>& units,
        const std::vector<std::string>& hudLines)
    {
        glClear(GL_COLOR_BUFFER_BIT);

        for (int r = 0; r < GRID_SIZE; ++r)
            for (int c = 0; c < GRID_SIZE; ++c)
                drawCellIcon(grid, r, c, grid.at(r, c));

        drawUnits(grid, units);
        if (gHudEnabled) drawHUD(hudLines);

        glutSwapBuffers();
    }

    void RenderFrameWithSecurity(const Models::Grid& grid,
        const std::vector<Models::Unit*>& units,
        const Simulation::SecurityMap& smap,
        const std::vector<std::string>& hudLines)
    {
        glClear(GL_COLOR_BUFFER_BIT);

        for (int r = 0; r < GRID_SIZE; ++r)
            for (int c = 0; c < GRID_SIZE; ++c)
                drawCellIcon(grid, r, c, grid.at(r, c));

        const auto& A = smap.data();
        const float M = std::max(0.001f, smap.maxValue());
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        for (int r = 0; r < GRID_SIZE; ++r) {
            for (int c = 0; c < GRID_SIZE; ++c) {
                float t = A[r][c] / M;
                t = std::min(1.f, std::max(0.f, t));
                int x = cellX(c), y = cellY(r);
                glColor4f(0.05f, 0.8f, 0.2f, 0.28f * t);
                glBegin(GL_QUADS);
                glVertex2i(x, y); glVertex2i(x + CELL_PX, y);
                glVertex2i(x + CELL_PX, y + CELL_PX); glVertex2i(x, y + CELL_PX);
                glEnd();
            }
        }
        glDisable(GL_BLEND);

        drawUnits(grid, units);
        if (gHudEnabled) drawHUD(hudLines);
        glutSwapBuffers();
    }

    void RenderFrameWithVisibility(const Models::Grid& grid,
        const std::vector<Models::Unit*>& units,
        const AI::Visibility::BArray& vis,
        const std::vector<std::string>& hudLines)
    {
        glClear(GL_COLOR_BUFFER_BIT);

        for (int r = 0; r < GRID_SIZE; ++r) {
            for (int c = 0; c < GRID_SIZE; ++c) {
                int v = grid.at(r, c);
                drawCellIcon(grid, r, c, v);

                if (!vis[r][c]) {
                    int x = cellX(c), y = cellY(r);
                    glColor4f(0.f, 0.f, 0.f, 0.45f);
                    glBegin(GL_QUADS);
                    glVertex2i(x, y); glVertex2i(x + CELL_PX, y);
                    glVertex2i(x + CELL_PX, y + CELL_PX); glVertex2i(x, y + CELL_PX);
                    glEnd();
                }
            }
        }

        drawUnits(grid, units);
        if (gHudEnabled) drawHUD(hudLines);
        glutSwapBuffers();
    }

    void RenderFrame_Overlay(const Models::Grid& grid,
        const std::vector<Models::Unit*>& units,
        OverlayDrawFn overlay)
    {
        glClear(GL_COLOR_BUFFER_BIT);

        for (int r = 0; r < GRID_SIZE; ++r)
            for (int c = 0; c < GRID_SIZE; ++c)
                drawCellIcon(grid, r, c, grid.at(r, c));

        drawUnits(grid, units);

        if (overlay) overlay();
        glutSwapBuffers();
    }

    void RenderFrameWithHUD_Overlay(const Models::Grid& grid,
        const std::vector<Models::Unit*>& units,
        const std::vector<std::string>& hudLines,
        OverlayDrawFn overlay)
    {
        glClear(GL_COLOR_BUFFER_BIT);

        for (int r = 0; r < GRID_SIZE; ++r)
            for (int c = 0; c < GRID_SIZE; ++c)
                drawCellIcon(grid, r, c, grid.at(r, c));

        drawUnits(grid, units);

        if (overlay) overlay(); 
        if (gHudEnabled) drawHUD(hudLines);

        glutSwapBuffers();
    }

    void RenderFrameWithSecurity_Overlay(const Models::Grid& grid,
        const std::vector<Models::Unit*>& units,
        const Simulation::SecurityMap& smap,
        const std::vector<std::string>& hudLines,
        OverlayDrawFn overlay)
    {
        glClear(GL_COLOR_BUFFER_BIT);

        for (int r = 0; r < GRID_SIZE; ++r)
            for (int c = 0; c < GRID_SIZE; ++c)
                drawCellIcon(grid, r, c, grid.at(r, c));

        const auto& A = smap.data();
        const float M = std::max(0.001f, smap.maxValue());
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
        for (int r = 0; r < GRID_SIZE; ++r) {
            for (int c = 0; c < GRID_SIZE; ++c) {
                float t = A[r][c] / M;
                t = std::min(1.f, std::max(0.f, t));
                int x = cellX(c), y = cellY(r);
                glColor4f(0.05f, 0.8f, 0.2f, 0.28f * t);
                glBegin(GL_QUADS);
                glVertex2i(x, y); glVertex2i(x + CELL_PX, y);
                glVertex2i(x + CELL_PX, y + CELL_PX); glVertex2i(x, y + CELL_PX);
                glEnd();
            }
        }
        glDisable(GL_BLEND);

        drawUnits(grid, units);

        if (overlay) overlay();
        if (gHudEnabled) drawHUD(hudLines);

        glutSwapBuffers();
    }

    void RenderFrameWithVisibility_Overlay(const Models::Grid& grid,
        const std::vector<Models::Unit*>& units,
        const AI::Visibility::BArray& vis,
        const std::vector<std::string>& hudLines,
        OverlayDrawFn overlay)
    {
        glClear(GL_COLOR_BUFFER_BIT);

        for (int r = 0; r < GRID_SIZE; ++r) {
            for (int c = 0; c < GRID_SIZE; ++c) {
                int v = grid.at(r, c);
                drawCellIcon(grid, r, c, v);

                if (!vis[r][c]) {
                    int x = cellX(c), y = cellY(r);
                    glColor4f(0.f, 0.f, 0.f, 0.45f);
                    glBegin(GL_QUADS);
                    glVertex2i(x, y); glVertex2i(x + CELL_PX, y);
                    glVertex2i(x + CELL_PX, y + CELL_PX); glVertex2i(x, y + CELL_PX);
                    glEnd();
                }
            }
        }

        drawUnits(grid, units);

        if (overlay) overlay();
        if (gHudEnabled) drawHUD(hudLines);

        glutSwapBuffers();
    }

} // namespace Painting
