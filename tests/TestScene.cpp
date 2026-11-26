#include <gtest/gtest.h>
#include <vierkant/Scene.hpp>

#include <cereal/archives/json.hpp>
#include <serialization/components.hpp>

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
    auto emitter = create_emitter(scene, {.burst = 3, .lifetime = 1.0});
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
    emitter_component_t emitter = {.rate = 2.5, .burst = 25, .extents = {1.f, 2.f, 3.f}, .seed = 42, .lifetime = 4.0};
    emitter.emit();
    emitter.accumulator = .5;
    emitter.rng = std::mt19937(1);

    auto loaded = json_roundtrip(emitter);
    EXPECT_EQ(loaded.rate, emitter.rate);
    EXPECT_EQ(loaded.burst, emitter.burst);
    EXPECT_EQ(loaded.extents, emitter.extents);
    EXPECT_EQ(loaded.seed, emitter.seed);
    EXPECT_EQ(loaded.lifetime, emitter.lifetime);
    EXPECT_EQ(loaded.pending, 0);
    EXPECT_EQ(loaded.accumulator, 0.0);
    EXPECT_FALSE(loaded.rng);

    // lifetime is optional
    emitter.lifetime = {};
    EXPECT_FALSE(json_roundtrip(emitter).lifetime);
}
