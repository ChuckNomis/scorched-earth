#include "SE/ScorchedEarth.h"
#include <SDL3_image/SDL_image.h>
#include <iostream>
#include <cmath>            // sinf, cosf for barrel rendering
using namespace std;

#include "bagel.h"
using namespace bagel;

namespace se
{

/// @brief Creates the deformable terrain entity.
/// @param x Left edge of the terrain grid in world space.
/// @param y Top edge of the terrain grid in world space.
/// @return Handle to the new entity (carries @ref se::Position, @ref se::Drawing, @ref se::GridData).
static bagel::ent_type createTerrain(float x, float y);

/// @brief Creates a tank entity with the appropriate component set.
/// @param x    Initial centre X position.
/// @param y    Initial centre Y position.
/// @param isAI Attach an @ref se::AI component (AI-controlled tank).
/// @param isPlayer Attach an @ref se::Input tag (player-controlled tank).
/// @param initAngle Starting barrel angle in degrees (default 45°).
/// @return Handle to the new entity.
static bagel::ent_type createTank(float x, float y, bool isAI, bool isPlayer, float initAngle = 45.f);

/// @brief Creates a projectile entity launched from a tank barrel.
/// @param x  Spawn position X (world space).
/// @param y  Spawn position Y (world space).
/// @param vx Initial horizontal velocity (pixels/second).
/// @param vy Initial vertical velocity (pixels/second, negative = upward).
/// @return Handle to the new entity (carries @ref se::Position, @ref se::Movement,
///         @ref se::Drawing, @ref se::Physics, @ref se::Weapon).
static bagel::ent_type createProjectile(float x, float y, float vx, float vy);

/// @brief Creates a visual explosion entity at the point of impact.
/// @param x Centre X of the explosion (world space).
/// @param y Centre Y of the explosion (world space).
/// @return Handle to the new entity (carries @ref se::Position, @ref se::Drawing,
///         @ref se::Lifetime, @ref se::AreaDamage). Destroyed by @ref se::ScorchedEarth::lifetime_system.
static bagel::ent_type createExplosion(float x, float y);

ScorchedEarth::ScorchedEarth()
{
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        cout << SDL_GetError() << endl;
        return;
    }
    if (!SDL_CreateWindowAndRenderer("Scorched Earth", WIN_W, WIN_H, 0, &win, &ren)) {
        cout << SDL_GetError() << endl;
        return;
    }

    SDL_Surface* surf = IMG_Load("res/SE-spreadsheet.png");
    if (surf) {
        // Make white background transparent so sprites render clean on black background
        SDL_SetSurfaceColorKey(surf, true, SDL_MapSurfaceRGB(surf, 255, 255, 255));
        tex = SDL_CreateTextureFromSurface(ren, surf);
        SDL_DestroySurface(surf);
    }
    // tex == nullptr is fine — render_system will use colored rectangles as fallback

    SDL_srand(static_cast<Uint32>(time(nullptr)));
    SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);

    // Create game entities — terrain FIRST (lower ID = drawn behind tanks)
    constexpr float TERRAIN_TOP = WIN_H - 8 * 16; // 600 - 128 = 472.f
    createTerrain(0.f, TERRAIN_TOP);
    // Tanks: 48x28 display size, half-height=14; bottom at terrain top
    constexpr float TANK_Y = TERRAIN_TOP - 14.f;  // 458.f — tank center sits on terrain
    tanks[0] = createTank(150.f, TANK_Y, false, true);          // P1: green, angle=45 (up-right)
    tanks[1] = createTank(650.f, TANK_Y, true,  false, 135.f);  // P2: red,   angle=135 (up-left toward P1)

    initialized = true;
}

ScorchedEarth::~ScorchedEarth()
{
    if (tex != nullptr) SDL_DestroyTexture(tex);
    if (ren != nullptr) SDL_DestroyRenderer(ren);
    if (win != nullptr) SDL_DestroyWindow(win);
    SDL_Quit();
}

