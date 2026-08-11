#include <gtest/gtest.h>
#include <random>
#include <spdlog/spdlog.h>
#include <vierkant/octahedral_map.hpp>

using namespace vierkant;
//____________________________________________________________________________//

namespace
{

//! deterministic unit-directions
std::vector<glm::vec3> random_directions(size_t num, uint32_t seed = 0x5eed)
{
    std::mt19937 rng(seed);
    std::uniform_real_distribution<float> dist(-1.f, 1.f);
    std::vector<glm::vec3> ret;
    ret.reserve(num);

    while(ret.size() < num)
    {
        glm::vec3 v(dist(rng), dist(rng), dist(rng));
        float len2 = glm::length2(v);
        if(len2 > 1e-4f && len2 <= 1.f) { ret.push_back(v / std::sqrt(len2)); }
    }
    return ret;
}

//! axis-aligned + diagonal directions - n.z == 0 exercises the reference_basis discontinuity
const std::vector<glm::vec3> g_degenerate_directions = {
        {1, 0, 0},  {-1, 0, 0}, {0, 1, 0},
        {0, -1, 0}, {0, 0, 1},  {0, 0, -1},
        glm::normalize(glm::vec3(1, 1, 0)),
        glm::normalize(glm::vec3(-1, 1, 0)),
        glm::normalize(glm::vec3(1, -1, 0)),
        glm::normalize(glm::vec3(1, 1, 1)),
        glm::normalize(glm::vec3(-1, -1, -1)),
};

float angle_between(const glm::vec3 &a, const glm::vec3 &b)
{
    return glm::degrees(std::acos(glm::clamp(glm::dot(a, b), -1.f, 1.f)));
}

//! an arbitrary tangent orthogonal to n, rotated by 'angle' within the tangent-plane
glm::vec3 tangent_for(const glm::vec3 &n, float angle)
{
    glm::vec3 b1, b2;
    reference_basis(n, b1, b2);
    return std::cos(angle) * b1 + std::sin(angle) * b2;
}

}// namespace

TEST(OctahedralMap, snorm_2x12_roundtrip)
{
    // 12-bit snorm: 1/2047 quantum, so worst-case error is half of that
    constexpr float max_error = 0.5f / 2047.f + 1e-6f;

    for(int i = -2047; i <= 2047; ++i)
    {
        glm::vec2 v(static_cast<float>(i) / 2047.f, static_cast<float>(-i) / 2047.f);
        glm::vec2 rt = unpack_snorm_2x12(pack_snorm_2x12(v));
        EXPECT_NEAR(v.x, rt.x, max_error);
        EXPECT_NEAR(v.y, rt.y, max_error);
    }

    // the two components must not bleed into each other
    EXPECT_EQ(pack_snorm_2x12({1.f, 0.f}) >> 12, 0u);
    EXPECT_EQ(pack_snorm_2x12({0.f, 1.f}) & 0xfff, 0u);

    // nothing may escape the low 24 bits
    for(const auto &d: random_directions(4096))
    {
        EXPECT_EQ(pack_snorm_2x12(normalized_vector_to_octahedral_mapping(d)) & 0xff000000u, 0u);
    }
}

TEST(OctahedralMap, reference_basis_orthonormal)
{
    auto directions = random_directions(65536);
    directions.insert(directions.end(), g_degenerate_directions.begin(), g_degenerate_directions.end());

    for(const auto &n: directions)
    {
        glm::vec3 b1, b2;
        reference_basis(n, b1, b2);

        EXPECT_NEAR(glm::length(b1), 1.f, 1e-5f);
        EXPECT_NEAR(glm::length(b2), 1.f, 1e-5f);
        EXPECT_NEAR(glm::dot(b1, b2), 0.f, 1e-5f);
        EXPECT_NEAR(glm::dot(b1, n), 0.f, 1e-5f);
        EXPECT_NEAR(glm::dot(b2, n), 0.f, 1e-5f);

        // right-handed: cross(b1, b2) == n. compared component-wise - acos is ill-conditioned here
        glm::vec3 c = glm::cross(b1, b2);
        EXPECT_NEAR(c.x, n.x, 1e-5f);
        EXPECT_NEAR(c.y, n.y, 1e-5f);
        EXPECT_NEAR(c.z, n.z, 1e-5f);
    }
}

