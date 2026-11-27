#include <gtest/gtest.h>
#include <vierkant/Scene.hpp>

#include <cereal/archives/json.hpp>
#include <serialization/components.hpp>
#include <serialization/optional_nvp_json.hpp>
#include <vierkant/physics_context.hpp>

using namespace vierkant;
//____________________________________________________________________________//

TEST(Scene, lifetime_removes_object)
{
    auto scene = vierkant::Scene::create();
    auto obj = scene->create_object();
    obj->add_component<lifetime_component_t>({.remaining = 1.0});
    scene->add_object(obj);

    scene->update(0.5);
    EXPECT_EQ(obj->parent(), scene->root().get());
    EXPECT_DOUBLE_EQ(obj->get_component<lifetime_component_t>().remaining, 0.5);

    scene->update(0.5);
    EXPECT_EQ(obj->parent(), nullptr);
    EXPECT_TRUE(scene->root()->children.empty());
}

TEST(Scene, lifetime_scaled_by_animation_speed)
{
    auto scene = vierkant::Scene::create();
    auto obj = scene->create_object();
    obj->add_component<lifetime_component_t>({.remaining = 1.0});
    scene->add_object(obj);

    // paused
    scene->animation_speed = 0.0;
    scene->update(10.0);
    EXPECT_EQ(obj->parent(), scene->root().get());

    scene->animation_speed = 2.0;
    scene->update(0.5);
    EXPECT_EQ(obj->parent(), nullptr);
}

TEST(Scene, lifetime_paused_while_disabled)
{
    auto scene = vierkant::Scene::create();

    // a disabled object, and an enabled child below a disabled parent
    auto disabled = scene->create_object();
    disabled->enabled = false;
    disabled->add_component<lifetime_component_t>({.remaining = 1.0});

    auto parent = scene->create_object();
    parent->enabled = false;
    auto child = scene->create_object();
    child->add_component<lifetime_component_t>({.remaining = 1.0});
    parent->add_child(child);

    scene->add_object(disabled);
    scene->add_object(parent);

    scene->update(10.0);
    EXPECT_EQ(disabled->parent(), scene->root().get());
    EXPECT_EQ(child->parent(), parent.get());
    EXPECT_DOUBLE_EQ(disabled->get_component<lifetime_component_t>().remaining, 1.0);
    EXPECT_DOUBLE_EQ(child->get_component<lifetime_component_t>().remaining, 1.0);

    disabled->enabled = true;
    parent->enabled = true;
    scene->update(1.0);
    EXPECT_EQ(disabled->parent(), nullptr);
    EXPECT_EQ(child->parent(), nullptr);
    EXPECT_EQ(parent->parent(), scene->root().get());
}

TEST(Scene, lifetime_removes_subtree)
{
    auto scene = vierkant::Scene::create();

    // parent and child expire in the same frame, the subtree leaves as one
    auto parent = scene->create_object();
    parent->add_component<lifetime_component_t>({.remaining = 1.0});
    auto child = scene->create_object();
    child->add_component<lifetime_component_t>({.remaining = 1.0});
    parent->add_child(child);
    scene->add_object(parent);

    scene->update(1.0);
    EXPECT_EQ(parent->parent(), nullptr);
    EXPECT_EQ(child->parent(), parent.get());
    EXPECT_TRUE(scene->root()->children.empty());
}

namespace
{

//! an emitter with one disabled template-child, added to the scene
Object3DPtr create_emitter(const vierkant::ScenePtr &scene, const emitter_component_t &params)
{
    auto emitter = scene->create_object();
    emitter->name = "emitter";
    emitter->set_transform({.translation = {0.f, 10.f, 0.f}});
    emitter->add_component(params);

    auto tmpl = scene->create_object();
    tmpl->name = "template";
    tmpl->enabled = false;
    tmpl->set_transform({.translation = {1.f, 0.f, 0.f}});
    emitter->add_child(tmpl);

    scene->add_object(emitter);
    return emitter;
}

//! objects next to the emitter, i.e. spawned clones
std::vector<Object3DPtr> spawned(const vierkant::ScenePtr &scene, const Object3DPtr &emitter)
{
    std::vector<Object3DPtr> ret;
    for(const auto &obj: scene->root()->children)
    {
        if(obj != emitter) { ret.push_back(obj); }
    }
    return ret;
}

}// namespace

