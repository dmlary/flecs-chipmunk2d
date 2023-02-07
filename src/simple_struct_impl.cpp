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
    Space() {
        ptr = cpSpaceNew();
    }
    Space(const Space&) = delete;
    Space(Space&& other) {
        ptr       = other.ptr;
        other.ptr = nullptr;
    }
    ~Space() {
        if (ptr) {
            cpSpaceFree(ptr);
        }
    }

    Space& operator=(const Space&) = delete;
    Space& operator=(Space&& other) {
        if (this != &other) {
            ptr       = other.ptr;
            other.ptr = nullptr;
        }
        return *this;
    }

    /// support implicit cast to cpSpace*
    inline operator cpSpace*() const { return ptr; };

    cpSpace* ptr;
};

/// wrapper around cpBody
struct Body {
    Body(cpBody *p) : ptr{p} {}
    Body(const Body&) = delete;
    Body(Body&& other) {
        ptr       = other.ptr;
        other.ptr = nullptr;
    }
    ~Body() {
        if (ptr) {
            assert(cpBodyGetSpace(ptr) == nullptr && "not removed from space");
            cpBodyFree(ptr);
        }
    }

    Body& operator=(const Body&) = delete;
    Body& operator=(Body&& other) {
        if (this != &other) {
            ptr       = other.ptr;
            other.ptr = nullptr;
        }
        return *this;
    }

    /// support implicit cast to cpBody*
    inline operator cpBody*() const { return ptr; };

    cpBody* ptr;
};

/// wrapper around cpShape (cpSegmentShape, cpPolyShape, ...)
struct Shape {
    Shape(cpShape *p) : ptr{p} {}
    Shape(const Shape&) = delete;
    Shape(Shape&& other) {
        ptr       = other.ptr;
        other.ptr = nullptr;
    }
    ~Shape() {
        if (ptr) {
            assert(cpShapeGetSpace(ptr) == nullptr && "not removed from space");
            cpShapeFree(ptr);
        }
    }

    Shape& operator=(const Shape&) = delete;
    Shape& operator=(Shape&& other) {
        if (this != &other) {
            ptr       = other.ptr;
            other.ptr = nullptr;
        }
        return *this;
    }

    /// support implicit cast to cpShape*
    inline operator cpShape*() const { return ptr; };

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
        Space space{};
        cpSpaceSetGravity(space, {0, 0});

        // add the space as a singleton component, note the use of std::move to
        // move the component as none of our components are copyable
        ecs.set<Space>(std::move(space));

        // add a system to step the physics space each frame
        ecs.system<>("step_space")
            .kind(flecs::PreUpdate)
            .iter([](flecs::iter &it) {
                auto *space = it.world().get_mut<Space>();
                log_debug("step space");
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
            {0,0}, {400,0}, 0)};
    cpShapeSetCollisionType(shape, CT_Ground);
    ecs.entity("ground").set<Shape>(std::move(shape));

    // create an apple to fall to the ground
    Body body{cpBodyNew(1, INFINITY)};
    cpBodySetPosition(body, {100, 100});
    shape = cpBoxShapeNew(body, 10, 10, 3);
    ecs.entity("apple")
        .set<Body>(std::move(body))
        .set<Shape>(std::move(shape));

    ecs.progress(1/60.0);
    {
        const Shape& ground = *ecs.entity("ground").get<Shape>();
        EXPECT_EQ(cpSegmentShapeGetA(ground), cpv(0,0));
        EXPECT_EQ(cpSegmentShapeGetA(ground), cpv(400,0));
    }
}
