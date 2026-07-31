#include <gtest/gtest.h>
#include <vierkant/Object3D.hpp>
#include <vierkant/Scene.hpp>
#include <vierkant/physics_context.hpp>

using namespace vierkant;
//____________________________________________________________________________//

CollisionShapeId create_collision_shape(PhysicsContext &context, const vierkant::GeometryPtr &geom, bool convex = true)
{
    Mesh::entry_create_info_t entry_create_info = {};
    entry_create_info.geometry = geom;
    vierkant::mesh_buffer_params_t buffer_params = {};
    // buffer_params.pack_vertices = true;

    CollisionShapeId shape_id = CollisionShapeId::nil();
    collision::mesh_t mesh_cpm = {};
    mesh_cpm.mesh_id = {};
    vierkant::mesh_asset_t mesh_asset = {};
    mesh_asset.bundle = vierkant::create_mesh_buffers({entry_create_info}, buffer_params);
    context.mesh_provider = [&mesh_asset](const vierkant::MeshId &mesh_id) { return &mesh_asset; };
    shape_id = convex ? context.create_convex_collision_shape(mesh_cpm) : context.create_collision_shape(mesh_cpm);
    EXPECT_TRUE(shape_id);
    return shape_id;
}

//! static box, its top-surface at y == 0
void create_ground(const std::shared_ptr<vierkant::ObjectStore> &object_store,
                   const std::shared_ptr<vierkant::PhysicsScene> &scene, const glm::vec3 &half_extents)
{
    auto ground = object_store->create_object();
    ground->transform = transform_t{.translation = {0.f, -half_extents.y, 0.f}};
    vierkant::physics_component_t cmp = {};
    cmp.shape = collision::box_t{.half_extents = half_extents};
    cmp.mass = 0.f;
    ground->add_component(cmp);
    scene->add_object(ground);
}

//! 1.8m character: 0.3 radius + 1.2 cylinder, shape_transform puts the origin at the feet
vierkant::Object3DPtr create_character(const std::shared_ptr<vierkant::ObjectStore> &object_store,
                                       const std::shared_ptr<vierkant::PhysicsScene> &scene,
                                       const glm::vec3 &position)
{
    constexpr float radius = .3f, cylinder_height = 1.2f;
    auto player = object_store->create_object();
    player->transform = transform_t{.translation = position};
    vierkant::physics_component_t cmp = {};
    cmp.shape = collision::capsule_t{.radius = radius, .height = cylinder_height};
    cmp.shape_transform = transform_t{.translation = {0.f, .5f * cylinder_height + radius, 0.f}};
    cmp.mass = 80.f;
    cmp.character = vierkant::character_t{};
    player->add_component(cmp);
    scene->add_object(player);
    return player;
}

TEST(PhysicsContext, collision_shapes)
{
    PhysicsContext context;
    auto box = Geometry::Box();
    EXPECT_TRUE(create_collision_shape(context, box, true));
    EXPECT_TRUE(create_collision_shape(context, box, false));
    EXPECT_TRUE(context.create_collision_shape(collision::plane_t()));
    EXPECT_TRUE(context.create_collision_shape(collision::box_t()));
    EXPECT_TRUE(context.create_collision_shape(collision::sphere_t()));
    EXPECT_TRUE(context.create_collision_shape(collision::cylinder_t()));
    EXPECT_TRUE(context.create_collision_shape(collision::capsule_t()));
}

TEST(PhysicsContext, add_remove_object)
{
    std::shared_ptr<vierkant::ObjectStore> object_store = vierkant::create_object_store();
    auto scene = vierkant::PhysicsScene::create(object_store);
    auto &context = scene->physics_context();

    glm::vec3 gravity = {0.f, -9.81f, 0.f};
    context.set_gravity(gravity);
    EXPECT_EQ(context.gravity(), gravity);

    // create object / init transform
    auto a = object_store->create_object();
    a->transform.emplace();

    // a does not (yet) have a vierkant::physics_component, so adding has no effect
    scene->add_object(a);
    EXPECT_FALSE(context.contains(a->id()));
    scene->remove_object(a);

    // now add required component
    vierkant::object_component auto &cmp = a->add_component<vierkant::physics_component_t>();
    cmp.shape = collision::box_t{glm::vec3(0.5f)};
    scene->physics_context().add_object(a->id(), *a->transform, cmp);

    EXPECT_TRUE(context.contains(a->id()));
    EXPECT_EQ(context.body_interface().velocity(a->id()), glm::vec3(0));
    auto test_velocity = glm::vec3(0, 1.f, 0.f);
    context.body_interface().set_velocity(a->id(), test_velocity);

    context.step_simulation(1.f / 60.f, 2);

    // TODO: fails, why?
    //    EXPECT_EQ(context.body_interface().velocity(a->id()), test_velocity);

    context.remove_object(a->id(), cmp);
    EXPECT_FALSE(context.contains(a->id()));
}