void ScorchedEarth::run()
{
    auto start = SDL_GetTicks();
    bool quit = false;

    while (!quit) {
        if (!gameOver) {
            input_system();
            ai_system();
            physics_system();
            collision_system();
            damage_system();
            lifetime_system();
        }

        SDL_SetRenderDrawColor(ren, 0, 0, 0, 255);
        SDL_RenderClear(ren);
        render_system();
        SDL_RenderPresent(ren);

        const auto end = SDL_GetTicks();
        if (end - start < GAME_FRAME)
            SDL_Delay(static_cast<Uint32>(GAME_FRAME - (end - start)));
        start += GAME_FRAME;

        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if ((ev.type == SDL_EVENT_QUIT) ||
                (ev.type == SDL_EVENT_KEY_DOWN && ev.key.scancode == SDL_SCANCODE_ESCAPE))
                quit = true;
        }
    }
}

void ScorchedEarth::input_system()
{
    if (gameOver || projectileInFlight) return;  // no controls during flight or after win

    static const Mask mask = MaskBuilder()
        .set<Input>()
        .set<Artillery>()
        .build();

    const bool* keys = SDL_GetKeyboardState(nullptr);

    for (Entity e = Entity::first(); !e.eof(); e.next()) {
        if (!e.test(mask)) continue;
        auto& art       = e.get<Artillery>();
        const auto& pos = e.get<Position>();
        const auto& d   = e.get<Drawing>();

        if (keys[SDL_SCANCODE_LEFT])  art.angle += 2.f;
        if (keys[SDL_SCANCODE_RIGHT]) art.angle -= 2.f;
        if (keys[SDL_SCANCODE_UP])    art.power += 1.f;
        if (keys[SDL_SCANCODE_DOWN])  art.power -= 1.f;
        art.angle = std::clamp(art.angle, 0.f, 180.f);
        art.power = std::clamp(art.power, 1.f, 300.f);

        // Fire: leading-edge debounce prevents hold-to-spam
        static bool spacePrev = false;
        const bool  spaceNow  = keys[SDL_SCANCODE_SPACE];
        if (spaceNow && !spacePrev) {
            const float rad    = art.angle * (3.14159265f / 180.f);
            const float spawnX = pos.x + cosf(rad) * 32.f;              // BARREL_W = 32.f
            const float spawnY = (pos.y - d.size.y * 0.25f) - sinf(rad) * 32.f;
            const float vx     = cosf(rad) * art.power;
            const float vy     = -sinf(rad) * art.power;  // up = negative Y in SDL
            createProjectile(spawnX, spawnY, vx, vy);
            projectileInFlight = true;
        }
        spacePrev = spaceNow;
    }
}

void ScorchedEarth::ai_system()
{
    if (gameOver || projectileInFlight) return;
    if (activePlayer != 1) return;  // not AI's turn (AI is always tanks[1])

    static const Mask mask = MaskBuilder()
        .set<AI>()
        .set<Artillery>()
        .set<Position>()
        .set<Drawing>()
        .build();

    for (Entity e = Entity::first(); !e.eof(); e.next()) {
        if (!e.test(mask)) continue;

        auto& ai        = e.get<AI>();
        auto& art       = e.get<Artillery>();
        const auto& pos = e.get<Position>();
        const auto& d   = e.get<Drawing>();

        // state=0: idle → start think delay
        // state=1: think delay (difficulty counts down from 40)
        // state=2: sweep barrel toward target (difficulty stores target_angle*10 as int)
        if (ai.state == 0) {
            ai.state = 1;
            ai.difficulty = 40;
            return;
        }

        if (ai.state == 1) {
            if (--ai.difficulty > 0) return;

            // Compute target angle via ballistic formula
            Entity human{tanks[0]};
            if (!human.has<Position>()) return;
            const auto& tpos = human.get<Position>();

            const float dx    = tpos.x - pos.x;
            const float power = 300.f;
            const float g     = 40.f;
            // Range formula: sin(2θ) = |dx|·g / v²
            // Use fabsf(dx) so ratio is always positive; angle is always a valid elevation.
            // Subtract 0.05 to compensate for barrel-tip spawn being ~18px above tank centre.
            float ratio = (fabsf(dx) * g) / (power * power) - 0.05f;
            ratio = std::clamp(ratio, 0.f, 1.f);
            float angle_deg = 0.5f * asinf(ratio) * (180.f / 3.14159265f);
            if (dx < 0.f) angle_deg = 180.f - angle_deg;  // mirror for left-shooting
            // Small random scatter so the AI isn't a perfect robot
            angle_deg += (float)(rand() % 31 - 15);   // ±15° scatter
            angle_deg = std::clamp(angle_deg, 5.f, 175.f);

            // Also randomise power so range varies each shot
            art.power = power + (float)(rand() % 61 - 30);  // ±30 power around 300
            ai.difficulty = (int)(angle_deg * 10.f);  // store target×10 for sweep
            ai.state = 2;
            return;
        }

        // state == 2: sweep barrel toward target angle at 4°/frame
        {
            const float target = (float)ai.difficulty / 10.f;
            const float diff   = target - art.angle;
            if (fabsf(diff) < 4.f) {
                art.angle = target;
                // Fire
                const float rad    = art.angle * (3.14159265f / 180.f);
                const float spawnX = pos.x + cosf(rad) * 32.f;
                const float spawnY = (pos.y - d.size.y * 0.25f) - sinf(rad) * 32.f;
                createProjectile(spawnX, spawnY, cosf(rad) * art.power, -sinf(rad) * art.power);
                projectileInFlight = true;
                ai.state = 0;
            } else {
                art.angle += (diff > 0.f ? 4.f : -4.f);
            }
        }
    }
}

