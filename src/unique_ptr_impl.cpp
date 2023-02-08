/* integration of chipmunk2d in flecs using std::unique_ptr
 *
 * Building off the simple struct implementation, this implementation will
 * use std::unique_ptr as our Components.
 *
 * Note: flecs does not support static addresses for components, and chipmunk2d
 * uses pointers internally for connecting structs together.  As a result, all
 * of our flecs components will be wrappers around pointers to chipmunk
 * structures.  Additionally, all of our components will be move-only
 * structures, as they must manage the cleanup of any resources at destruction
 * time.
 */

#include <chipmunk/chipmunk.h>
#include <cmath>
#include <flecs.h>
#include <gtest/gtest.h>
#include <memory>

#include "common.hpp"
#include "flecs/addons/cpp/c_types.hpp"

/// wrapper around cpShape (cpSegmentShape, cpPolyShape, ...)
struct Shape {
    Shape() : ptr{nullptr} {}
    Shape(cpShape *p) : ptr{p} {
        log_debug("wrap shape {}", fmt::ptr(ptr));
    }
    Shape(const Shape&) = delete;
    Shape(Shape&& other) : ptr{nullptr} {
        *this = std::move(other);
    }
    ~Shape() {
        if (ptr) {
            log_debug("free shape {}", fmt::ptr(ptr));
            assert(cpShapeGetSpace(ptr) == nullptr && "not removed from space");
            cpShapeFree(ptr);
        }
    }

    Shape& operator=(const Shape&) = delete;
    Shape& operator=(Shape&& other) {
        if (this != &other) {
            if (ptr) {
                log_debug("free shape {}", fmt::ptr(ptr));
                assert(cpShapeGetSpace(ptr) == nullptr && "not removed from space");
                cpShapeFree(ptr);
            }
            ptr       = other.ptr;
            other.ptr = nullptr;
        }
        return *this;
    }

    /// support implicit cast to cpShape*
    inline operator cpShape*() const {
        assert(ptr != nullptr && "cpShape pointer not initialized");
        return ptr;
    };

    cpShape* ptr;
};

/// component to denote a collision has occurred
struct Collision {};

/// different collision types
enum CollisionType {
    CT_Player = 1,
    CT_Object,
    CT_Projectile,
    CT_Sensor,
};

struct SpaceDeleter {
    void operator()(cpSpace *space) {
        if (space) {
            log_debug("free space {}", fmt::ptr(space));
            cpSpaceFree(space);
        }
    }
};
using Space = std::unique_ptr<cpSpace, SpaceDeleter>;

struct BodyDeleter {
    void operator()(cpBody *body) {
        if (body) {
            log_debug("free body {}", fmt::ptr(body));
            cpBodyFree(body);
        }
    }
};
using Body = std::unique_ptr<cpBody, BodyDeleter>;

/// chipmunk2d module to load into flecs
struct chipmunk2d {
    chipmunk2d(flecs::world &ecs) {
        // create the physics space all bodies will reside in
        cpSpace *space = cpSpaceNew();
        assert(space != nullptr);

        // set no gravity for now
        cpSpaceSetGravity(space, {0, 0});

        // add the space to the world as a singleton component
        ecs.emplace<Space>(space);

        auto *sp = ecs.get<Space>();

        // add a system to step the physics space each frame
        ecs.system<>("step_space")
            .kind(flecs::PreUpdate)
            .iter([](flecs::iter &it) {
                auto *space = it.world().get_mut<Space>();
                cpSpaceStep(space->get(), it.delta_time());
            });

        // When a Body component is added to an entity do the following:
        // - set the cpBody UserData to be the entity id
        //   - this allows chipmunk2d collision handlers to map from cpBody to
        //     a flecs entity id
        // - add the cpBody to the singleton cpSpace
        ecs.observer<Body, Space>("body_on_set")
            .arg(2).src<Space>()
            .event(flecs::OnSet)
            .each([](flecs::entity entity, Body& body, Space& space) {
                    log_debug("Body OnSet {}", entity);
                    cpBodySetUserData(body.get(), (void *)entity.id());
                    cpSpaceAddBody(space.get(), body.get());
                });

        // When a Body component is removed from an entity, remove the
        // associated cpBody from the singleton cpSpace
        ecs.observer<Body, Space>("body_on_remove")
            .arg(2).src<Space>()
            .event(flecs::OnRemove)
            .each([](flecs::entity entity, Body& body, Space& space) {
                    log_debug("Body OnRemove {}", entity);
                    cpSpaceRemoveBody(space.get(), body.get());
                });

        // When a Shape component is added to an entity do the following:
        // - add the cpBody to the singleton cpSpace
        ecs.observer<Shape, Space>("shape_on_set")
            .arg(2).src<Space>()
            .event(flecs::OnSet)
            .each([](flecs::entity entity, Shape& shape, Space& space) {
                    log_debug("Shape OnSet {}", entity);
                    cpSpaceAddShape(space.get(), shape);
                });

        // When a Shape component is removed from an entity, remove the cpShape
        // from the singleton cpSpace
        ecs.observer<Shape, Space>("shape_on_remove")
            .arg(2).src<Space>()
            .event(flecs::OnRemove)
            .each([](flecs::entity entity, Shape& shape, Space& space) {
                    log_debug("Shape OnRemove {}", entity);
                    cpSpaceRemoveShape(space.get(), shape);
                });
    }
};

