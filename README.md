Tactical AI Simulation (C++ / OpenGL & GLUT)

A grid-based real-time tactics sandbox featuring autonomous units, a commander AI that issues orders, simple ballistics (bullets & grenades), line-of-sight visibility, and a “security map” risk field—rendered with OpenGL via FreeGLUT.

On launch you’ll see “PRESS K TO START THE MATCH”; toggle the commander AI with K. 

main

✨ Features

120×120 world grid with cell-sized rendering and fixed-timestep simulation. 

Definitions

Unit roles: Commander, Warrior, Medic, Supplier—each with role-specific stats/behaviors and HUD role letters. 

Definitions

 

Units

Commander AI that monitors injured/low-ammo units and assigns Medics/Suppliers accordingly, with lock/cooldowns to avoid thrashing. 

Commander

 

Commander

 

Commander

Autonomy fallback: If a team’s commander dies, its Warriors switch to autonomous behavior; if Warriors die, the Commander joins the fight. 

main

Event bus for decoupled AI messaging (e.g., “CommanderDown”, “LowAmmo”, “Injured”). 

EventBus

Line-of-sight visibility and team visibility builders (Bresenham-style ray stepping). 

Visibility

 

Visibility

Security Map risk field cast from map borders/lanes to influence tactics. 

SecurityMap

 

SecurityMap

Ballistics & effects: bullets as points; grenades with shadow + glow sprites drawn in overlay. 

Combat

 

Combat

Heads-up display and role badges for teams; overlay rendering paths (security/visibility/none). 

Renderer

 

main

🧠 How it works (quick tour)

Main loop & GLUT callbacks: display, idle, mouse, keyboard, reshape are registered in main() using FreeGLUT. 

main

Rendering: Painting::Render* draws the grid, units, and optional overlays (HUD/Security/Visibility). 

Renderer

Commander AI: Ticks every frame when enabled, scans status lists, and issues state transitions (e.g., State_Healing, State_Supplying, State_RefillAtDepot). 

Commander

 

Commander

Autonomous supports: Event-driven behaviors for Medics/Suppliers when not under commander control. 

Units

Game flow: Victory when one side loses all Warriors; frame counter/HUD printed periodically. 

main

 

main

🎮 Controls

K — Toggle Commander AI (shows “ENABLED/OFF” in console). 

main

Left mouse — Set a target cell (prints “Target set to (r,c)”). 

main

Right mouse — Toggle Security Map overlay (auto-hides Visibility overlay). 

main

When game over: N — new world; E — exit. 

main

Test keys:

X — kill Blue commander and auto-enable Blue Warriors. 

main

O — kill Orange commander and auto-enable Orange Warriors. 

main

🧱 World & balance knobs

A few notable constants (see Definitions.h):

Grid/cell/time: GRID_SIZE=120, CELL_PX=8, TICK_MS=16 

Definitions

Ranges: SIGHT_RANGE=140, FIRE_RANGE=24, GRENADE_RANGE=18 

Definitions

Damage/ammos/heal/supply thresholds for each role are centralized here. 

Definitions

🛠️ Build & Run

Toolchain: Windows + Visual Studio (project files included), C++17+, OpenGL
Dependencies: FreeGLUT and GLEW headers/libs are provided in the repo (freeglut_*.h/.lib, glew.h, glew32*.lib). 

freeglut

 

glew

Open the Visual Studio solution/project (.vcxproj included) and make sure the headers in this repo are on your include path (glut.h, freeglut_*.h, glew.h). 

glut

Link the provided libraries (freeglut.lib, glew32.lib or glew32s.lib depending on static/dynamic config). 

glew

Build > Run. The app creates a window sized to the world (GRID_SIZE * CELL_PX) and registers GLUT callbacks. 

main

Note: If you swap to your system’s FreeGLUT/GLEW, ensure matching CRT settings on Windows to avoid atexit/CRT mismatch issues. 

freeglut_std

📁 Project layout (major components)

main.cpp — App entry, GLUT setup, world builder, input handling, per-frame simulation. 

main

Renderer.{h,cpp} — Grid, units, overlays (HUD/Security/Visibility) drawing. 

Renderer

Combat.{h,cpp} — Bullets/grenades simulation and overlay rendering. 

Combat

Visibility.{h,cpp} — LOS queries and team visibility accumulation. 

Visibility

SecurityMap.{h,cpp} — Risk field generation. 

SecurityMap

Commander.{h,cpp} — Centralized AI issuing orders to supports/warriors. 

Commander

Units.{h,cpp}, Warrior.{h,cpp}, Medic.h, Supplier.h — Unit model & role logic; autonomy hooks. 

Units

State* — Finite State Machine states (moving, attacking, healing, retreating, resupplying, etc.).

EventBus.h, AIEvents.h — Pub/sub for gameplay events. 

EventBus

Definitions.h, Globals.h — Tunables, enums, colors, and shared constants. 

Definitions

🚀 Getting started (quick play)

Run the app—press K to let Commanders start issuing orders. 

main

Left-click to set a target cell for context in the overlay. 

main

Right-click to toggle the Security Map; Visibility overlay auto-disables when Security is on. 

main

Use X/O to simulate a commander being down and watch contingencies/autonomy kick in. 

main

📜 Credits

FreeGLUT for windowing/callbacks. 

glut

GLEW for OpenGL extensions on Windows.
