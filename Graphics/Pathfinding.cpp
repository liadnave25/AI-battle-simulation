#include "Pathfinding.h"
#include <queue>
#include <cmath>
#include <algorithm>
#include <limits>
#include "Visibility.h"
#include "Definitions.h"
#include "Globals.h"
#include "Units.h"
#include <cstdio>

using namespace Definitions;

namespace AI {
    namespace Pathfinding {

        using Cell = std::pair<int, int>;
        using Path = std::vector<Cell>;

        float RiskWeightForUnit(const Models::Unit* u) {
            if (!u) return Definitions::ASTAR_RISK_WEIGHT;
            const float hp = u->hpNorm();                     
            const float hpFactor = 1.0f + (1.0f - hp) * 0.75f;
            const float carrying = u->isCarryingObjective ? 1.25f : 1.0f;
            return Definitions::ASTAR_RISK_WEIGHT * hpFactor * carrying;
        }

        float effectiveRiskWeight(const Models::Unit& u) noexcept {
            const float hpNorm = u.hpNorm();
            const float hpFactor = 1.0f + (1.0f - hpNorm) * 0.75f;
            const float carrying = u.isCarryingObjective ? 1.25f : 1.0f;
            return Definitions::ASTAR_RISK_WEIGHT * hpFactor * carrying;
        }

        bool inPlayfield(int r, int c) {
            return (r >= PLAY_TOP) &&
                (r <= GRID_SIZE - 1 - PLAY_BOTTOM) &&
                (c >= PLAY_LEFT) &&
                (c <= GRID_SIZE - 1 - PLAY_RIGHT);
        }

        static inline bool blocksExplosive(int cell) {
            return (cell == TREE || cell == ROCK || cell == DEPOT_AMMO || cell == DEPOT_MED);
        }

        bool raycastBlocked(const Models::Grid& grid, int r0, int c0, int r1, int c1)
        {
            int dr = std::abs(r1 - r0), dc = std::abs(c1 - c0);
            int sr = (r0 < r1) ? 1 : -1;
            int sc = (c0 < c1) ? 1 : -1;
            int err = dr - dc;
            int r = r0, c = c0;

            while (!(r == r1 && c == c1)) {
                if (r < 0 || r >= GRID_SIZE || c < 0 || c >= GRID_SIZE) break;
                if (blocksExplosive(g_grid.at(r, c))) return true;
                int e2 = 2 * err;
                if (e2 > -dc) { err -= dc; r += sr; }
                if (e2 < dr) { err += dr; c += sc; }
            }
            return false;
        }

        bool PickBestDefendCell(int goalR, int goalC, int radiusCells,
            int selfR, int selfC,
            int& outR, int& outC)
        {
            const int R = std::max(1, radiusCells);
            float bestScore = 1e9f;
            int bestR = -1, bestC = -1;

            auto consider = [&](int r, int c) {
                if (r < 0 || r >= GRID_SIZE || c < 0 || c >= GRID_SIZE) return;
                if (!inPlayfield(r, c)) return;
                if (!AI::Pathfinding::IsWalkableForMovement(g_grid.at(r, c))) return;
                if (IsOccupied(r, c)) return;

                float risk = g_smap.at(r, c);
                bool covered = raycastBlocked(g_grid, goalR, goalC, r, c);
                float coverPenalty = covered ? -0.25f : +0.0f;
                float distSelf = float(std::abs(r - selfR) + std::abs(c - selfC)) * 0.01f;
                float score = risk + coverPenalty + distSelf;
                if (score < bestScore) { bestScore = score; bestR = r; bestC = c; }
                };

            for (int dr = -R; dr <= R; ++dr)
                for (int dc = -R; dc <= R; ++dc) {
                    if (dr * dr + dc * dc > R * R) continue;
                    consider(goalR + dr, goalC + dc);
                }

            if (bestR < 0) return false;
            outR = bestR; outC = bestC;
            return true;
        }

        bool IsOccupiedByOther(int r, int c, int pathingUnitId)
        {
            for (const auto* u : g_units) {
                if (!u->isAlive) continue;
                if (u->id == pathingUnitId) continue; 
                if (u->row == r && u->col == c) return true;
            }
            return false;
        }

        bool IsOccupied(int r, int c)
        {
            for (const auto* u : g_units) {
                if (!u->isAlive) continue;
                if (u->row == r && u->col == c) return true;
            }
            return false;
        }

        static inline bool inBounds(int r, int c) {
            return (r >= 0 && r < GRID_SIZE && c >= 0 && c < GRID_SIZE);
        }

        bool IsWalkableForMovement(int cell) {
            if (cell == ROCK || cell == WATER) return false;
            return true;
        }

        static Path Reconstruct(std::vector<std::vector<Cell>>& parent, Cell start, Cell goal) {
            Path path;
            if (!inBounds(goal.first, goal.second)) {
                return path;
            }
            if (parent[goal.first][goal.second].first == -1 && !(goal == start)) {
                return path;
            }

            Cell cur = goal;
            while (!(cur == start)) {
                path.push_back(cur);
                if (!inBounds(cur.first, cur.second)) {
                    path.clear();
                    break;
                }
                cur = parent[cur.first][cur.second];
                if (cur.first < 0 || cur.second < 0) {
                    path.clear();
                    break;
                }
            }

            if ((!path.empty() || goal == start) && cur == start) {
                path.push_back(start);
                std::reverse(path.begin(), path.end());
            }
            else if (!path.empty()) {
                path.clear();
            }

            return path;
        }