TEST(Scene, emitter_burst)
{
    auto scene = vierkant::Scene::create();
    auto emitter = create_emitter(scene, {.burst = 5, .extents = glm::vec3(.5f)});

    // nothing without a trigger
    scene->update(1.0);
    EXPECT_TRUE(spawned(scene, emitter).empty());

    emitter->get_component<emitter_component_t>().emit();
    scene->update(0.0);
    auto clones = spawned(scene, emitter);
    ASSERT_EQ(clones.size(), 5);

    // template stays, clones are enabled regular objects around the template's position
    ASSERT_EQ(emitter->children.size(), 1);
    EXPECT_FALSE(emitter->children.front()->enabled);
    for(const auto &clone: clones)
    {
        EXPECT_TRUE(clone->enabled);
        EXPECT_EQ(clone->name, "template");
        const auto pos = clone->global_transform().translation;
        EXPECT_LE(glm::abs(pos.x - 1.f), .5f);
        EXPECT_LE(glm::abs(pos.y - 10.f), .5f);
        EXPECT_LE(glm::abs(pos.z), .5f);
    }

    // one burst per trigger
    scene->update(1.0);
    EXPECT_EQ(spawned(scene, emitter).size(), 5);
}

TEST(Scene, emitter_rate)
{
    auto scene = vierkant::Scene::create();
    auto emitter = create_emitter(scene, {.rate = 10.0});

    // 2.5 spawns per update, fractions carry over
    const uint32_t expected[] = {2, 5, 7, 10};
    for(uint32_t num: expected)
    {
        scene->update(0.25);
        EXPECT_EQ(spawned(scene, emitter).size(), num);
    }

    // paused
    scene->animation_speed = 0.0;
    scene->update(1.0);
    EXPECT_EQ(spawned(scene, emitter).size(), 10);

    // disabled
    scene->animation_speed = 1.0;
    emitter->enabled = false;
    scene->update(1.0);
    EXPECT_EQ(spawned(scene, emitter).size(), 10);
}

TEST(Scene, emitter_seed)
{
    auto spawn_positions = [](uint32_t seed) {
        auto scene = vierkant::Scene::create();
        auto emitter = create_emitter(scene, {.burst = 8, .extents = glm::vec3(1.f), .seed = seed});
        emitter->get_component<emitter_component_t>().emit();
        scene->update(0.0);

        std::vector<glm::vec3> ret;
        for(const auto &clone: spawned(scene, emitter)) { ret.push_back(clone->global_transform().translation); }
        return ret;
    };
    EXPECT_EQ(spawn_positions(1), spawn_positions(1));
    EXPECT_NE(spawn_positions(1), spawn_positions(2));
}

TEST(Scene, emitter_lifetime)
{
    auto scene = vierkant::Scene::create();
    auto emitter = create_emitter(scene, {.burst = 3, .lifetime = glm::vec2(1.f)});
    emitter->get_component<emitter_component_t>().emit();

    scene->update(0.0);
    auto clones = spawned(scene, emitter);
    ASSERT_EQ(clones.size(), 3);
    for(const auto &clone: clones) { EXPECT_DOUBLE_EQ(clone->get_component<lifetime_component_t>().remaining, 1.0); }

    // template does not carry it
    EXPECT_FALSE(emitter->children.front()->has_component<lifetime_component_t>());

    scene->update(1.0);
    EXPECT_TRUE(spawned(scene, emitter).empty());
}

TEST(Scene, emitter_lifetime_range)
{
    auto scene = vierkant::Scene::create();
    auto emitter = create_emitter(scene, {.burst = 20, .lifetime = glm::vec2(1.f, 3.f)});
    emitter->get_component<emitter_component_t>().emit();

    scene->update(0.0);
    auto clones = spawned(scene, emitter);
    ASSERT_EQ(clones.size(), 20);

    std::set<double> lifetimes;
    for(const auto &clone: clones)
    {
        double remaining = clone->get_component<lifetime_component_t>().remaining;
        EXPECT_GE(remaining, 1.0);
        EXPECT_LE(remaining, 3.0);
        lifetimes.insert(remaining);
    }
    // drawn per clone, not one value for all
    EXPECT_GT(lifetimes.size(), 1);
}

TEST(Scene, emitter_multiple_templates)
{
    auto scene = vierkant::Scene::create();
    auto emitter = create_emitter(scene, {.burst = 64});

    auto other = scene->create_object();
    other->name = "other";
    other->enabled = false;
    emitter->add_child(other);

    // an enabled child is not a template
    auto enabled_child = scene->create_object();
    enabled_child->name = "enabled";
    emitter->add_child(enabled_child);

    emitter->get_component<emitter_component_t>().emit();
    scene->update(0.0);

    std::map<std::string, uint32_t> counts;
    for(const auto &clone: spawned(scene, emitter)) { counts[clone->name]++; }
    EXPECT_GT(counts["template"], 0);
    EXPECT_GT(counts["other"], 0);
    EXPECT_EQ(counts["template"] + counts["other"], 64);
}

