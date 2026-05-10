#pragma once
#include <algorithm>
#include "bagel.h"
#include <SDL3/SDL.h>

namespace scorched {

    // Storage: sparse – each entity has unique visual data; rarely iterated in bulk
    struct Drawing {
        SDL_Texture* texture = nullptr;
        SDL_FRect srcRect = {};
        SDL_FRect dstRect = {};
        // int sound = -1;  // sound effect ID – int placeholder, audio API not yet learned
    };

    // Storage: dense (PackedStorage) – read and written every frame by physics & render
    struct Position {
        float x = 0.f;
        float y = 0.f;
    };

    // Storage: dense (PackedStorage) – updated every frame by physics system
    struct Movement {
        float vx = 0.f;
        float vy = 0.f;
    };

    // Storage: tag (TaggedStorage) – marks player-controlled entity; carries no data
    struct Input {};

    // Storage: sparse – only a few AI tanks exist; accessed only on that tank's turn
    struct AI {
        int state = 0;
        int difficulty = 1;
    };

    // Storage: sparse – only tanks possess a barrel; accessed once per turn
    struct Artillery {
        float angle = 45.f;   // degrees, 0 = right, 90 = up
        float power = 50.f;
    };

    // Storage: sparse – only projectiles carry weapon data; accessed on collision only
    struct Weapon {
        float explosionRadius = 50.f;
        float damage = 25.f;
    };

    // Storage: sparse – only tanks have health; updated only when damage is applied
    struct Health {
        float current = 100.f;
        float max = 100.f;
    };

    // Storage: sparse – only physics-affected entities; constants rarely change
    struct Physics {
        float gravity = 9.8f;
        float windResistance = 0.f;
    };

    // Storage: dense (PackedStorage) – decremented every frame; tight iteration required
    struct Lifetime {
        float remaining = 3.f;
        float duration = 3.f;
    };

    // Storage: tag (TaggedStorage) – temporary event marker; added/removed per collision frame
    struct Collided {};

    // --- Systems ---
    class RenderSystem    { public: static void update(); };
    class PhysicsSystem   { public: static void update(); };
    class InputSystem     { public: static void update(); };
    class AISystem        { public: static void update(); };
    class CollisionSystem { public: static void update(); };
    class DamageSystem    { public: static void update(); };

    // --- Entity Factories ---
    bagel::ent_type createTank(float x, float y, bool isAI = false, bool isPlayerTurn = false);
    bagel::ent_type createProjectile(float x, float y, float vx, float vy);
    bagel::ent_type createTerrain(float x, float y);
    bagel::ent_type createExplosion(float x, float y, float lifetime = 2.f);

} // namespace scorched

template<> struct bagel::Storage<scorched::Position> final : bagel::NoInstance {
    using type = bagel::PackedStorage<scorched::Position>;
};
template<> struct bagel::Storage<scorched::Movement> final : bagel::NoInstance {
    using type = bagel::PackedStorage<scorched::Movement>;
};
template<> struct bagel::Storage<scorched::Lifetime> final : bagel::NoInstance {
    using type = bagel::PackedStorage<scorched::Lifetime>;
};
template<> struct bagel::Storage<scorched::Input> final : bagel::NoInstance {
    using type = bagel::TaggedStorage<scorched::Input>;
};
template<> struct bagel::Storage<scorched::Collided> final : bagel::NoInstance {
    using type = bagel::TaggedStorage<scorched::Collided>;
};
