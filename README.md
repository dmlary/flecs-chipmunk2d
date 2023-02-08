## flecs/Chipmunk2D integration examples

This repo contains a number of different ways to use the Chipmunk2D physics
library with the flecs ECS library in C++.

- flecs: https://github.com/SanderMertens/flecs
- Chipmunk2D: https://github.com/slembcke/Chipmunk2D

Key details:
* Chipmunk2D uses pointers internally to connect various structures together
* flecs does not currently have support for fixed address components
* flecs components for Chipmunk2D must manage some number of pointers to
    Chipmunk2D structs, as a result they will be move-only components

### Implementation Approaches
* simple struct
    * naive approach
    * 1:1 mapping of one flecs component wrapping one pointer to a Chipmunk2D
        type (`Body` wraps `struct cpBody *`)
    * single helper for implicit conversion from `Body` to `struct cpBody *`
    * no allocator call in component
    * destructor handles resource release
    * no templating or inheritance
* std::unique_ptr
    * component is `std::unique_ptr<T, TDeleter>`
    * custom destructors for each component type
    * `using Body = std::unique_ptr<cpBody, BodyDeleter>`
