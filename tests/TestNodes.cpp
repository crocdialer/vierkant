#include "vierkant/nodes.hpp"
#include <gtest/gtest.h>

using namespace vierkant;

namespace
{

//! a two-bone chain: root -> child, both with a non-identity local transform and offset
struct test_hierarchy_t
{
    nodes::NodePtr root, child;
};

test_hierarchy_t create_test_hierarchy()
{
    test_hierarchy_t ret;
    ret.root = std::make_shared<nodes::node_t>();
    ret.root->name = "root";
    ret.root->index = 0;
    ret.root->transform.translation = {1.f, 0.f, 0.f};
    ret.root->offset.translation = {-100.f, 0.f, 0.f};

    ret.child = std::make_shared<nodes::node_t>();
    ret.child->name = "child";
    ret.child->index = 1;
    ret.child->parent = ret.root;
    ret.child->transform.translation = {0.f, 2.f, 0.f};
    ret.child->offset.translation = {0.f, -100.f, 0.f};
    ret.root->children = {ret.child};
    return ret;
}

}// namespace

TEST(Nodes, build_local_transforms_bfs_without_animation)
{
    auto hierarchy = create_test_hierarchy();

    std::vector<transform_t> transforms;
    nodes::build_local_transforms_bfs(hierarchy.root, {}, 0.f, transforms);

    ASSERT_EQ(transforms.size(), 2);

    // local transforms are returned as-is: neither accumulated along the hierarchy nor offset-combined
    EXPECT_EQ(transforms[0].translation, hierarchy.root->transform.translation);
    EXPECT_EQ(transforms[1].translation, hierarchy.child->transform.translation);
}

TEST(Nodes, build_local_transforms_bfs_animated_node_stays_local)
{
    auto hierarchy = create_test_hierarchy();

    // animate the root only
    nodes::node_animation_t animation = {};
    animation.keys[hierarchy.root].positions[0.f] = {.value = {7.f, 8.f, 9.f}};

    std::vector<transform_t> transforms;
    nodes::build_local_transforms_bfs(hierarchy.root, animation, 0.f, transforms);

    ASSERT_EQ(transforms.size(), 2);

    // sampled keys replace the node's own transform ...
    EXPECT_EQ(transforms[0].translation, glm::vec3(7.f, 8.f, 9.f));

    // ... and do not propagate to the child, in contrast to build_node_matrices_bfs
    EXPECT_EQ(transforms[1].translation, hierarchy.child->transform.translation);
}

TEST(Nodes, build_node_matrices_bfs_accumulates_and_applies_offset)
{
    auto hierarchy = create_test_hierarchy();

    std::vector<transform_t> matrices;
    nodes::build_node_matrices_bfs(hierarchy.root, {}, 0.f, matrices);

    ASSERT_EQ(matrices.size(), 2);

    // the existing routine yields skinning-matrices: global * offset
    EXPECT_EQ(matrices[0].translation, glm::vec3(1.f, 0.f, 0.f) + glm::vec3(-100.f, 0.f, 0.f));
    EXPECT_EQ(matrices[1].translation, glm::vec3(1.f, 2.f, 0.f) + glm::vec3(0.f, -100.f, 0.f));
}

TEST(Nodes, num_nodes_and_lookup)
{
    auto hierarchy = create_test_hierarchy();
    EXPECT_EQ(nodes::num_nodes_in_hierarchy(hierarchy.root), 2);
    EXPECT_EQ(nodes::node_by_name(hierarchy.root, "child"), hierarchy.child);
    EXPECT_EQ(nodes::node_by_name(hierarchy.root, "nope"), nullptr);
}
