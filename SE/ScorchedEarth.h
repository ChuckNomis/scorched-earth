#pragma once
#include <cstdlib>
#include <algorithm>
#include <SDL3/SDL.h>
#include "bagel.h"

namespace se {

/// @brief Sprite source rect and display size used by @ref ScorchedEarth::render_system.
using Drawing = struct {
    SDL_FRect  part; ///< Source rectangle within the sprite sheet.
    SDL_FPoint size; ///< Display size in pixels (width, height).
};

/// @brief World-space position of an entity.
using Position = struct {
    float x; ///< Horizontal coordinate (pixels, left = 0).
    float y; ///< Vertical coordinate (pixels, top = 0).
};

/// @brief Linear velocity applied each frame by @ref ScorchedEarth::physics_system.
using Movement = struct {
    float vx; ///< Horizontal velocity (pixels/second).
    float vy; ///< Vertical velocity (pixels/second, positive = down).
};

/// @brief Tag component that marks the player-controlled tank.
/// Queried by @ref ScorchedEarth::input_system to restrict keyboard handling to one entity.
using Input = struct {};

/// @brief State machine data for the AI-controlled tank.
/// Drives @ref ScorchedEarth::ai_system through three states:
/// 0 = idle, 1 = think delay, 2 = barrel sweep toward ballistic target.
using AI = struct {
    int state;      ///< Current FSM state.
    int difficulty; ///< Countdown timer (state 1) or encoded target angle × 10 (state 2).
};

/// @brief Barrel angle and launch power for a tank cannon.
/// Written by @ref ScorchedEarth::input_system (player) and @ref ScorchedEarth::ai_system (AI).
using Artillery = struct {
    float angle; ///< Barrel elevation in degrees (0–180; 90 = straight up).
    float power; ///< Launch speed in pixels/second (clamped 1–300).
};

/// @brief Explosion parameters carried by a projectile entity.
/// Read by @ref ScorchedEarth::damage_system to deform terrain and reduce @ref Health.
using Weapon = struct {
    float explosionRadius; ///< Blast radius in pixels.
    float damage;          ///< Flat hit-point loss dealt to tanks inside the blast.
};

/// @brief Hit-point pool for a tank entity.
/// Reduced by @ref ScorchedEarth::damage_system; triggers game-over when current reaches 0.
using Health = struct {
    float current; ///< Remaining hit points.
    float max;     ///< Starting (maximum) hit points.
};

/// @brief Per-entity gravity and wind-resistance coefficients.
/// Applied by @ref ScorchedEarth::physics_system every frame.
using Physics = struct {
    float gravity;        ///< Downward acceleration (pixels/second²).
    float windResistance; ///< Multiplier on the global wind force.
};

/// @brief Countdown timer that controls how long an entity lives.
/// @ref ScorchedEarth::lifetime_system decrements `remaining` each frame and destroys the entity at zero.
using Lifetime = struct {
    float remaining; ///< Seconds until destruction.
    float duration;  ///< Original lifespan (seconds).
};

/// @brief Tag component added to a projectile the frame it hits something.
/// Detected by @ref ScorchedEarth::collision_system; consumed by @ref ScorchedEarth::damage_system.
using Collided = struct {};

/// @brief Deformable terrain represented as a 50 × 8 boolean grid.
/// Each cell is 16 × 16 pixels. `true` = solid ground; `false` = blasted away.
/// @ref ScorchedEarth::collision_system reads it for hit detection;
/// @ref ScorchedEarth::damage_system clears cells within the blast radius.
using GridData = struct {
    bool cells[50][8]; ///< Solid flags indexed by [column][row].
};

/// @brief Area-of-effect descriptor attached to an explosion entity.
using AreaDamage = struct {
    float radius; ///< Blast radius in pixels.
    float damage; ///< Damage dealt to entities inside the radius.
};

} // namespace se

/// @brief Uses PackedStorage for cache-friendly @ref se::Position iteration.
template <> struct bagel::Storage<se::Position> final : bagel::NoInstance {
    using type = bagel::PackedStorage<se::Position>;
};
/// @brief Uses PackedStorage for cache-friendly @ref se::Movement iteration.
template <> struct bagel::Storage<se::Movement> final : bagel::NoInstance {
    using type = bagel::PackedStorage<se::Movement>;
};
/// @brief Uses PackedStorage for cache-friendly @ref se::Lifetime iteration.
template <> struct bagel::Storage<se::Lifetime> final : bagel::NoInstance {
    using type = bagel::PackedStorage<se::Lifetime>;
};
/// @brief Uses TaggedStorage (no per-entity data) for @ref se::Input.
template <> struct bagel::Storage<se::Input> final : bagel::NoInstance {
    using type = bagel::TaggedStorage<se::Input>;
};
/// @brief Uses TaggedStorage (no per-entity data) for @ref se::Collided.
template <> struct bagel::Storage<se::Collided> final : bagel::NoInstance {
    using type = bagel::TaggedStorage<se::Collided>;
};

namespace se {

/// @brief Top-level game class — owns the ECS world and drives the main loop.
///
/// Construction spawns all initial entities (terrain, two tanks).
/// @ref run() ticks every system in a fixed 60 Hz loop until ESC or window close.
///
/// **System order per frame:**
/// 1. @ref input_system
/// 2. @ref ai_system
/// 3. @ref physics_system
/// 4. @ref collision_system
/// 5. @ref damage_system
/// 6. @ref lifetime_system
/// 7. @ref render_system (always runs, even on game-over)
class ScorchedEarth {
public:
    ScorchedEarth();
    ~ScorchedEarth();

    /// @brief Runs the fixed-timestep game loop.
    /// Blocks until the user closes the window or presses ESC.
    void run();

    /// @brief Returns `true` if SDL initialisation succeeded and the game is ready.
    bool valid() const { return initialized; }

private:
    static constexpr int    WIN_W      = 800;
    static constexpr int    WIN_H      = 600;
    static constexpr int    FPS        = 60;
    static constexpr Uint64 GAME_FRAME = 1000 / FPS;

    /// @brief Reads keyboard state and updates @ref Artillery for the @ref Input -tagged tank.
    /// Arrow keys adjust angle/power; Space fires via `createProjectile`.
    void input_system();

    /// @brief Applies @ref Physics (gravity, wind) to every entity with @ref Position + @ref Movement.
    void physics_system() const;

    /// @brief Tags projectiles with @ref Collided when they intersect terrain cells or tank AABBs.
    void collision_system();

    /// @brief Resolves @ref Collided projectiles: spawns explosion, deforms @ref GridData,
    /// reduces @ref Health, and switches `activePlayer`. Destroys the projectile entity.
    void damage_system();

    /// @brief Three-state FSM (idle → think → sweep) that aims and fires for the @ref AI tank.
    void ai_system();

    /// @brief Decrements @ref Lifetime `remaining` each frame and destroys entities that reach zero.
    void lifetime_system();

    /// @brief Draws all @ref Position + @ref Drawing entities, the HUD health bars, and the win overlay.
    void render_system() const;

    SDL_Texture*  tex               = nullptr;
    SDL_Renderer* ren               = nullptr;
    SDL_Window*   win               = nullptr;
    bool          initialized       = false;
    int           activePlayer      = 0;          ///< 0 = P1 (human), 1 = P2 (AI).
    bool          projectileInFlight = false;
    bagel::ent_type tanks[2]        = {{-1}, {-1}}; ///< tanks[0]=P1, tanks[1]=P2.
    bool            gameOver        = false;
    int             winner          = -1;            ///< 0 or 1 once @ref gameOver is true.
};

} // namespace se