TEST(PhysicsContext, character)
{
    std::shared_ptr<vierkant::ObjectStore> object_store = vierkant::create_object_store();
    auto scene = vierkant::PhysicsScene::create(object_store);
    auto &context = scene->physics_context();
    context.set_gravity({0.f, -9.81f, 0.f});

    // static, concave triangle-mesh as ground, top-surface at y == 0.5
    constexpr float ground_height = .5f;
    auto ground = object_store->create_object();
    ground->transform.emplace();
    vierkant::physics_component_t ground_cmp = {};
    ground_cmp.shape = create_collision_shape(context, Geometry::Box(), false);
    ground_cmp.mass = 0.f;
    ground->add_component(ground_cmp);
    scene->add_object(ground);

    // 1.8m character: 0.3 radius + 1.2 cylinder, shape_transform puts the origin at the feet
    constexpr float radius = .3f, cylinder_height = 1.2f, start_height = 3.f;
    auto player = object_store->create_object();
    player->transform = transform_t{.translation = {0.f, start_height, 0.f}};
    vierkant::physics_component_t player_cmp = {};
    player_cmp.shape = collision::capsule_t{.radius = radius, .height = cylinder_height};
    player_cmp.shape_transform = transform_t{.translation = {0.f, .5f * cylinder_height + radius, 0.f}};
    player_cmp.mass = 80.f;
    player_cmp.character = vierkant::character_t{};
    player->add_component(player_cmp);
    scene->add_object(player);

    // the character-state is part of the physics-component, refreshed after each step
    const auto &character = *player->get_component<vierkant::physics_component_t>().character;

    // next update will pick up newly added objects
    scene->update(0.f);
    EXPECT_TRUE(context.contains(player->id()));
    EXPECT_EQ(character.ground_state, vierkant::GroundState::InAir);

    // reading the state of a non-character is a no-op, it does not clobber the passed-in state
    vierkant::character_t not_a_character = {.ground_state = vierkant::GroundState::OnGround};
    context.read_character_state(ground->id(), not_a_character);
    EXPECT_EQ(not_a_character.ground_state, vierkant::GroundState::OnGround);

    for(uint32_t i = 0; i < 200; ++i) { scene->update(1.f / 60.f); }

    // the character fell, its object-transform tracked the body via the existing readback
    EXPECT_LT(player->transform->translation.y, start_height);

    // ... and came to rest with its feet on the ground
    EXPECT_NEAR(player->transform->translation.y, ground_height, .05f);
    EXPECT_EQ(character.ground_state, vierkant::GroundState::OnGround);

    // rotation is locked -> the capsule stayed upright
    EXPECT_NEAR(std::abs(player->transform->rotation.w), 1.f, 1.e-5f);

    context.remove_object(player->id(), player_cmp);
    EXPECT_FALSE(context.contains(player->id()));
}

