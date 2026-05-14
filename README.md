# Scorched Earth

A two-player artillery game. Player 1 (human) vs Player 2 (AI). Adjust your angle and power, fire a projectile, blow up the other tank. First to destroy the enemy wins.

---

## How to build and run

Open in CLion, select the **se** target, and hit Run. That's it.

---

## What is an ECS?

ECS stands for **Entity Component System**. It's a way of structuring game objects that's common in game development.

Instead of having a `Tank` class with health, position, and AI all bundled together, you split everything into three separate ideas:

### Entity
Just an ID number. Nothing more. It's a handle that says "thing #42 exists."

### Component
Pure data attached to an entity. No logic, no methods — just fields.

```
Position  { x, y }
Health    { current, max }
Artillery { angle, power }
```

A tank entity might have: Position + Health + Artillery + Drawing + Physics + AI.
A projectile entity might have: Position + Movement + Physics + Weapon + Lifetime.

### System
A function that loops over all entities that have a specific set of components and does something with them.

```
physics_system()   — finds everything with Position + Movement + Physics, applies gravity
collision_system() — finds everything with Position + Movement + Weapon, checks for hits
render_system()    — finds everything with Position + Drawing, draws them on screen
```

The key insight: systems don't care what "type" an entity is. The physics system will move anything that has Position + Movement, whether it's a projectile, a tank, or anything else. You build behavior by combining components.

---

## The engine: bagel26

`bagel.h` is the ECS engine this game runs on. It's a single header file (~600 lines of C++20).

**Never modify bagel.h.** The game adapts to the engine, not the other way around.

### Important rule: all components must be plain data

bagel stores components in raw memory (`malloc`/`realloc`). This means every component must be trivially copyable — it can only contain simple types like `float`, `int`, `bool`, and plain C arrays. You cannot put `std::vector` or `std::string` inside a component; it will crash.

### How you use bagel in code

**Creating an entity with components:**
```cpp
auto e = Entity::create();
e.add<Position>({100.f, 458.f});
e.add<Health>({100.f, 100.f});
```

**Iterating over entities in a system:**
```cpp
static const Mask mask = MaskBuilder().set<Position>().set<Movement>().build();
for (Entity e = Entity::first(); !e.eof(); e.next()) {
    if (!e.test(mask)) continue;
    auto& pos = e.get<Position>();
    auto& mov = e.get<Movement>();
    pos.x += mov.vx * DT;
    pos.y += mov.vy * DT;
}
```

The `Mask` is a bitmask that says "I want entities that have at least these components." You build it once (static) and reuse it every frame.

**Accessing a specific entity by handle:**
```cpp
Entity tank{tanks[0]};          // tanks[0] is a saved ent_type handle
auto& hp = tank.get<Health>();
hp.current -= 50.f;
```

---

## Project structure

```
scorched-earth/
├── bagel.h                  — the ECS engine (do not touch)
├── main.cpp                 — entry point, creates ScorchedEarth and calls run()
├── SE/
│   ├── ScorchedEarth.h      — component definitions, class declaration
│   └── ScorchedEarth.cpp    — all systems, factories, game loop
├── res/
│   └── SE-spreadsheet.png   — sprite sheet (tanks, barrels, explosions)
└── lib/
    ├── SDL/                 — graphics/input library
    └── SDL_image/           — image loading
```

---

## Components

Defined in `ScorchedEarth.h`:

| Component | Fields | Used by |
|-----------|--------|---------|
| `Position` | x, y | everything |
| `Movement` | vx, vy | projectiles |
| `Drawing` | part (sprite rect), size | tanks, projectiles, explosions |
| `Artillery` | angle (0–180°), power | tanks |
| `Physics` | gravity, windResistance | tanks, projectiles |
| `Health` | current, max | tanks |
| `AI` | state, difficulty | AI tank |
| `Weapon` | explosionRadius, damage | projectiles |
| `Lifetime` | remaining, duration | explosions |
| `GridData` | cells[50][8] (bool grid) | terrain entity |
| `Input` | (empty tag) | player tank |
| `Collided` | (empty tag) | projectiles that hit something |
| `AreaDamage` | radius, damage | explosion entities |
| `Currency` | amount | (reserved for shop) |
| `Inventory` | weaponType, count | (reserved for shop) |

