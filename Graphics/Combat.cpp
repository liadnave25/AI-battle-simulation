#include "Combat.h"
#include "glut.h"
#include <cmath>
#include <algorithm>

using namespace Definitions;

namespace {
    inline bool blocksShot(int cell) { return (cell == ROCK); }

    inline float dist2(float r1, float c1, float r2, float c2) {
        float dr = r1 - r2, dc = c1 - c2; return dr * dr + dc * dc;
    }

    inline float clampf(float v, float lo, float hi) { return std::max(lo, std::min(hi, v)); }

    inline bool blocksExplosive(const Models::Grid& grid, int r, int c) {
        int cell = grid.at(r, c);
        return (cell == TREE || cell == ROCK || cell == DEPOT_AMMO || cell == DEPOT_MED);
    }

    bool raycastBlocked(const Models::Grid& grid, int r0, int c0, int r1, int c1) {
        int dr = std::abs(r1 - r0), dc = std::abs(c1 - c0);
        int sr = (r0 < r1) ? 1 : -1;
        int sc = (c0 < c1) ? 1 : -1;
        int err = dr - dc;
        int r = r0, c = c0;

        while (!(r == r1 && c == c1)) {
            if (blocksExplosive(grid, r, c)) return true;
            int e2 = 2 * err;
            if (e2 > -dc) { err -= dc; r += sr; }
            if (e2 < dr) { err += dr; c += sc; }
        }
        return false;
    }
}

namespace Combat {

    void System::fireBulletTowards(float r0, float c0, float rT, float cT, Definitions::Team shooterTeam) {
        float dr = rT - r0, dc = cT - c0;
        float L = std::sqrt(dr * dr + dc * dc);
        if (L < 1e-4f) return;
        dr /= L; dc /= L;

        bullets.emplace_back(r0, c0, dr, dc, bulletTTL);
        bullets.back().team = shooterTeam;

        bullets.back().isShrapnel = false;
        bullets.back().colR = 0.f; bullets.back().colG = 0.f; bullets.back().colB = 0.f;
    }


    void System::dropGrenade(float r0, float c0,
        const Models::Grid& grid,
        Definitions::Team shooterTeam) {
        applyGrenadeAoE(grid, r0, c0, shooterTeam);
        explode(r0, c0, shooterTeam); 
    }

    void System::throwGrenadeParabola(float r0, float c0, float rT, float cT, Definitions::Team shooterTeam) {
        Grenade g;
        g.r = r0 + 0.5f;   
        g.c = c0 + 0.5f;
        g.z = 0.0f;
        g.team = shooterTeam;

        float dr = (rT + 0.5f) - g.r;
        float dc = (cT + 0.5f) - g.c;
        float D = std::sqrt(dr * dr + dc * dc);
        float T = (throwHorizSpeed > 1e-5f ? D / throwHorizSpeed : 0.f);
        int   Ti = (int)std::round(T);
        Ti = std::max(throwMinFrames, std::min(throwMaxFrames, Ti));
        if (Ti <= 0) Ti = throwMinFrames;

        g.vr = (Ti > 0 ? dr / (float)Ti : 0.f);
        g.vc = (Ti > 0 ? dc / (float)Ti : 0.f);

        g.g = -0.08f;                   
        g.vz = -0.5f * g.g * (float)Ti;  

        grenades.push_back(g);
    }

    void System::explode(float r0, float c0, Definitions::Team shooterTeam) {
        const float dAlpha = 2.f * PI / float(grenadeRays);
        for (int i = 0; i < grenadeRays; ++i) {
            float a = i * dAlpha;
            float dr = std::cos(a);
            float dc = std::sin(a);
            bullets.emplace_back(r0, c0, dr, dc, bulletTTL);
            bullets.back().team = shooterTeam;

            bullets.back().isShrapnel = true;
            bullets.back().colR = 1.f; bullets.back().colG = 0.f; bullets.back().colB = 0.f;
        }
    }


    void System::tickBullets(const Models::Grid& grid, Simulation::SecurityMap& smap) {
        auto& risk = smap.data();

        for (auto& b : bullets) {
            if (!b.alive) continue;

            float nr = b.r + bulletSpeed * b.dr;
            float nc = b.c + bulletSpeed * b.dc;

            if (nr < 0 || nr >= GRID_SIZE || nc < 0 || nc >= GRID_SIZE) { b.alive = false; continue; }

            // Map collision
            int cell = grid.at(int(nr), int(nc));
            if (blocksShot(cell)) { b.alive = false; continue; }

            b.r = nr; b.c = nc;
            risk[int(b.r)][int(b.c)] += secmIncrement;

            applyBulletHitUnits(b);

        }

        bullets.erase(std::remove_if(bullets.begin(), bullets.end(),
            [](const Bullet& x) { return !x.alive; }),
            bullets.end());

        for (auto& g : grenades) {
            if (!g.alive) continue;
            bool detonated = stepGrenade(g, grid);
            if (detonated) {
                float gr = std::floor(g.r);
                float gc = std::floor(g.c);
                applyGrenadeAoE(grid, gr, gc, g.team);
                explode(gr, gc, g.team);
            }
        }

        grenades.erase(std::remove_if(grenades.begin(), grenades.end(),
            [](const Grenade& x) { return !x.alive; }),
            grenades.end());
    }