TEST(PhysicsContext, character_slope)
{
    std::shared_ptr<vierkant::ObjectStore> object_store = vierkant::create_object_store();
    auto scene = vierkant::PhysicsScene::create(object_store);
    auto &context = scene->physics_context();
    context.set_gravity({0.f, -9.81f, 0.f});

    // static ramp, tilted around x. its top-surface passes through the origin
    constexpr float ramp_angle = 40.f;
    auto ramp = object_store->create_object();
    glm::quat ramp_rotation = glm::angleAxis(glm::radians(ramp_angle), glm::vec3(1.f, 0.f, 0.f));
    ramp->transform = transform_t{.translation = ramp_rotation * glm::vec3(0.f, -.5f, 0.f), .rotation = ramp_rotation};
    vierkant::physics_component_t ramp_cmp = {};
    ramp_cmp.shape = collision::box_t{.half_extents = {50.f, .5f, 50.f}};
    ramp_cmp.mass = 0.f;
    ramp->add_component(ramp_cmp);
    scene->add_object(ramp);

    // 1.8m character standing on the ramp, slope-limit below the ramp's angle
    constexpr float radius = .3f, cylinder_height = 1.2f;
    auto player = object_store->create_object();
    player->transform = transform_t{.translation = {0.f, .1f, 0.f}};
    vierkant::physics_component_t player_cmp = {};
    player_cmp.shape = collision::capsule_t{.radius = radius, .height = cylinder_height};
    player_cmp.shape_transform = transform_t{.translation = {0.f, .5f * cylinder_height + radius, 0.f}};
    player_cmp.mass = 80.f;
    player_cmp.character = vierkant::character_t{.max_slope_angle = glm::radians(ramp_angle - 10.f)};
    player->add_component(player_cmp);
    scene->add_object(player);

    constexpr float dt = 1.f / 60.f;
    auto &phys_cmp = player->get_component<vierkant::physics_component_t>();

    // max_slope_angle reaches the body: the ramp is too steep to stand on
    for(uint32_t i = 0; i < 60; ++i) { scene->update(dt); }
    EXPECT_EQ(phys_cmp.character->ground_state, vierkant::GroundState::OnSteepGround);

    // raising the limit above the ramp's angle is a creation-time change -> rebuild the body
    phys_cmp.character->max_slope_angle = glm::radians(ramp_angle + 10.f);
    phys_cmp.mode = vierkant::physics_component_t::UPDATE;

    for(uint32_t i = 0; i < 60; ++i) { scene->update(dt); }
    EXPECT_EQ(phys_cmp.character->ground_state, vierkant::GroundState::OnGround);
}

TEST(PhysicsContext, player_move)
{
    std::shared_ptr<vierkant::ObjectStore> object_store = vierkant::create_object_store();
    auto scene = vierkant::PhysicsScene::create(object_store);
    auto &context = scene->physics_context();
    context.set_gravity({0.f, -9.81f, 0.f});

    // static box as ground, top-surface at y == 0
    auto ground = object_store->create_object();
    ground->transform = transform_t{.translation = {0.f, -.5f, 0.f}};
    vierkant::physics_component_t ground_cmp = {};
    ground_cmp.shape = collision::box_t{.half_extents = {50.f, .5f, 50.f}};
    ground_cmp.mass = 0.f;
    ground->add_component(ground_cmp);
    scene->add_object(ground);

    // 1.8m character: 0.3 radius + 1.2 cylinder, shape_transform puts the origin at the feet
    constexpr float radius = .3f, cylinder_height = 1.2f;
    auto player = object_store->create_object();
    player->transform = transform_t{.translation = {0.f, .1f, 0.f}};
    vierkant::physics_component_t player_cmp = {};
    player_cmp.shape = collision::capsule_t{.radius = radius, .height = cylinder_height};
    player_cmp.shape_transform = transform_t{.translation = {0.f, .5f * cylinder_height + radius, 0.f}};
    player_cmp.mass = 80.f;
    player_cmp.character = vierkant::character_t{};
    player->add_component(player_cmp);
    scene->add_object(player);

    constexpr float dt = 1.f / 60.f;
    auto &input = *player->get_component<vierkant::physics_component_t>().character;

    // let the capsule settle on the ground
    for(uint32_t i = 0; i < 60; ++i) { scene->update(dt); }
    ASSERT_EQ(input.ground_state, vierkant::GroundState::OnGround);
    const glm::vec3 rest_position = player->transform->translation;

    // no input -> the tracker holds the position
    for(uint32_t i = 0; i < 60; ++i) { scene->update(dt); }
    EXPECT_NEAR(glm::length(player->transform->translation - rest_position), 0.f, .01f);

    // the top-speed the tracker settles at is short of max_speed by the ground-friction it has to
    // balance: mu * g * t_accel_ground ~ .16 m/s at the defaults. P4 removes it via friction == 0.
    constexpr float speed_eps = .25f;

    // full forward-input, yaw == 0 -> movement along -z at max_speed
    input.move = {0.f, 1.f};
    for(uint32_t i = 0; i < 120; ++i) { scene->update(dt); }
    glm::vec3 velocity = context.body_interface().velocity(player->id());
    EXPECT_NEAR(velocity.z, -input.max_speed, speed_eps);
    EXPECT_NEAR(velocity.x, 0.f, .01f);
    EXPECT_LT(player->transform->translation.z, rest_position.z - 1.f);

    // ... yaw rotates the movement-basis, 90 degrees puts 'forward' onto -x
    input.yaw = glm::half_pi<float>();
    for(uint32_t i = 0; i < 120; ++i) { scene->update(dt); }
    velocity = context.body_interface().velocity(player->id());
    EXPECT_NEAR(velocity.x, -input.max_speed, speed_eps);

    // input beyond the unit-disc is clamped, not scaled up
    input.move = {0.f, 10.f};
    for(uint32_t i = 0; i < 120; ++i) { scene->update(dt); }
    EXPECT_NEAR(context.body_interface().velocity(player->id()).x, -input.max_speed, speed_eps);

    // releasing the input brakes to a standstill
    input.move = {};
    for(uint32_t i = 0; i < 60; ++i) { scene->update(dt); }
    EXPECT_NEAR(glm::length(context.body_interface().velocity(player->id())), 0.f, .05f);

    // deliberately no remove_object here: the scene is destroyed with a live character
}