        Path BFS_FindPath(const Models::Grid& grid, Cell start, Cell goal) {
            Path empty;
            if (!inBounds(start.first, start.second) || !inBounds(goal.first, goal.second)) return empty;
            if (!IsWalkableForMovement(grid.at(goal.first, goal.second))) return empty;

            std::vector<std::vector<bool>> vis(GRID_SIZE, std::vector<bool>(GRID_SIZE, false));
            std::vector<std::vector<Cell>> parent(GRID_SIZE, std::vector<Cell>(GRID_SIZE, { -1,-1 }));

            std::queue<Cell> q;
            q.push(start);
            vis[start.first][start.second] = true;

            const int dr[4] = { +1,-1,0,0 };
            const int dc[4] = { 0,0,+1,-1 };

            while (!q.empty()) {
                Cell u = q.front(); q.pop();
                if (u == goal) break;

                for (int k = 0; k < 4; ++k) {
                    int nr = u.first + dr[k];
                    int nc = u.second + dc[k];
                    if (!inBounds(nr, nc) || vis[nr][nc]) continue;
                    if (!IsWalkableForMovement(grid.at(nr, nc))) continue;
                    vis[nr][nc] = true;
                    parent[nr][nc] = u;
                    q.push({ nr,nc });
                }
            }
            return Reconstruct(parent, start, goal);
        }

        struct Node {
            int r = 0, c = 0;
            float f = 0, g = 0, h = 0;
        };
        struct NodeCmp {
            bool operator()(const Node& a, const Node& b) const { return a.f > b.f; }
        };

        static inline float Heuristic(Cell a, Cell b) {
            return (float)(std::abs(a.first - b.first) + std::abs(a.second - b.second));
        }

        Path AStar_FindPath(const Models::Unit* pathingUnit,
            const Models::Grid& grid,
            const Simulation::SecurityMap& smap,
            Cell start, Cell goal,
            float riskWeight) {
            Path empty;
            if (!pathingUnit) {
                printf("ERROR: A* called with null pathingUnit!\n");
                return empty;
            }

            if (!inBounds(start.first, start.second) || !inBounds(goal.first, goal.second)) return empty;
            if (!IsWalkableForMovement(grid.at(goal.first, goal.second))) return empty;

            const float maxV = std::max(0.0001f, smap.maxValue());
            const int dr[4] = { +1,-1,0,0 };
            const int dc[4] = { 0,0,+1,-1 };

            constexpr float OCCUPANCY_PENALTY = 25.0f;

            std::vector<std::vector<float>> gScore(GRID_SIZE, std::vector<float>(GRID_SIZE, std::numeric_limits<float>::infinity()));
            std::vector<std::vector<Cell>> parent(GRID_SIZE, std::vector<Cell>(GRID_SIZE, { -1,-1 }));
            std::vector<std::vector<bool>> closed(GRID_SIZE, std::vector<bool>(GRID_SIZE, false));

            auto riskNorm = [&](int r, int c)->float {
                if (!inBounds(r, c)) return 1.0f;
                float v = smap.at(r, c);
                return (v <= 0.0f ? 0.0f : std::min(1.0f, v / maxV));
                };

            std::priority_queue<Node, std::vector<Node>, NodeCmp> open;

            gScore[start.first][start.second] = 0.0f;
            Node s; s.r = start.first; s.c = start.second; s.g = 0.0f; s.h = Heuristic(start, goal); s.f = s.g + s.h;
            open.push(s);

            while (!open.empty()) {
                Node cur = open.top(); open.pop();

                if (!inBounds(cur.r, cur.c) || closed[cur.r][cur.c]) {
                    continue;
                }
                closed[cur.r][cur.c] = true;

                if (cur.r == goal.first && cur.c == goal.second) break;

                for (int k = 0; k < 4; ++k) {
                    int nr = cur.r + dr[k];
                    int nc = cur.c + dc[k];

                    if (!inBounds(nr, nc) || closed[nr][nc]) continue;
                    if (!IsWalkableForMovement(grid.at(nr, nc))) continue;

                    float step = 1.0f + riskWeight * riskNorm(nr, nc);

                    if (IsOccupiedByOther(nr, nc, pathingUnit->id)) {
                        step += OCCUPANCY_PENALTY;
                    }

                    float tentative = gScore[cur.r][cur.c] + step;

                    if (tentative < gScore[nr][nc]) {
                        gScore[nr][nc] = tentative;
                        parent[nr][nc] = { cur.r, cur.c };
                        Node nxt;
                        nxt.r = nr; nxt.c = nc;
                        nxt.g = tentative;
                        nxt.h = Heuristic({ nr,nc }, goal);
                        nxt.f = nxt.g + nxt.h;
                        open.push(nxt);
                    }
                }
            }
            return Reconstruct(parent, start, goal);
        }

