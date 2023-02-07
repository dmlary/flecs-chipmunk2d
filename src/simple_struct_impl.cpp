/* integration of chipmunk2d in flecs using simple structs
 *
 * Note: there's no inheritance, no templating, just dumb copy-paste to show
 * the most direct implementation of this possible.
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

#include "common.hpp"
#include "flecs/addons/cpp/mixins/pipeline/decl.hpp"

/// wrapper around cpSpace
struct Space {
    // we need a default constructor to be able to use
    // `flecs::entity::set(std::move(...))`, otherwise we will trigger a
    // runtime assertion during `set()`.
    //
    // If you're adamently against having a default constructor, you can use
    // `flecs::entity::emplace()` for these components, but if you accidentally
    // call `set()` instead of `emplace()`, you'll hit the runtime assertion.
    Space() : ptr{nullptr} {}
    Space(cpSpace *p) : ptr{p} {
        log_debug("wrap space {}", fmt::ptr(ptr));
    }
    Space(const Space&) = delete;
    Space(Space&& other) : ptr{nullptr} {
        *this = std::move(other);
    }
    ~Space() {
        if (ptr) {
            log_debug("free space {}", fmt::ptr(ptr));
            cpSpaceFree(ptr);
        }
    }

    Space& operator=(const Space&) = delete;
    Space& operator=(Space&& other) {
        if (this != &other) {
            assert(ptr == nullptr);
            ptr       = other.ptr;
            other.ptr = nullptr;
        }
        return *this;
    }

    /// support implicit cast to cpSpace*
    inline operator cpSpace*() const {
        assert(ptr != nullptr && "cpSpace pointer not initialized");
        return ptr;
    };

    cpSpace* ptr;
};

/// wrapper around cpBody
struct Body {
    Body() : ptr{nullptr} {}
    Body(cpBody *p) : ptr{p} {
        log_debug("wrap body {}", fmt::ptr(ptr));
    }
    Body(const Body&) = delete;
    Body(Body&& other) : ptr{nullptr} {
        *this = std::move(other);
    }
    ~Body() {
        if (ptr) {
            log_debug("free body {}", fmt::ptr(ptr));
            assert(cpBodyGetSpace(ptr) == nullptr && "not removed from space");
            cpBodyFree(ptr);
        }
    }

    Body& operator=(const Body&) = delete;
    Body& operator=(Body&& other) {
        if (this != &other) {
            assert(ptr == nullptr);
            ptr       = other.ptr;
            other.ptr = nullptr;
        }
        return *this;
    }

    /// support implicit cast to cpBody*
    inline operator cpBody*() const {
        assert(ptr != nullptr && "cpBody pointer not initialized");
        return ptr;
    };

    cpBody* ptr;
};

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
            assert(ptr == nullptr);
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

/// different collision types
enum CollisionType {
    CT_Ground = 1,
    CT_Apple,
};

/// chipmunk2d module to load into flecs
struct chipmunk2d {
    chipmunk2d(flecs::world &ecs) {
        // create the physics space all bodies will reside in
        Space space{cpSpaceNew()};
        cpSpaceSetGravity(space, {0, 0});

        // add the space as a singleton component, note the use of std::move to
        // move the component as none of our components are copyable
        ecs.set<Space>(std::move(space));

        // add a system to step the physics space each frame
        ecs.system<>("step_space")
            .kind(flecs::PreUpdate)
            .iter([](flecs::iter &it) {
                auto *space = it.world().get_mut<Space>();
                cpSpaceStep(*space, it.delta_time());
            });
    }
};

TEST(simple_struct, space_created) {
    flecs::world ecs;
    ecs.import<chipmunk2d>();
    ASSERT_NE(ecs.get<Space>(), nullptr) << "Space singleton not created";

    // get the space singleton; needed for setting up our scene
    Space &space = *ecs.get_mut<Space>();

    // create a ground entity; this is just a shape attached to the space's
    // static body
    Shape shape{cpSegmentShapeNew(cpSpaceGetStaticBody(space),
            {-10,0}, {10,0}, 0)};
    cpShapeSetCollisionType(shape, CT_Ground);
    cpSpaceAddShape(space, shape);
    ecs.entity("ground").set<Shape>(std::move(shape));

    // create an apple to fall to the ground
    Body body{cpBodyNew(1, INFINITY)};
    cpBodySetPosition(body, {0, 20});
    cpBodySetVelocity(body, {0, -20});
    cpSpaceAddBody(space, body);
    cpBodyActivate(body);
    shape = cpBoxShapeNew(body, 10, 10, 0);
    cpShapeSetCollisionType(shape, CT_Apple);
    cpSpaceAddShape(space, shape);
    ecs.entity("apple")
        .set<Body>(std::move(body))
        .set<Shape>(std::move(shape));

    auto *handler = cpSpaceAddWildcardHandler(space, CT_Apple);
    handler->beginFunc = [](cpArbiter* arb, cpSpace*,
                             cpDataPointer) -> cpBool {
        cpBody *a, *b;
        cpArbiterGetBodies(arb, &a, &b);
        log_debug("collision: a {}, b {}", *a, *b);
        return true;
    };

    log_debug("stepping space");
    for (int i = 0; i < 60; i++) {
        ecs.progress(1/60.0);
        auto *body = ecs.entity("apple").get_mut<Body>();
        log_debug("apple {}", *body->ptr);
    }

    {
        const Shape& ground = *ecs.entity("ground").get<Shape>();
        EXPECT_EQ(cpSegmentShapeGetA(ground), cpv(-10,0)) <<
            fmt::format("A moved to {}", cpSegmentShapeGetA(ground));
        EXPECT_EQ(cpSegmentShapeGetB(ground), cpv(10,0)) <<
            fmt::format("B moved to {}", cpSegmentShapeGetB(ground));
    }

    {
        const Body& apple = *ecs.entity("apple").get<Body>();
        EXPECT_EQ(cpvnear(cpBodyGetPosition(apple), cpv(0,5), 0.01), true) <<
            fmt::format("Apple moved to {}", cpBodyGetPosition(apple));
    }
}