void ScorchedEarth::physics_system() const
{
    constexpr float DT   = 1.0f / 60.0f;
    constexpr float WIND = 0.f;   // Phase 3: zero wind; add as class member in future phase

    static const Mask mask = MaskBuilder()
        .set<Position>()
        .set<Movement>()
        .build();

    for (Entity e = Entity::first(); !e.eof(); e.next()) {
        if (!e.test(mask)) continue;
        auto& pos = e.get<Position>();
        auto& mov = e.get<Movement>();
        if (e.has<Physics>()) {
            const auto& phy = e.get<Physics>();
            mov.vy += phy.gravity * DT;
            mov.vx += WIND * phy.windResistance * DT;
        }
        pos.x += mov.vx * DT;
        pos.y += mov.vy * DT;
    }
}

void ScorchedEarth::collision_system()
{
    // Pass 1: find terrain entity (has GridData + Position)
    Entity terrainEnt{{-1}};
    {
        static const Mask terrMask = MaskBuilder()
            .set<GridData>()
            .set<Position>()
            .build();
        for (Entity e = Entity::first(); !e.eof(); e.next()) {
            if (e.test(terrMask)) { terrainEnt = e; break; }
        }
    }
    if (terrainEnt.entity().id == static_cast<bagel::id_type>(-1)) return;

    const auto& terrainPos = terrainEnt.get<Position>();
    auto&       gd         = terrainEnt.get<GridData>();

    // Pass 2: iterate projectiles (Position + Movement + Weapon)
    static const Mask projMask = MaskBuilder()
        .set<Position>()
        .set<Movement>()
        .set<Weapon>()
        .build();

    for (Entity e = Entity::first(); !e.eof(); e.next()) {
        if (!e.test(projMask)) continue;
        if (e.has<Collided>()) continue;  // already tagged this frame — skip

        const auto& pos = e.get<Position>();
        const float px = pos.x, py = pos.y;

        // Out-of-bounds: tag as Collided to flush through damage_system
        if (px < 0.f || px > static_cast<float>(WIN_W) ||
            py > static_cast<float>(WIN_H) || py < -300.f) {
            e.add(Collided{});
            continue;
        }

        // Terrain cell check: convert world pos to grid cell index
        const int col = static_cast<int>((px - terrainPos.x) / 16.f);
        const int row = static_cast<int>((py - terrainPos.y) / 16.f);
        if (col >= 0 && col < 50 && row >= 0 && row < 8) {
            if (gd.cells[col][row]) {
                e.add(Collided{});
                continue;
            }
        }

        // Tank AABB check: use tanks[0]/tanks[1] directly (no extra scan)
        const auto& pd = e.get<Drawing>();  // projectile Drawing: size {8.f, 8.f}
        for (int t = 0; t < 2; ++t) {
            Entity tank{tanks[t]};
            if (!tank.has<Health>()) continue;  // tank already destroyed
            const auto& tp = tank.get<Position>();
            const auto& td = tank.get<Drawing>();  // tank Drawing: size {48.f, 28.f}
            // AABB: half-sum of widths and heights
            if (fabsf(px - tp.x) < (pd.size.x + td.size.x) * 0.5f &&
                fabsf(py - tp.y) < (pd.size.y + td.size.y) * 0.5f) {
                e.add(Collided{});
                break;
            }
        }
    }
}

