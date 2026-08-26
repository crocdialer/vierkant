#include "vierkant/Scene.hpp"
#include <gtest/gtest.h>

using namespace vierkant;

namespace
{

//! a two-bone chain plus a non-identity skin-transform, the minimum a mirror needs
vierkant::MeshPtr create_skinned_mesh()
{
    auto mesh = vierkant::Mesh::create();

    auto root_bone = std::make_shared<nodes::node_t>();
    root_bone->name = "root_bone";
    root_bone->index = 0;
    root_bone->transform.translation = {1.f, 0.f, 0.f};

    auto child_bone = std::make_shared<nodes::node_t>();
    child_bone->name = "child_bone";
    child_bone->index = 1;
    child_bone->parent = root_bone;
    child_bone->transform.translation = {0.f, 2.f, 0.f};
    root_bone->children = {child_bone};

    mesh->root_bone = root_bone;
    mesh->skin_transform.translation = {0.f, 0.f, 10.f};
    mesh->node_animations.resize(1);
    mesh->node_animations[0].duration = 10.f;
    return mesh;
}

}// namespace

TEST(BoneMirror, create_mirrors_the_hierarchy)
{
    auto scene = vierkant::Scene::create();
    auto mesh_object = scene->create_mesh_object({create_skinned_mesh()});

    auto *mirror_root = scene->create_bone_mirror(mesh_object);
    ASSERT_TRUE(mirror_root);

    // the root is unindexed and carries the skin-transform, the bones below mirror the node-hierarchy
    ASSERT_TRUE(mirror_root->has_component<bone_component_t>());
    EXPECT_FALSE(mirror_root->get_component<bone_component_t>().index);
    EXPECT_EQ(mirror_root->transform()->translation, glm::vec3(0.f, 0.f, 10.f));

    ASSERT_EQ(mirror_root->children.size(), 1);
    const auto &root_bone = mirror_root->children.front();
    EXPECT_EQ(root_bone->name, "root_bone");
    EXPECT_EQ(root_bone->get_component<bone_component_t>().index, 0);

    ASSERT_EQ(root_bone->children.size(), 1);
    const auto &child_bone = root_bone->children.front();
    EXPECT_EQ(child_bone->name, "child_bone");
    EXPECT_EQ(child_bone->get_component<bone_component_t>().index, 1);

    // idempotent: a second call returns the existing mirror instead of adding another one
    EXPECT_EQ(scene->create_bone_mirror(mesh_object), mirror_root);
    EXPECT_EQ(mesh_object->children.size(), 1);
}

TEST(BoneMirror, create_without_skeleton)
{
    auto scene = vierkant::Scene::create();
    auto mesh_object = scene->create_mesh_object({vierkant::Mesh::create()});
    EXPECT_EQ(scene->create_bone_mirror(mesh_object), nullptr);
    EXPECT_TRUE(mesh_object->children.empty());
}

TEST(BoneMirror, update_drives_globals_and_attachments)
{
    auto scene = vierkant::Scene::create();
    auto mesh = create_skinned_mesh();

    // animate the root-bone, so the sampled key has to replace its own transform
    mesh->node_animations[0].keys[mesh->root_bone].positions[0.f] = {.value = {7.f, 8.f, 9.f}};

    auto mesh_object = scene->create_mesh_object({mesh});
    mesh_object->set_transform({.translation = {100.f, 0.f, 0.f}});
    scene->add_object(mesh_object);

    auto *mirror_root = scene->create_bone_mirror(mesh_object);
    ASSERT_TRUE(mirror_root);

    // an ordinary object attached to the deepest bone
    auto attachment = scene->create_object();
    attachment->set_transform({.translation = {0.f, 0.f, 1.f}});
    mirror_root->children.front()->children.front()->add_child(attachment);

    scene->update(0.0);

    // object * skin_transform, then the animated root-bone, then the unanimated child-bone
    EXPECT_EQ(mirror_root->global_transform().translation, glm::vec3(100.f, 0.f, 10.f));
    EXPECT_EQ(mirror_root->children.front()->global_transform().translation, glm::vec3(107.f, 8.f, 19.f));

    const auto &child_bone = mirror_root->children.front()->children.front();
    EXPECT_EQ(child_bone->global_transform().translation, glm::vec3(107.f, 10.f, 19.f));
    EXPECT_EQ(attachment->global_transform().translation, glm::vec3(107.f, 10.f, 20.f));

    // moving the mesh-object moves everything below it, the mirror included
    mesh_object->set_transform({.translation = {0.f, 0.f, 0.f}});
    scene->update(0.0);
    EXPECT_EQ(attachment->global_transform().translation, glm::vec3(7.f, 10.f, 20.f));
}