TEST(OctahedralMap, tangent_frame_roundtrip)
{
    // 12:12 octahedral - measured worst case is ~0.056 degrees (at the octahedral cell-corners)
    constexpr float max_normal_error = 0.1f;

    // 7-bit angle -> 360/128 == 2.8125 degree steps, worst case half a step
    constexpr float max_tangent_error = 0.5f * 360.f / 128.f + 0.05f;

    std::mt19937 rng(0xc0ffee);
    std::uniform_real_distribution<float> angle_dist(-glm::pi<float>(), glm::pi<float>());

    auto directions = random_directions(65536);
    directions.insert(directions.end(), g_degenerate_directions.begin(), g_degenerate_directions.end());

    float worst_normal = 0.f, worst_tangent = 0.f;

    for(const auto &n: directions)
    {
        for(float w: {1.f, -1.f})
        {
            glm::vec3 t = tangent_for(n, angle_dist(rng));

            glm::vec3 out_n, out_t;
            float out_w = 0.f;
            unpack_tangent_frame(pack_tangent_frame(n, t, w), out_n, out_t, out_w);

            EXPECT_EQ(w, out_w) << "handedness must survive the roundtrip";

            EXPECT_NEAR(glm::length(out_n), 1.f, 1e-5f);
            EXPECT_NEAR(glm::length(out_t), 1.f, 1e-5f);

            // the decoded tangent is orthogonal to the decoded normal by construction
            EXPECT_NEAR(glm::dot(out_n, out_t), 0.f, 1e-5f);

            float normal_error = angle_between(n, out_n);
            EXPECT_LT(normal_error, max_normal_error);
            worst_normal = std::max(worst_normal, normal_error);

            // compare against the input tangent projected into the decoded normal's plane
            glm::vec3 expected_t = glm::normalize(t - out_n * glm::dot(out_n, t));
            float tangent_error = angle_between(expected_t, out_t);
            EXPECT_LT(tangent_error, max_tangent_error);
            worst_tangent = std::max(worst_tangent, tangent_error);
        }
    }
    spdlog::info("tangent-frame worst-case error: normal {:.4f} deg, tangent {:.4f} deg", worst_normal, worst_tangent);
}

TEST(OctahedralMap, tangent_frame_non_orthogonal_input)
{
    // a skewed tangent must be projected into the tangent-plane, not produce garbage
    glm::vec3 n(0, 0, 1);
    glm::vec3 skewed = glm::normalize(glm::vec3(1, 0, 0.5f));

    glm::vec3 out_n, out_t;
    float out_w = 0.f;
    unpack_tangent_frame(pack_tangent_frame(n, skewed, 1.f), out_n, out_t, out_w);

    EXPECT_NEAR(glm::dot(out_n, out_t), 0.f, 1e-5f);
    EXPECT_LT(angle_between(out_t, glm::vec3(1, 0, 0)), 1.5f);

    // a tangent parallel to the normal is degenerate but must stay finite
    unpack_tangent_frame(pack_tangent_frame(n, n, 1.f), out_n, out_t, out_w);
    EXPECT_TRUE(std::isfinite(out_t.x) && std::isfinite(out_t.y) && std::isfinite(out_t.z));
    EXPECT_NEAR(glm::length(out_t), 1.f, 1e-5f);
}

TEST(OctahedralMap, tangent_frame_bitangent_handedness)
{
    // glTF convention: bitangent == cross(normal, tangent) * w
    glm::vec3 n(0, 0, 1);
    glm::vec3 t(1, 0, 0);

    glm::vec3 out_n, out_t;
    float out_w = 0.f;

    unpack_tangent_frame(pack_tangent_frame(n, t, 1.f), out_n, out_t, out_w);
    glm::vec3 bitangent = glm::cross(out_n, out_t) * out_w;
    EXPECT_LT(angle_between(bitangent, glm::vec3(0, 1, 0)), 1.5f);

    unpack_tangent_frame(pack_tangent_frame(n, t, -1.f), out_n, out_t, out_w);
    bitangent = glm::cross(out_n, out_t) * out_w;
    EXPECT_LT(angle_between(bitangent, glm::vec3(0, -1, 0)), 1.5f);
}
