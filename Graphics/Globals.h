#pragma once
#include "Grid.h"
#include "SecurityMap.h"
#include "Units.h"
#include <vector>
#include "Combat.h"

extern Models::Grid g_grid;
extern std::vector<Models::Unit*> g_units;
extern Simulation::SecurityMap g_smap;
extern Combat::System g_combat;