// scenarios:
// - projectile collides with entity
// - player runs into closed door
// - player steps on trap
// - pivot-joint based movement
// - multiple shapes on a single body

/// shoot a projectile at an object, destroying both when they collide
TEST(unique_ptr, projectile_collision) {
    // create the world and load the chipmunk2d plugin
    flecs::world ecs;
    ecs.import<chipmunk2d>();

    // get the space singleton, it was created by the plugin.  We need it for
    // creating physics bodies and shapes.
    Space *space = ecs.get_mut<Space>();
    ASSERT_NE(space->get(), nullptr)
        << "Space singleton pointer is null";

    // add a collision handler to the space to add a collision component to any
    // entity struct by a projectile
    cpCollisionHandler* handler =
        cpSpaceAddWildcardHandler(space->get(), CT_Projectile);
    handler->userData  = &ecs;
    handler->beginFunc = [](cpArbiter* arb, cpSpace*,
                             cpDataPointer data) -> cpBool {
        auto* ecs = static_cast<flecs::world *>(data);
        assert(ecs != nullptr && "invalid world in space userdata");

        cpBody *a, *b;
        cpArbiterGetBodies(arb, &a, &b);
        flecs::entity proj = ecs->entity((uintptr_t)cpBodyGetUserData(a));
        flecs::entity other = ecs->entity((uintptr_t)cpBodyGetUserData(b));

        log_debug("projectile collision: {} -> {}",
                proj.doc_name(), other.doc_name());

        proj.add<Collision>(other);
        other.add<Collision>(proj);

        return true;
    };

    // create a system that removes any entity struck in a collision, both the
    // projectile and the object
    ecs.system<>()
        .with<Collision>(flecs::Wildcard)
        .each([](flecs::entity e) {
                log_debug("{} collided; removing", e.doc_name());
                e.destruct();
            });

    // create an arrow entity
    flecs::entity arrow = ecs.entity("arrow");

    // assign a physics body to it moving to the right at 10 units/sec
    cpBody *body = cpBodyNew(1, INFINITY);
    cpBodySetPosition(body, {0, 0});
    cpBodySetVelocity(body, {10, 0});
    arrow.emplace<Body>(body);

    // add a shape to the physics body
    cpShape *shape = cpCircleShapeNew(body, 1, {0,0});
    cpShapeSetCollisionType(shape, CT_Projectile);
    arrow.set<Shape>(shape);

    // create an apple entity
    flecs::entity apple = ecs.entity("apple");

    // assign a physics body for the apple
    body = cpBodyNew(1, INFINITY);
    cpBodySetPosition(body, {10, 0});
    apple.emplace<Body>(body);

    // assign a 5x5 shape to the apple physics body
    shape = cpBoxShapeNew(body, 5, 5, 3);
    cpShapeSetCollisionType(shape, CT_Object);
    apple.set<Shape>(shape);

    // advance the world a second in 1/60 sec frames
    log_debug("stepping space");
    for (int i = 0; i < 60; i++) {
        ecs.progress(1/60.0);
        if (!arrow.is_valid() || !apple.is_valid()) {
            break;
        }
        auto *p = arrow.get<Body>();
        auto *a = apple.get<Body>();
        log_debug("arrow {}, apple {}", *p->get(), *a->get());
    }

    // verify that both the apple and the arrow were destroyed
    EXPECT_EQ(arrow.is_valid(), false) << "arrow should have been destroyed";
    EXPECT_EQ(apple.is_valid(), false) << "apple should have been destroyed";
}