TEST(PhysicsContext, character_jump)
{
    std::shared_ptr<vierkant::ObjectStore> object_store = vierkant::create_object_store();
    auto scene = vierkant::PhysicsScene::create(object_store);
    auto &context = scene->physics_context();
    context.set_gravity({0.f, -9.81f, 0.f});
    create_ground(object_store, scene, {50.f, .5f, 50.f});
    auto player = create_character(object_store, scene, {0.f, .1f, 0.f});
    auto &input = *player->get_component<vierkant::physics_component_t>().character;

    constexpr float dt = 1.f / 60.f;
    for(uint32_t i = 0; i < 60; ++i) { scene->update(dt); }
    ASSERT_EQ(input.ground_state, vierkant::GroundState::OnGround);
    const float rest_height = player->transform->translation.y;

    // a jump from standing reaches jump_height and lands again
    input.jump = true;
    float apex = rest_height;
    for(uint32_t i = 0; i < 120; ++i)
    {
        scene->update(dt);
        apex = std::max(apex, player->transform->translation.y);
    }
    EXPECT_NEAR(apex - rest_height, input.jump_height, .1f);
    EXPECT_NEAR(player->transform->translation.y, rest_height, .05f);
    EXPECT_EQ(input.ground_state, vierkant::GroundState::OnGround);

    // jump again, then press mid-air, well past the coyote-window
    input.jump = true;
    for(uint32_t i = 0; i < 20; ++i) { scene->update(dt); }
    ASSERT_NE(input.ground_state, vierkant::GroundState::OnGround);
    float velocity_y = context.body_interface().velocity(player->id()).y;

    // ... which is ignored: gravity keeps pulling, there is no second impulse
    input.jump = true;
    scene->update(dt);
    EXPECT_LT(context.body_interface().velocity(player->id()).y, velocity_y);
}

TEST(PhysicsContext, character_coyote_time)
{
    constexpr float dt = 1.f / 60.f;

    // walk off a narrow platform, wait, then jump. returns the vertical velocity right afterwards
    auto jump_after_leaving_ground = [](float wait) {
        std::shared_ptr<vierkant::ObjectStore> object_store = vierkant::create_object_store();
        auto scene = vierkant::PhysicsScene::create(object_store);
        scene->physics_context().set_gravity({0.f, -9.81f, 0.f});
        create_ground(object_store, scene, {2.f, .5f, 2.f});
        auto player = create_character(object_store, scene, {0.f, .1f, 0.f});
        auto &input = *player->get_component<vierkant::physics_component_t>().character;

        for(uint32_t i = 0; i < 60; ++i) { scene->update(dt); }
        EXPECT_EQ(input.ground_state, vierkant::GroundState::OnGround);

        // forward until the platform-edge is passed
        input.move = {0.f, 1.f};
        uint32_t steps = 0;
        while(input.ground_state == vierkant::GroundState::OnGround && steps++ < 600) { scene->update(dt); }
        EXPECT_LT(steps, 600);

        for(float t = 0.f; t < wait; t += dt) { scene->update(dt); }
        input.jump = true;
        scene->update(dt);
        return scene->physics_context().body_interface().velocity(player->id()).y;
    };

    // just after leaving the ground the jump is still accepted: sqrt(2 * g * jump_height), minus one step of gravity
    EXPECT_NEAR(jump_after_leaving_ground(0.f), std::sqrt(2.f * 9.81f * 1.1f) - 9.81f * dt, .1f);

    // ... and is gone once the window has passed
    EXPECT_LT(jump_after_leaving_ground(.2f), 0.f);
}