namespace
{

//! JSON round-trip for a component
template<typename T>
T json_roundtrip(const T &in)
{
    std::stringstream ss;
    {
        cereal::JSONOutputArchive archive(ss);
        archive(cereal::make_nvp("cmp", in));
    }
    T out = {};
    {
        cereal::JSONInputArchive archive(ss);
        archive(cereal::make_nvp("cmp", out));
    }
    return out;
}

}// namespace

TEST(Scene, emitter_lifetime_serialization)
{
    // a lifetime resumes where it left off
    auto scene = vierkant::Scene::create();
    auto obj = scene->create_object();
    obj->add_component<lifetime_component_t>({.remaining = 1.0});
    scene->add_object(obj);
    scene->update(0.75);
    EXPECT_DOUBLE_EQ(json_roundtrip(obj->get_component<lifetime_component_t>()).remaining, 0.25);

    // parameters persist, runtime-state does not
    emitter_component_t emitter = {.rate = 2.5,
                                   .burst = 25,
                                   .extents = {1.f, 2.f, 3.f},
                                   .seed = 42,
                                   .lifetime = glm::vec2(4.f),
                                   .speed = glm::vec2(1.f, 2.f),
                                   .direction = {1.f, 0.f, 0.f},
                                   .cone_angle = .5f,
                                   .angular_speed = glm::vec2(3.f, 4.f)};
    emitter.emit();
    emitter.accumulator = .5;
    emitter.rng = std::mt19937(1);

    auto loaded = json_roundtrip(emitter);
    EXPECT_EQ(loaded.rate, emitter.rate);
    EXPECT_EQ(loaded.burst, emitter.burst);
    EXPECT_EQ(loaded.extents, emitter.extents);
    EXPECT_EQ(loaded.seed, emitter.seed);
    EXPECT_EQ(loaded.lifetime, emitter.lifetime);
    EXPECT_EQ(loaded.speed, emitter.speed);
    EXPECT_EQ(loaded.direction, emitter.direction);
    EXPECT_EQ(loaded.cone_angle, emitter.cone_angle);
    EXPECT_EQ(loaded.angular_speed, emitter.angular_speed);
    EXPECT_EQ(loaded.pending, 0);
    EXPECT_EQ(loaded.accumulator, 0.0);
    EXPECT_FALSE(loaded.rng);

    // lifetime is optional
    emitter.lifetime = {};
    EXPECT_FALSE(json_roundtrip(emitter).lifetime);

    // a file written before the velocity-fields loads with their defaults
    std::stringstream ss;
    {
        cereal::JSONOutputArchive archive(ss);
        archive(cereal::make_nvp("cmp", emitter));
    }
    CEREAL_RAPIDJSON_NAMESPACE::Document doc;
    doc.Parse(ss.str().c_str());
    for(const char *key: {"speed", "direction", "cone_angle", "angular_speed"})
    {
        ASSERT_TRUE(doc["cmp"].RemoveMember(key)) << key;
    }
    CEREAL_RAPIDJSON_NAMESPACE::StringBuffer buffer;
    CEREAL_RAPIDJSON_NAMESPACE::Writer<CEREAL_RAPIDJSON_NAMESPACE::StringBuffer> writer(buffer);
    doc.Accept(writer);

    std::stringstream old_file(buffer.GetString());
    emitter_component_t old_loaded = {};
    old_loaded.direction = {};
    {
        cereal::JSONInputArchive archive(old_file);
        archive(cereal::make_nvp("cmp", old_loaded));
    }
    EXPECT_EQ(old_loaded.rate, emitter.rate);
    EXPECT_FALSE(old_loaded.speed);
    EXPECT_EQ(old_loaded.direction, glm::vec3(0.f, 1.f, 0.f));
    EXPECT_EQ(old_loaded.cone_angle, 0.f);
    EXPECT_FALSE(old_loaded.angular_speed);
}

namespace
{

//! emitter with one physics-template, its children[0]
Object3DPtr create_physics_emitter(const vierkant::ScenePtr &scene, const emitter_component_t &params)
{
    auto emitter = create_emitter(scene, params);
    emitter->children.front()->add_component<physics_component_t>({.mass = 1.f});
    emitter->get_component<emitter_component_t>().emit();
    return emitter;
}

//! angle between two non-zero vectors
float angle(const glm::vec3 &a, const glm::vec3 &b)
{ return std::acos(glm::clamp(glm::dot(glm::normalize(a), glm::normalize(b)), -1.f, 1.f)); }

}// namespace