        Cell PickVantagePoint(const Models::Grid& grid,
            const Simulation::SecurityMap& smap,
            Cell agent, Cell target,
            int attackRangeCells, float distWeight, int searchRadius)
        {
            const int tr = target.first, tc = target.second;
            const int ar = agent.first, ac = agent.second;
            const float maxRisk = std::max(0.0001f, smap.maxValue());
            float bestScore = std::numeric_limits<float>::infinity();
            Cell bestCell = { -1, -1 };

            const int r0 = std::max(0, tr - searchRadius);
            const int r1 = std::min(GRID_SIZE - 1, tr + searchRadius);
            const int c0 = std::max(0, tc - searchRadius);
            const int c1 = std::min(GRID_SIZE - 1, tc + searchRadius);

            for (int r = r0; r <= r1; ++r) {
                for (int c = c0; c <= c1; ++c) {
                    if (!inBounds(r, c) || !IsWalkableForMovement(grid.at(r, c))) continue;

                    int distToTarget = std::abs(r - tr) + std::abs(c - tc);
                    if (distToTarget > attackRangeCells) continue;
                    if (!AI::Visibility::HasLineOfSight(grid, r, c, tr, tc)) continue;

                    float riskNorm = smap.at(r, c) / maxRisk;
                    int distFromAgent = std::abs(r - ar) + std::abs(c - ac);
                    float score = riskNorm + distWeight * float(distFromAgent);

                    if (score < bestScore) {
                        bestScore = score;
                        bestCell = { r, c };
                    }
                }
            }
            return bestCell;
        }

        Cell FindLocalCoverStep(const Models::Grid& grid,
            const Simulation::SecurityMap& smap,
            Cell from, int radius, float riskDeltaMin)
        {
            const int r0 = from.first, c0 = from.second;
            if (!inBounds(r0, c0)) return { -1,-1 };

            const float maxV = std::max(0.0001f, smap.maxValue());
            auto riskN = [&](int r, int c)->float {
                float v = smap.at(r, c);
                return (v <= 0.f ? 0.f : std::min(1.f, v / maxV));
                };

            const float here = riskN(r0, c0);
            const int dr4[4] = { +1,-1,0,0 };
            const int dc4[4] = { 0,0,+1,-1 };
            Cell best = { -1,-1 };
            float bestDrop = 0.f;

            for (int k = 0; k < 4; ++k) {
                int r = r0 + dr4[k], c = c0 + dc4[k];
                if (!inBounds(r, c) || !IsWalkableForMovement(grid.at(r, c))) continue;
                float rn = riskN(r, c);
                float drop = here - rn;
                if (drop >= riskDeltaMin && drop > bestDrop) {
                    bestDrop = drop;
                    best = { r,c };
                }
            }
            if (best.first != -1) return best;

            int rmin = std::max(0, r0 - radius), rmax = std::min(GRID_SIZE - 1, r0 + radius);
            int cmin = std::max(0, c0 - radius), cmax = std::min(GRID_SIZE - 1, c0 + radius);
            Cell target = { -1,-1 };
            float targetDrop = 0.f;

            for (int r = rmin; r <= rmax; ++r) for (int c = cmin; c <= cmax; ++c) {
                if (!IsWalkableForMovement(grid.at(r, c))) continue;
                float rn = riskN(r, c);
                float drop = here - rn;
                if (drop >= riskDeltaMin && drop > targetDrop) {
                    targetDrop = drop; target = { r,c };
                }
            }

            if (target.first == -1) return { -1,-1 };

            Cell step = { -1,-1 };
            int bestD = std::abs(r0 - target.first) + std::abs(c0 - target.second);
            for (int k = 0; k < 4; ++k) {
                int r = r0 + dr4[k], c = c0 + dc4[k];
                if (!inBounds(r, c) || !IsWalkableForMovement(grid.at(r, c))) continue;
                int d = std::abs(r - target.first) + std::abs(c - target.second);
                if (d < bestD) { bestD = d; step = { r,c }; }
            }
            return step;
        }

        float PathRiskSample(const Simulation::SecurityMap& smap,
            const Path& path, int sampleLen, bool useMax)
        {
            if (path.empty() || sampleLen <= 0) return 0.f;
            const float maxV = std::max(0.0001f, smap.maxValue());
            int n = 0;
            const int upto = std::min<int>(sampleLen, (int)path.size());

            if (useMax) {
                float mx = 0.f;
                for (int i = 0; i < upto; ++i) {
                    float v = smap.at(path[i].first, path[i].second);
                    float rn = (v <= 0.f ? 0.f : std::min(1.f, v / maxV));
                    if (rn > mx) mx = rn;
                }
                return mx;
            }
            else {
                float agg = 0.f;
                for (int i = 0; i < upto; ++i) {
                    float v = smap.at(path[i].first, path[i].second);
                    float rn = (v <= 0.f ? 0.f : std::min(1.f, v / maxV));
                    agg += rn; ++n;
                }
                return (n > 0 ? agg / float(n) : 0.f);
            }
        }

    } // namespace Pathfinding
} // namespace AI