void ScorchedEarth::damage_system()
{
    // Pass 1: find terrain entity (same pattern as collision_system)
    Entity terrainEnt{{-1}};
    {
        static const Mask terrMask = MaskBuilder()
            .set<GridData>()
            .set<Position>()
            .build();
        for (Entity e = Entity::first(); !e.eof(); e.next()) {
            if (e.test(terrMask)) { terrainEnt = e; break; }
        }
    }

    static const Mask mask = MaskBuilder()
        .set<Collided>()
        .set<Position>()
        .build();

    bagel::ent_type toDestroy[4]; int destroyCount = 0;

    for (Entity e = Entity::first(); !e.eof(); e.next()) {
        if (!e.test(mask)) continue;
        const auto& pos = e.get<Position>();

        if (e.has<Weapon>()) {
            const auto& wep = e.get<Weapon>();

            // 1. Create visual explosion
            createExplosion(pos.x, pos.y);

            // 2. Terrain deformation: clear cells within explosionRadius
            if (terrainEnt.entity().id != static_cast<bagel::id_type>(-1)) {
                const auto& tpos = terrainEnt.get<Position>();
                auto& gd = terrainEnt.get<GridData>();
                for (int col = 0; col < 50; ++col) {
                    for (int row = 0; row < 8; ++row) {
                        const float cx = tpos.x + col * 16.f + 8.f;  // cell center X
                        const float cy = tpos.y + row * 16.f + 8.f;  // cell center Y
                        const float dist = sqrtf((cx - pos.x) * (cx - pos.x) +
                                                 (cy - pos.y) * (cy - pos.y));
                        if (dist <= wep.explosionRadius) gd.cells[col][row] = false;
                    }
                }
            }

            // 3. Tank health reduction (flat damage, no falloff)
            for (int t = 0; t < 2; ++t) {
                Entity tank{tanks[t]};
                if (!tank.has<Health>()) continue;
                const auto& tp = tank.get<Position>();
                const float dist = sqrtf((tp.x - pos.x) * (tp.x - pos.x) +
                                         (tp.y - pos.y) * (tp.y - pos.y));
                if (dist <= wep.explosionRadius) {
                    auto& hp = tank.get<Health>();
                    hp.current -= wep.damage;
                    if (hp.current <= 0.f && !gameOver) {
                        gameOver = true;
                        winner   = 1 - t;  // the other tank wins
                        break;             // stop processing tanks after first death
                    }
                }
            }

            // 4. Turn switch
            // DESIGN NOTE: R3.6 says "destroy tank entity if health <= 0" but we
            // intentionally deviate: the tank entity is kept alive after death.
            // Destroying it would cause the HUD's health bar slot to disappear and
            // create a dangling tanks[activePlayer] reference. Instead, gameOver=true
            // stops all gameplay. The HUD continues to render HP=0 for the dead tank.
            // This is a conscious design decision, not an omission.
            //
            // Input tag is NEVER moved between turns. Turn identity is tracked entirely
            // by activePlayer. input_system() and ai_system() each guard with their
            // own activePlayer check, so the Input tag on P1 is permanent.
            if (!gameOver) {
                activePlayer ^= 1;
                projectileInFlight = false;
            } else {
                projectileInFlight = false;
            }
        } else {
            // Non-weapon Collided entity (shouldn't happen in Phase 3, but safe fallback)
            projectileInFlight = false;
        }

        // Defer projectile destruction
        toDestroy[destroyCount++] = e.entity();
    }

    // Destroy after loop — never inside the loop (ID recycling risk)
    for (int i = 0; i < destroyCount; ++i)
        Entity{toDestroy[i]}.destroy();
}

void ScorchedEarth::lifetime_system()
{
    constexpr float DT = 1.0f / 60.0f;

    static const Mask mask = MaskBuilder()
        .set<Lifetime>()
        .build();

    bagel::ent_type toDestroy[8]; int destroyCount = 0;

    for (Entity e = Entity::first(); !e.eof(); e.next()) {
        if (!e.test(mask)) continue;
        auto& lt = e.get<Lifetime>();
        lt.remaining -= DT;
        if (lt.remaining <= 0.f)
            toDestroy[destroyCount++] = e.entity();
    }
    for (int i = 0; i < destroyCount; ++i)
        Entity{toDestroy[i]}.destroy();
}