TEST(Scene, emitter_velocity_cone)
{
    constexpr float cone_angle = .3f;
    auto scene = vierkant::Scene::create();
    auto emitter = create_physics_emitter(
            scene, {.burst = 50, .speed = glm::vec2(2.f, 4.f), .direction = {2.f, 0.f, 0.f}, .cone_angle = cone_angle});

    // the emitter's rotation aims the cone: local +x is world +y
    emitter->set_transform({.translation = {0.f, 10.f, 0.f},
                            .rotation = glm::angleAxis(glm::half_pi<float>(), glm::vec3(0.f, 0.f, 1.f))});

    // the template's own velocity is kept, a child-body gets the same velocity
    const glm::vec3 template_velocity = {0.f, 0.f, 5.f};
    auto tmpl = emitter->children.front();
    tmpl->get_component<physics_component_t>().linear_velocity = template_velocity;
    auto tmpl_child = scene->create_object();
    tmpl_child->add_component<physics_component_t>({.mass = 1.f, .linear_velocity = template_velocity});
    tmpl->add_child(tmpl_child);

    scene->update(0.0);
    auto clones = spawned(scene, emitter);
    ASSERT_EQ(clones.size(), 50);

    float max_angle = 0.f;
    for(const auto &clone: clones)
    {
        const auto &phys_cmp = clone->get_component<physics_component_t>();
        const glm::vec3 v = phys_cmp.linear_velocity - template_velocity;
        EXPECT_GE(glm::length(v), 2.f - 1.e-5f);
        EXPECT_LE(glm::length(v), 4.f + 1.e-5f);
        EXPECT_LE(angle(v, {0.f, 1.f, 0.f}), cone_angle + 1.e-4f);
        max_angle = std::max(max_angle, angle(v, {0.f, 1.f, 0.f}));
        EXPECT_EQ(phys_cmp.angular_velocity, glm::vec3(0.f));
        EXPECT_EQ(clone->children.front()->get_component<physics_component_t>().linear_velocity,
                  phys_cmp.linear_velocity);
    }
    // spread over the cone, not only the axis
    EXPECT_GT(max_angle, .5f * cone_angle);
    EXPECT_EQ(tmpl->get_component<physics_component_t>().linear_velocity, template_velocity);
}

TEST(Scene, emitter_velocity_cone_uniform)
{
    // uniform on the cap: for a hemisphere the mean cosine to the axis is 1/2. uniform angles give 2/pi
    auto scene = vierkant::Scene::create();
    auto emitter =
            create_physics_emitter(scene, {.burst = 500, .speed = glm::vec2(1.f), .cone_angle = glm::half_pi<float>()});
    scene->update(0.0);

    float sum_cos = 0.f;
    for(const auto &clone: spawned(scene, emitter))
    {
        sum_cos += glm::normalize(clone->get_component<physics_component_t>().linear_velocity).y;
    }
    EXPECT_NEAR(sum_cos / 500.f, .5f, .05f);
}

TEST(Scene, emitter_velocity_jet)
{
    auto scene = vierkant::Scene::create();
    auto emitter = create_physics_emitter(scene, {.burst = 5, .speed = glm::vec2(3.f), .direction = {0.f, 0.f, -1.f}});
    scene->update(0.0);

    for(const auto &clone: spawned(scene, emitter))
    {
        const glm::vec3 v = clone->get_component<physics_component_t>().linear_velocity;
        EXPECT_NEAR(glm::distance(v, glm::vec3(0.f, 0.f, -3.f)), 0.f, 1.e-5f);
    }
}

TEST(Scene, emitter_angular_speed)
{
    auto scene = vierkant::Scene::create();
    auto emitter = create_physics_emitter(scene, {.burst = 50, .angular_speed = glm::vec2(1.f, 2.f)});
    scene->update(0.0);

    auto clones = spawned(scene, emitter);
    ASSERT_EQ(clones.size(), 50);
    const glm::vec3 first_axis = clones.front()->get_component<physics_component_t>().angular_velocity;
    float max_angle = 0.f;
    for(const auto &clone: clones)
    {
        const auto &phys_cmp = clone->get_component<physics_component_t>();
        EXPECT_GE(glm::length(phys_cmp.angular_velocity), 1.f - 1.e-5f);
        EXPECT_LE(glm::length(phys_cmp.angular_velocity), 2.f + 1.e-5f);
        EXPECT_EQ(phys_cmp.linear_velocity, glm::vec3(0.f));
        max_angle = std::max(max_angle, angle(phys_cmp.angular_velocity, first_axis));
    }
    // random axes, not one for all
    EXPECT_GT(max_angle, glm::half_pi<float>());
}