TEST(PhysicsContext, simulation)
{
    //    spdlog::set_level(spdlog::level::debug);

    std::shared_ptr<vierkant::ObjectStore> object_store = vierkant::create_object_store();
    auto scene = vierkant::PhysicsScene::create(object_store);
    auto box = Geometry::Box();
    auto collision_shape = create_collision_shape(scene->physics_context(), box, true);

    Object3DPtr a(object_store->create_object()), b(object_store->create_object()), c(object_store->create_object()),
            ground(object_store->create_object());

    auto &body_interface = scene->physics_context().body_interface();

    std::map<uint32_t, uint32_t> contact_map, sensor_map;

    std::mutex mutex;

    vierkant::physics_component_t phys_cmp = {};
    phys_cmp.mass = 1.f;
    phys_cmp.shape = collision_shape;

    vierkant::PhysicsContext::callbacks_t callbacks;
    callbacks.contact_begin = [&contact_map, &mutex](uint32_t obj1, uint32_t obj2) {
        std::unique_lock lock(mutex);
        spdlog::debug("contact_begin: {}", obj1);
        contact_map[obj1]++;
    };
    callbacks.contact_end = [&contact_map, &mutex](uint32_t obj1, uint32_t obj2) {
        std::unique_lock lock(mutex);
        spdlog::debug("contact_end: {}", obj1);
        contact_map[obj1]--;
    };
    a->add_component(phys_cmp);
    b->add_component(phys_cmp);

    // add c as static body with zero mass
    phys_cmp.mass = 0.f;
    c->add_component(phys_cmp);

    phys_cmp.shape = vierkant::collision::box_t{.half_extents = {2.f, .2f, 2.f}};
    ground->add_component(phys_cmp);

    Object3DPtr objects[] = {ground, a, b, c};
    float i = 0;
    for(const auto &obj: objects)
    {
        obj->transform.emplace();
        obj->transform->translation.y = i++ * 5.f;
        scene->add_object(obj);
        scene->physics_context().set_callbacks(obj->id(), callbacks);

        // will be added after an update
        EXPECT_FALSE(scene->physics_context().contains(obj->id()));
    }

    // next update will pick up newly added objects
    scene->update(0.f);

    for(const auto &obj: objects)
    {
        // will be added after an update
        EXPECT_TRUE(scene->physics_context().contains(obj->id()));
    }

    auto sensor = object_store->create_object();
    sensor->name = "sensor";
    sensor->transform = transform_t{.translation = {0.f, 3.f, 0.f}};
    phys_cmp.sensor = true;
    phys_cmp.kinematic = true;
    phys_cmp.shape = collision::box_t{glm::vec3(4.f, 0.5f, 4.f)};
    sensor->add_component(phys_cmp);
    scene->add_object(sensor);
    scene->physics_context().set_callbacks(sensor->id(), callbacks);

    auto tground = ground->transform;
    auto ta = a->transform;
    auto tb = b->transform;
    auto tc = c->transform;

    // run simulation a bit
    for(uint32_t l = 0; l < 50; ++l) { scene->update(1.f / 60.f); }

    EXPECT_NE(body_interface.velocity(a->id()), glm::vec3(0));

    // bodies should be pulled down some way
    EXPECT_NE(ta, a->transform);
    EXPECT_NE(tb, b->transform);

    // ground and c were static and did not move
    EXPECT_EQ(tc, c->transform);
    EXPECT_EQ(tground, ground->transform);

    // remove object, again keep track of transforms
    scene->remove_object(b);
    ta = a->transform;
    tb = b->transform;

    // again, run simulation a bit
    for(uint32_t l = 0; l < 50; ++l)
    {
        scene->update(1.f / 60.f);
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

    // b was removed, transform should still be the same
    EXPECT_NE(ta, a->transform);
    EXPECT_EQ(tb, b->transform);

    // check if a and ground have contacts
    EXPECT_TRUE(contact_map[a->id()]);
    EXPECT_TRUE(contact_map[ground->id()]);

    // c was floating, -> no contacts ever
    EXPECT_FALSE(contact_map.contains(c->id()));

    // b got removed
    //    EXPECT_TRUE(contact_map.contains(b->id()));
    //    EXPECT_TRUE(!contact_map[b->id()]);

    // sensor was passed -> no contacts now, but there were some
    EXPECT_TRUE(contact_map.contains(sensor->id()));
    EXPECT_TRUE(!contact_map[sensor->id()]);

    //    auto debug_lines = context.debug_render();
    //    EXPECT_FALSE(debug_lines->positions.empty());
    //    EXPECT_EQ(debug_lines->positions.size(), debug_lines->colors.size());
}