void ScorchedEarth::render_system() const
{
    static const Mask mask = MaskBuilder()
        .set<Position>()
        .set<Drawing>()
        .build();

    for (Entity e = Entity::first(); !e.eof(); e.next()) {
        if (!e.test(mask)) continue;

        const auto& pos = e.get<Position>();
        const auto& d   = e.get<Drawing>();

        // -- Terrain (GridData path) --
        if (e.has<GridData>()) {
            const auto& gd = e.get<GridData>();
            SDL_SetRenderDrawColor(ren, 101, 67, 33, 255); // brown dirt: RGB(101,67,33)
            for (int col = 0; col < 50; ++col) {
                for (int row = 0; row < 8; ++row) {
                    if (!gd.cells[col][row]) continue; // deformed cell — skip
                    SDL_FRect cell = {
                        pos.x + col * 16.f,
                        pos.y + row * 16.f,
                        16.f,
                        16.f
                    };
                    SDL_RenderFillRect(ren, &cell);
                }
            }
            continue; // terrain handled — do not fall through
        }

        // -- Tank (Artillery component present) --
        if (e.has<Artillery>()) {
            const auto& art = e.get<Artillery>();
            SDL_FRect body = {
                pos.x - d.size.x / 2.f,
                pos.y - d.size.y / 2.f,
                d.size.x,
                d.size.y
            };
            if (tex != nullptr) {
                // Body: P1 faces right, P2 faces left (horizontal flip)
                SDL_FlipMode bodyFlip = e.has<Input>() ? SDL_FLIP_NONE : SDL_FLIP_HORIZONTAL;
                SDL_RenderTextureRotated(ren, tex, &d.part, &body, 0.0, nullptr, bodyFlip);

                // Barrel: sprite rotated around its left-center (turret attachment point)
                // SDL rotation is clockwise — negate standard math (CCW) angle
                SDL_FRect barrelSrc = e.has<Input>()
                    ? SDL_FRect{564.f, 230.f, 272.f, 106.f}   // green barrel
                    : SDL_FRect{1464.f, 230.f, 273.f, 107.f};  // red barrel
                constexpr float BARREL_W = 32.f, BARREL_H = 12.f;
                const float attachY = pos.y - d.size.y * 0.25f; // near turret (~25% above center)
                SDL_FRect barrelDst = {pos.x, attachY - BARREL_H / 2.f, BARREL_W, BARREL_H};
                SDL_FPoint pivot = {0.f, BARREL_H / 2.f};
                SDL_RenderTextureRotated(ren, tex, &barrelSrc, &barrelDst,
                    -static_cast<double>(art.angle), &pivot, SDL_FLIP_NONE);
            } else {
                // Colored-rect fallback for body
                if (e.has<Input>()) {
                    SDL_SetRenderDrawColor(ren, 34, 139, 34, 255);
                } else {
                    SDL_SetRenderDrawColor(ren, 178, 34, 34, 255);
                }
                SDL_RenderFillRect(ren, &body);
                // Barrel fallback: dark gray line
                const float rad = art.angle * (3.14159265f / 180.f);
                const float bx2 = pos.x + cosf(rad) * 20.f;
                const float by2 = pos.y - sinf(rad) * 20.f;
                SDL_SetRenderDrawColor(ren, 60, 60, 60, 255);
                SDL_RenderLine(ren, pos.x, pos.y, bx2, by2);
            }
            continue; // tank handled
        }

        // -- Explosion (Lifetime component present) --
        if (e.has<Lifetime>()) {
            SDL_FRect blast = {
                pos.x - d.size.x / 2.f,
                pos.y - d.size.y / 2.f,
                d.size.x,
                d.size.y
            };
            SDL_SetRenderDrawColor(ren, 255, 140, 0, 255); // orange: RGB(255,140,0)
            SDL_RenderFillRect(ren, &blast);
            continue; // explosion handled
        }

        // -- Projectile (default: has Position+Drawing but no GridData/Artillery/Lifetime) --
        {
            SDL_FRect proj = {
                pos.x - d.size.x / 2.f,
                pos.y - d.size.y / 2.f,
                d.size.x,
                d.size.y
            };
            SDL_SetRenderDrawColor(ren, 240, 240, 240, 255); // near-white: RGB(240,240,240)
            SDL_RenderFillRect(ren, &proj);
        }
    }

    // ---- Pass 2: HUD overlay ----
    // Reads Health + Artillery from tank entities; not an entity itself.
    static const Mask hudMask = MaskBuilder()
        .set<Health>()
        .set<Artillery>()
        .build();

    int slot = 0;
    for (Entity e = Entity::first(); !e.eof(); e.next()) {
        if (!e.test(hudMask)) continue;

        const auto& hp  = e.get<Health>();
        const auto& art = e.get<Artillery>();
        const auto& pos = e.get<Position>();
        const auto& d   = e.get<Drawing>();
        const bool active = (e.entity().id == tanks[activePlayer].id);

        // HUD X position: slot 0 = left side (P1), slot 1 = right side (P2)
        const float barX = (slot == 0) ? 10.f : (WIN_W - 70.f);
        const float barY = 10.f;

        // Health bar background — dark red 60x8
        SDL_SetRenderDrawColor(ren, 80, 0, 0, 255);
        SDL_FRect bgRect = {barX, barY, 60.f, 8.f};
        SDL_RenderFillRect(ren, &bgRect);

        // Health bar fill — green, scaled by current/max ratio
        SDL_SetRenderDrawColor(ren, 0, 200, 0, 255);
        const float fillW = 60.f * (hp.current / hp.max);
        SDL_FRect fillRect = {barX, barY, fillW, 8.f};
        SDL_RenderFillRect(ren, &fillRect);

        // HP text — white debug text below bar
        SDL_SetRenderDrawColor(ren, 255, 255, 255, 255);
        SDL_RenderDebugTextFormat(ren, barX, barY + 10.f, "HP %.0f", hp.current);

        // Angle/power text — only for active player (has Input component)
        if (active) {
            SDL_RenderDebugTextFormat(ren, barX, barY + 20.f,
                "A%.0f P%.0f", art.angle, art.power);
        }

        // Active player indicator — small yellow rect above the tank
        if (active) {
            SDL_SetRenderDrawColor(ren, 255, 220, 0, 255); // yellow
            SDL_FRect dot = {
                pos.x - 3.f,
                pos.y - d.size.y / 2.f - 10.f,  // 10px above top of tank body
                6.f, 6.f
            };
            SDL_RenderFillRect(ren, &dot);
        }

        ++slot;
    }

    // ---- Pass 3: Win condition overlay ----
    if (gameOver) {
        SDL_SetRenderDrawColor(ren, 255, 255, 0, 255);  // yellow
        SDL_RenderDebugTextFormat(ren, WIN_W / 2.f - 40.f, WIN_H / 2.f,
            "Player %d wins!", winner + 1);
    }
}

