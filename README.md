# Tactical AI Simulation (C++ / OpenGL & FreeGLUT)

A grid‑based real‑time tactics sandbox written in modern C++. Two teams (**Blue** vs **Orange**) fight over a 2D world. Each team fields a **Commander**, **Warriors**, **Medic**, and **Supplier**. The simulation features line‑of‑sight, a risk field (“Security Map”), simple ballistics (bullets & grenades), an event bus for AI signals, and a finite‑state machine for per‑unit behavior. Rendering is done with OpenGL via **FreeGLUT** and **GLEW**.

> On launch you’ll see a debug HUD with FPS, grid info, and counts. Toggle the commander AI with **K** to start the match.

---

## ✨ Features

- **120×120 world grid** with cell‑sized rendering and fixed timestep update.
- **Unit roles** with distinct logic and stats: Commander, Warrior, Medic, Supplier.
- **Commander AI**: scans the battlefield, assigns orders (heal/supply/move/engage) and prevents thrashing with locks/cooldowns.
- **Autonomy fallback**: If a commander is down, warriors continue fighting under local logic.
- **Event Bus**: decoupled AI messaging (e.g., CommanderDown, EnemySighted, LowAmmo, Injured).
- **Line‑of‑Sight & Visibility**: per‑unit LOS and team visibility accumulation (ray‑stepping).
- **Security Map**: a risk field that influences decisions and can be visualized as an overlay.
- **Ballistics & Effects**: bullets as points; grenades with overlay (shadow/glow) and area damage.
- **Debug overlays & HUD**: toggle visibility/security/none; role letters and team coloring.

---

## 🎮 Controls

- **K** — Toggle Commander AI (ENABLED/OFF).
- **Left Click** — Set/mark a target cell for context.
- **Right Click** — Toggle **Security Map** overlay (visibility overlay auto‑hides when security is on).
- **X** — Simulate Blue commander down (watch autonomy kick in).
- **O** — Simulate Orange commander down.
- **N** — New world when the match is over.
- **E** — Exit.

---

## 🧱 Notable Tunables (see `Definitions.h`)

- Grid/cell/time: `GRID_SIZE = 120`, `CELL_PX = 8`, `TICK_MS = 16`
- Ranges: `SIGHT_RANGE`, `FIRE_RANGE`, `GRENADE_RANGE`
- Role stats: HP, damage, ammo counts, heal/supply thresholds
- Colors & HUD flags

These constants provide quick balance knobs without digging through gameplay code.

---

## 📁 Project Structure (high‑level)

- `main.cpp` — App entry, GLUT setup, world builder, input handling, per‑frame simulation.
- `Renderer.{h,cpp}` — Grid, units, HUD, and overlays (Security/Visibility).
- `Combat.{h,cpp}` — Bullets/grenades simulation and overlay rendering.
- `Visibility.{h,cpp}` — LOS queries & team visibility aggregation.
- `SecurityMap.{h,cpp}` — Risk field generation and utilities.
- `Commander.{h,cpp}` — Central brain that issues orders to supports/warriors.
- `Units.{h,cpp}`, `Warrior.{h,cpp}`, `Medic.h`, `Supplier.h` — Unit model & role logic.
- `State*` — FSM states (Idle, MovingToTarget, Attacking, Defending, Healing, WaitingForMedic/Support, RetreatingToCover, Supplying, RefillAtDepot, etc.).
- `EventBus.h`, `AIEvents.h` — Lightweight pub/sub for gameplay events.
- `Definitions.h`, `Globals.h` — Enums, tunables, and shared constants.

---

## 🛠️ Build & Run (Windows + Visual Studio)

**Requirements**
- Visual Studio 2019/2022 (C++17 or later)
- OpenGL, **FreeGLUT**, **GLEW**
- This repository already includes the needed headers/libs:  
  `glut.h`, `freeglut.h` (+ `freeglut_ext.h`, `freeglut_std.h`), `glew.h`, and libraries `freeglut.lib`, `glew32.lib`/`glew32s.lib`.

**Steps**
1. Open the provided `.vcxproj` in Visual Studio.
2. Ensure **Additional Include Directories** include the repo path with `glut.h`, `freeglut_*.h`, `glew.h`.
3. Link against the libraries:  
   - Dynamic GLEW: `glew32.lib` (and ship `glew32.dll`), or  
   - Static GLEW: `glew32s.lib` (define `GLEW_STATIC` in preprocessor).  
   Also link `freeglut.lib` and the usual Windows OpenGL libs (`opengl32.lib`, optionally `glu32.lib` if used).
4. Build & Run. The window size defaults to `GRID_SIZE * CELL_PX`. Use the controls above to play and to toggle overlays.

**Notes**
- If you swap to system‑installed FreeGLUT/GLEW, ensure CRT settings match your build (avoid mixed static/dynamic CRT issues).
- If you see a black window, verify that a valid OpenGL context is created and that GLEW is initialized **after** the context is current.

---

## 🚀 Quick Start (Gameplay)

1. Run the app.
2. Press **K** to let Commanders start issuing orders.
3. Use **Left Click** to set a contextual target, **Right Click** to visualize the **Security Map**.
4. Use **X/O** to simulate “commander down” scenarios and observe autonomy/contingency behaviors.

---

## 🧪 Development Tips

- Keep balance changes inside `Definitions.h` to iterate quickly.
- Toggle overlays to visually debug pathing, LOS, and risk exposure.
- Use the state classes (`State_*`) to add or tweak behaviors in isolation.

---

## 📜 License

Choose a license (MIT, Apache‑2.0, BSD‑2‑Clause). Open an issue or PR and I’ll add it to the repo.

---

## 🙌 Credits

- **FreeGLUT** — windowing & input callbacks
- **GLEW** — OpenGL extensions on Windows