    bool System::stepGrenade(Grenade& g, const Models::Grid& grid) {
        float nr = g.r + g.vr;
        float nc = g.c + g.vc;
        float nz = g.z + g.vz + 0.5f * g.g; 
        g.vz += g.g;

        if (nr < 0 || nr >= GRID_SIZE || nc < 0 || nc >= GRID_SIZE) {
            g.alive = false; return false;
        }

        int rr = (int)std::floor(nr);
        int cc = (int)std::floor(nc);
        int cell = grid.at(rr, cc);
        if (blocksShot(cell) && nz <= 0.25f) {
            g.alive = false; return true; 
        }

        if (nz <= 0.f) {
            g.r = nr; g.c = nc; g.z = 0.f; 
            g.alive = false;
            return true; 
        }

        g.r = nr; g.c = nc; g.z = nz;
        return false;
    }

    void System::applyBulletHitUnits(Bullet& b) {
        if (!b.alive) return;
        if (!units || units->empty()) return;

        const float br = b.r + 0.5f;
        const float bc = b.c + 0.5f;
        const float hit2 = bulletHitRadiusCells * bulletHitRadiusCells;

        for (auto* u : *units) {
            if (!u->isAlive) continue;
            if (!friendlyFire && u->team == b.team) continue;

            const float ur = u->row + 0.5f;
            const float uc = u->col + 0.5f;
            if (dist2(br, bc, ur, uc) <= hit2) {
                u->stats.hp -= Definitions::DAMAGE_BULLET;
                if (u->stats.hp <= 0) { u->stats.hp = 0; u->isAlive = false; }
                b.alive = false;
                return;
            }
        }
    }

    void System::applyGrenadeAoE(const Models::Grid& grid,
        float r0, float c0,
        Definitions::Team shooterTeam) {
        if (!units || units->empty()) return;

        const float R = grenadeRadiusCells;
        const float R2 = R * R;

        const float cx = r0 + 0.5f;
        const float cy = c0 + 0.5f;
        const int   cr = int(std::floor(r0));
        const int   cc = int(std::floor(c0));

        for (auto* u : *units) {
            if (!u->isAlive) continue;
            if (!friendlyFire && u->team == shooterTeam) continue;

            const float ur = u->row + 0.5f, uc = u->col + 0.5f;
            float d2 = dist2(cx, cy, ur, uc);
            if (d2 > R2) continue;

            float d = std::sqrt(d2);
            float t = clampf(1.0f - (d / R), 0.f, 1.f);
            float dmgf = float(grenadeDmgEdge) +
                (float(grenadeDmgCenter) - float(grenadeDmgEdge)) * t;

            if (grenadeRaycastCover) {
                int tr = int(std::floor(ur));
                int tc = int(std::floor(uc));
                if (raycastBlocked(grid, cr, cc, tr, tc)) {
                    dmgf *= grenadeCoverBlockFactor;
                }
            }

            int dmg = int(std::round(dmgf));
            if (dmg <= 0) continue;

            u->stats.hp -= dmg;
            if (u->stats.hp <= 0) { u->stats.hp = 0; u->isAlive = false; }
        }
    }

    void System::draw() const {
        glPushAttrib(GL_ENABLE_BIT | GL_POINT_BIT | GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_LINE_BIT);
        glDisable(GL_TEXTURE_2D);
        glDisable(GL_DEPTH_TEST);

        glMatrixMode(GL_PROJECTION);
        glPushMatrix();
        glLoadIdentity();
        gluOrtho2D(0.0, GRID_SIZE * 1.0, 0.0, GRID_SIZE * 1.0);

        glMatrixMode(GL_MODELVIEW);
        glPushMatrix();
        glLoadIdentity();

        glPointSize(6.f);
        glBegin(GL_POINTS);
        for (const auto& b : bullets) {
            if (!b.alive) continue;

            glColor3f(b.colR, b.colG, b.colB);

            float x = b.c + 0.5f;
            float y = b.r + 0.5f;
            glVertex2f(x, y);
        }
        glEnd();

        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

        for (const auto& g : grenades) {
            if (!g.alive) continue;

            float sx = g.c;
            float sy = g.r;
            float srad = 0.20f; 
            glColor4f(0.f, 0.f, 0.f, 0.35f); 
            glBegin(GL_TRIANGLE_FAN);
            glVertex2f(sx, sy);
            for (int i = 0; i <= 16; ++i) {
                float a = (2.f * PI) * (i / 16.f);
                glVertex2f(sx + srad * std::cos(a), sy + srad * std::sin(a));
            }
            glEnd();

            float gx = g.c;
            float gy = g.r + std::min(0.4f, g.z * 0.35f); 
            float grad = 0.15f;

            glColor3f(g.colR, g.colG, g.colB);

            glBegin(GL_TRIANGLE_FAN);
            glVertex2f(gx, gy);
            for (int i = 0; i <= 16; ++i) {
                float a = (2.f * PI) * (i / 16.f);
                glVertex2f(gx + grad * std::cos(a), gy + grad * std::sin(a));
            }
            glEnd();
        }

        glDisable(GL_BLEND); 

        glPopMatrix();             
        glMatrixMode(GL_PROJECTION);
        glPopMatrix();               
        glMatrixMode(GL_MODELVIEW);
        glPopAttrib();
    }

} // namespace Combat