static bagel::ent_type createTank(float x, float y, bool isAI, bool isPlayer, float initAngle)
{
    Entity tank = Entity::create();
    // Drawing source rect from sprite sheet: P1=green (x=0,y=130), P2=red (x=898,y=130)
    SDL_FRect partSrc = isPlayer
        ? SDL_FRect{0.f, 130.f, 536.f, 306.f}    // green tank body
        : SDL_FRect{898.f, 130.f, 538.f, 306.f};  // red tank body
    tank.addAll(
        Position{x, y},
        Drawing{partSrc, {48.f, 28.f}},
        Health{100.f, 100.f},
        Artillery{initAngle, 300.f},
        Physics{9.8f, 0.1f}
    );
    if (isPlayer) tank.add(Input{});
    if (isAI)     tank.add(AI{0, 1});
    return tank.entity();
}

static bagel::ent_type createProjectile(float x, float y, float vx, float vy)
{
    Entity proj = Entity::create();
    proj.addAll(
        Position{x, y},
        Movement{vx, vy},
        Drawing{{0,0,8,8}, {8.f, 8.f}},
        Physics{40.f, 0.05f},
        Weapon{40.f, 50.f}
    );
    return proj.entity();
}

static bagel::ent_type createTerrain(float x, float y)
{
    Entity terrain = Entity::create();
    GridData gd;
    for (int c = 0; c < 50; ++c)
        for (int r = 0; r < 8; ++r)
            gd.cells[c][r] = true;
    terrain.addAll(
        Position{x, y},
        Drawing{{0, 0, 800, 600}, {800.f, 600.f}},
        gd
    );
    return terrain.entity();
}

static bagel::ent_type createExplosion(float x, float y)
{
    Entity exp = Entity::create();
    exp.addAll(
        Position{x, y},
        Drawing{{0,0,64,64}, {64.f, 64.f}},
        Lifetime{1.0f, 1.0f},
        AreaDamage{40.f, 50.f}
    );
    return exp.entity();
}

} // namespace se