Tags (`Input`, `Collided`) are empty structs — they carry no data, just mark that something is true about an entity.

---

## Entities in the game

| Entity | Components |
|--------|-----------|
| Player tank | Position, Drawing, Artillery, Physics, Health, Input |
| AI tank | Position, Drawing, Artillery, Physics, Health, AI |
| Projectile | Position, Movement, Drawing, Physics, Weapon |
| Explosion | Position, Drawing, Lifetime, AreaDamage |
| Terrain | Position, GridData |

---

## Systems (run every frame)

### `input_system()`
Reads keyboard state. Finds the entity with `Input` tag (always P1).
- Left/Right arrows → adjust angle
- Up/Down arrows → adjust power
- Space → fire a projectile from the barrel tip (leading-edge debounce so holding space doesn't spam)

Only runs on the human player's turn.

### `ai_system()`
Three-state machine for the AI (P2):
1. **Idle (state=0):** starts a think delay (~40 frames)
2. **Think (state=1):** counts down, then computes the aim angle using the ballistic formula (`sin(2θ) = |dx|·g / v²`), adds random scatter (±15°) and random power variation (±30)
3. **Sweep (state=2):** moves the barrel 4°/frame toward the target angle, fires when it arrives

### `physics_system()`
Finds everything with `Position + Movement + Physics`. Applies gravity to `vy`, then integrates velocity into position. Also kills projectiles that fly off-screen.

### `collision_system()`
Finds projectiles (entities with `Position + Movement + Weapon`). For each:
1. Checks if it hit a terrain cell (converts world position to grid column/row)
2. Checks if it hit either tank (AABB rectangle test)

If either hits: tags the projectile with `Collided`. Never destroys during iteration.

### `damage_system()`
Finds `Collided` projectiles. For each:
1. Creates an explosion entity at the impact point
2. Clears terrain cells within the explosion radius
3. Checks both tanks — if within radius, reduces health
4. If a tank reaches 0 health: sets `gameOver = true`, records the `winner`
5. Switches turn (`activePlayer ^= 1`)
6. Destroys the projectile (deferred — collected first, destroyed after iteration)

### `lifetime_system()`
Finds entities with `Lifetime`. Decrements `remaining` each frame. When it reaches 0, destroys the entity. Used for explosions so they disappear after a short time.

### `render_system()`
Draws everything. Runs every frame even when `gameOver` is true (so the win screen stays visible).
- Terrain: brown rectangles for each `true` cell in the grid
- Tanks: sprite from the sheet, flipped horizontally for P2
- Barrels: rotated rectangle, pivot at the tank's left-center
- Projectiles: small sprite
- Explosions: sprite
- HUD: health bars, angle/power readout, active player indicator
- Win overlay: "Player N wins!" text in the center

---

## Game loop (`run()`)

```
while not quit:
    handle SDL events (Escape = quit)
    if not gameOver:
        input_system()
        ai_system()
        physics_system()
        collision_system()
        damage_system()
        lifetime_system()
    render_system()      ← always runs
    wait for next frame (60 fps cap)
```

---

## The terrain grid

The terrain is a single entity with a `GridData` component containing `bool cells[50][8]`.

- 50 columns × 8 rows of 16×16 pixel cells
- Top of terrain starts at y=472 (600 − 128)
- `true` = solid ground, `false` = empty (blown up)

When an explosion happens, `damage_system` loops over the 50×8 grid and sets cells to `false` if their centre is within the explosion radius. `render_system` draws a brown rectangle for every `true` cell.

---

## Key numbers you can tweak

All in `ScorchedEarth.cpp`:

| What | Where | Current value |
|------|-------|---------------|
| Default power | `createTank()` → `Artillery{angle, 300.f}` | 300 |
| Projectile gravity | `createProjectile()` → `Physics{40.f, ...}` | 40 |
| Explosion radius | `createProjectile()` → `Weapon{40.f, 50.f}` | 40 px |
| Explosion damage | `createProjectile()` → `Weapon{40.f, 50.f}` | 50 hp |
| Tank health | `createTank()` → `Health{100.f, 100.f}` | 100 hp |
| AI scatter | `ai_system()` → `rand() % 31 - 15` | ±15° |
| AI power scatter | `ai_system()` → `rand() % 61 - 30` | ±30 |
| Angle adjust speed | `input_system()` → `art.angle += 2.f` | 2°/frame |
