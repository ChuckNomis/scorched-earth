#pragma once
#include <cstdlib>
#include <algorithm>
#include <SDL3/SDL.h>
#include "bagel.h"
namespace se {
    using Drawing   = struct { SDL_FRect part; SDL_FPoint size; };
    using Position  = struct { float x, y; };
    using Movement  = struct { float vx, vy; };
    using Input     = struct {};
    using AI        = struct { int state, difficulty; };
    using Artillery = struct { float angle, power; };
    using Weapon    = struct { float explosionRadius, damage; };
    using Health    = struct { float current, max; };
    using Physics   = struct { float gravity, windResistance; };
    using Lifetime  = struct { float remaining, duration; };
    using Collided  = struct {};
    using Currency  = struct { int amount; };
    using Inventory = struct { int weaponType, count; };
    using GridData  = struct { bool cells[50][8]; };
    using AreaDamage= struct { float radius, damage; };
}

template <> struct bagel::Storage<se::Position> final : bagel::NoInstance {
    using type = bagel::PackedStorage<se::Position>;
};
template <> struct bagel::Storage<se::Movement> final : bagel::NoInstance {
    using type = bagel::PackedStorage<se::Movement>;
};
template <> struct bagel::Storage<se::Lifetime> final : bagel::NoInstance {
    using type = bagel::PackedStorage<se::Lifetime>;
};
template <> struct bagel::Storage<se::Input> final : bagel::NoInstance {
    using type = bagel::TaggedStorage<se::Input>;
};
template <> struct bagel::Storage<se::Collided> final : bagel::NoInstance {
    using type = bagel::TaggedStorage<se::Collided>;
};

namespace se {
    class ScorchedEarth {
    public:
        ScorchedEarth();
        ~ScorchedEarth();
        void run();
        bool valid() const { return initialized; }
    private:
        static constexpr int    WIN_W = 800;
        static constexpr int    WIN_H = 600;
        static constexpr int    FPS   = 60;
        static constexpr Uint64 GAME_FRAME = 1000 / FPS;

        void input_system();
        void physics_system() const;
        void collision_system();
        void damage_system();
        void ai_system();
        void lifetime_system();
        void render_system() const;
        void shop_system();

        SDL_Texture*  tex              = nullptr;
        SDL_Renderer* ren              = nullptr;
        SDL_Window*   win              = nullptr;
        bool          initialized      = false;
        int           activePlayer     = 0;
        bool          shopOpen         = false;
        bool          projectileInFlight = false;
        bagel::ent_type tanks[2]          = {{-1}, {-1}};  // tanks[0]=P1, tanks[1]=P2; populated in constructor
        bool            gameOver          = false;
        int             winner            = -1;             // 0 or 1 when gameOver is true
    };
}