/// shoot a projectile at an object, only destroying the object, and allowing
/// the projectile to continue travelling
TEST(unique_ptr, indestructable_projectile) {
    // create the world and load the chipmunk2d plugin
    flecs::world ecs;
    ecs.import<chipmunk2d>();

    // get the space singleton, it was created by the plugin.  We need it for
    // creating physics bodies and shapes.
    Space &space = *ecs.get_mut<Space>();

    // add a collision handler to the space to add a collision component to any
    // entity struct by a projectile
    cpCollisionHandler* handler =
        cpSpaceAddWildcardHandler(space.get(), CT_Projectile);
    handler->userData  = &ecs;
    handler->beginFunc = [](cpArbiter* arb, cpSpace*,
                             cpDataPointer data) -> cpBool {
        auto* ecs = static_cast<flecs::world *>(data);
        assert(ecs != nullptr && "invalid world in space userdata");

        cpBody *a, *b;
        cpArbiterGetBodies(arb, &a, &b);
        flecs::entity proj = ecs->entity((uintptr_t)cpBodyGetUserData(a));
        flecs::entity other = ecs->entity((uintptr_t)cpBodyGetUserData(b));

        log_debug("projectile collision: {} -> {}",
                proj, other);

        other.add<Collision>(proj);

        // return false here so chipmunk2d does not reduce the velocity of the
        // projectile based on this collision.
        return false;
    };

    // create a system that removes any entity struck in a collision, both the
    // projectile and the object
    ecs.system<>()
        .with<Collision>(flecs::Wildcard)
        .each([](flecs::entity e) {
                log_debug("{} collided; removing", e);
                e.destruct();
            });

    // create an arrow entity
    flecs::entity arrow = ecs.entity("arrow");

    // assign a physics body to it moving to the right at 20 units/sec
    cpBody *body = cpBodyNew(1, INFINITY);
    cpBodySetPosition(body, {0, 0});
    cpBodySetVelocity(body, {25, 0});
    arrow.emplace<Body>(body);

    // add a shape to the physics body
    cpShape *shape = cpCircleShapeNew(body, 1, {0,0});
    cpShapeSetCollisionType(shape, CT_Projectile);
    arrow.set<Shape>(shape);

    // create a row of apples to destroy
    struct Apple {};
    for (int i = 0; i < 5; i++) {
        cpBody *body = cpBodyNew(1, INFINITY);
        cpBodySetPosition(body, {5.0 + (i * 5), 0});
        cpShape *shape = cpBoxShapeNew(body, 1, 1, 0);
        cpShapeSetCollisionType(shape, CT_Object);
        flecs::entity apple = ecs.entity()
            .add<Apple>()
            .emplace<Body>(body)
            .set<Shape>(shape);
    }

    // advance the world a second in 1/60 sec frames
    log_debug("stepping space");
    for (int i = 0; i < 60; i++) {
        ecs.progress(1/60.0);
        if (!arrow.is_valid()) {
            break;
        }
        auto *p = arrow.get<Body>();
        log_debug("arrow {}", *p->get());
    }

    // verify all the apples have been destroyed
    EXPECT_EQ(ecs.count<Apple>(), 0) << "not all apples were destroyed";

    EXPECT_EQ(arrow.is_valid(), true) << "arrow was unexpectedly destroyed";

    // get the arrow body and verify it wasn't slowed down
    const Body *b = arrow.get<Body>();
    ASSERT_NE(b, nullptr) << "arrow missing Body component";
    cpVect v = cpBodyGetVelocity(b->get());
    EXPECT_EQ(v, cpv(25, 0))
        << fmt::format("arrow did not maintain velocity: {}", v);
}
