#pragma once
#include <vector>
#include <algorithm>
#include <cmath>
#include "Definitions.h"
#include "Grid.h"
#include "SecurityMap.h"
#include "Units.h"

namespace Combat {

    struct Bullet {
        float r, c;
        float dr, dc;
        int   ttl;
        bool  alive = true;

        Definitions::Team team = Definitions::Team::Blue;

        float colR = 0.f, colG = 0.f, colB = 0.f;
        bool  isShrapnel = false;

        Bullet(float r0, float c0, float dr_, float dc_, int ttl_)
            : r(r0), c(c0), dr(dr_), dc(dc_), ttl(ttl_) {
        }
    };


    struct Grenade {
        float r = 0.f, c = 0.f, z = 0.f;
        float vr = 0.f, vc = 0.f, vz = 0.f;
        float g = -0.08f;
        Definitions::Team team = Definitions::Team::Blue;
        bool  alive = true;
        float colR = 1.f, colG = 0.f, colB = 0.f;
    };

    struct System {
        float bulletSpeed = 0.55f;       
        int   bulletTTL = 140;          
        int   grenadeRays = 8;        
        float secmIncrement = 0.0015f;  

        int   bulletDamage = 25;
        float bulletHitRadiusCells = 0.35f;

        float grenadeRadiusCells = 2.5f;
        int   grenadeDmgCenter = 50;
        int   grenadeDmgEdge = 10;
        float grenadeCoverBlockFactor = 0.35f;
        bool  grenadeRaycastCover = true; 

        bool  friendlyFire = false;      

        float throwHorizSpeed = 0.40f;   
        int   throwMinFrames = 10;       
        int   throwMaxFrames = 90;       

        std::vector<Bullet>  bullets;
        std::vector<Grenade> grenades;   

        void bindUnits(std::vector<Models::Unit*>* unitsRef) { units = unitsRef; }
        void fireBulletTowards(float r0, float c0, float rT, float cT,
            Definitions::Team shooterTeam = Definitions::Team::Blue);

        void dropGrenade(float r0, float c0,
            const Models::Grid& grid,
            Definitions::Team shooterTeam = Definitions::Team::Blue);

        void throwGrenadeParabola(float r0, float c0, float rT, float cT,
            Definitions::Team shooterTeam = Definitions::Team::Blue);

        void tickBullets(const Models::Grid& grid, Simulation::SecurityMap& smap);

        void draw() const;

        size_t bulletsCount() const { return bullets.size(); }

    private:
        void explode(float r0, float c0, Definitions::Team shooterTeam);
        void applyBulletHitUnits(Bullet& b);

        void applyGrenadeAoE(const Models::Grid& grid,
            float r0, float c0,
            Definitions::Team shooterTeam);

        bool stepGrenade(Grenade& g, const Models::Grid& grid);

        std::vector<Models::Unit*>* units = nullptr;
    };

} // namespace Combat
