#include "scorched_earth.h"
using namespace bagel;

namespace scorched {

    ent_type createTank(float x, float y, bool isAI, bool isPlayerTurn) {
        ent_type e = World::createEntity();
        Storage<Position>::type::add(e, {x, y});
        Storage<Drawing>::type::add(e, {});
        Storage<Health>::type::add(e, {});
        Storage<Artillery>::type::add(e, {});
        Storage<Physics>::type::add(e, {});
        if (isAI)
            Storage<AI>::type::add(e, {});
        if (isPlayerTurn)
            Storage<Input>::type::add(e, {});
        return e;
    }

    ent_type createProjectile(float x, float y, float vx, float vy) {
        ent_type e = World::createEntity();
        Storage<Position>::type::add(e, {x, y});
        Storage<Movement>::type::add(e, {vx, vy});
        Storage<Drawing>::type::add(e, {});
        Storage<Physics>::type::add(e, {});
        Storage<Weapon>::type::add(e, {});
        return e;
    }

    ent_type createTerrain(float x, float y) {
        ent_type e = World::createEntity();
        Storage<Position>::type::add(e, {x, y});
        Storage<Drawing>::type::add(e, {});
        return e;
    }

    ent_type createExplosion(float x, float y, float lifetime) {
        ent_type e = World::createEntity();
        Storage<Position>::type::add(e, {x, y});
        Storage<Drawing>::type::add(e, {});
        Storage<Lifetime>::type::add(e, {lifetime, lifetime});
        return e;
    }

    void RenderSystem::update()    {}
    void PhysicsSystem::update()   {}
    void InputSystem::update()     {}
    void AISystem::update()        {}
    void CollisionSystem::update() {}
    void DamageSystem::update()    {}

} // namespace scorched
