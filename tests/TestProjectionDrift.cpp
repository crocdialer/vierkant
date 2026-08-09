#include <gtest/gtest.h>
#include <vierkant/PBRPathTracer.hpp>
#include <vierkant/projection.hpp>

//____________________________________________________________________________//

namespace
{

// a narrow fov keeps the tangent-distortion at the sampled corners small, so the analytic
// small-angle prediction below holds for the maximum, not just for the image-center.
const float g_fovy = glm::radians(10.f);
constexpr float g_near = 0.1f;
constexpr float g_z_ref = 10.f;

glm::mat4 perspective(float aspect) { return vierkant::perspective_infinite_reverse_RH_ZO(g_fovy, aspect, g_near); }

//! projection-view for a camera placed at 'transform', looking down -z when identity
glm::mat4 projection_view(float aspect, const glm::mat4 &transform)
{ return perspective(aspect) * glm::inverse(transform); }

}// namespace

TEST(ProjectionDrift, static_camera)
{
    constexpr float aspect = 16.f / 9.f;
    auto projection_inverse = glm::inverse(perspective(aspect));

    // identical cameras. not exactly zero: the projection round-trips through glm::inverse,
    // which leaves ~1 ulp. that is ~1e-5 pixels at 240p, far below any tolerable budget.
    float drift = vierkant::projection_drift(projection_view(aspect, glm::mat4(1)), projection_inverse, glm::mat4(1),
                                             false, aspect, g_z_ref);
    EXPECT_LT(drift, 1.e-6f);
}

TEST(ProjectionDrift, rotation_magnitude)
{
    constexpr float aspect = 1.f;
    const float theta = glm::radians(1.f);
    auto projection_inverse = glm::inverse(perspective(aspect));

    // previous frame's camera, yawed away by 'theta'
    glm::mat4 prev_transform = glm::mat4_cast(glm::angleAxis(theta, glm::vec3(0.f, 1.f, 0.f)));

    float drift = vierkant::projection_drift(projection_view(aspect, prev_transform), projection_inverse, glm::mat4(1),
                                             false, aspect, g_z_ref);

    // a yaw of 'theta' moves content by theta/fovy of the image-height
    EXPECT_NEAR(drift, theta / g_fovy, 0.03f * theta / g_fovy);
}

TEST(ProjectionDrift, rotation_is_depth_independent)
{
    constexpr float aspect = 16.f / 9.f;
    const float theta = glm::radians(1.f);
    auto projection_inverse = glm::inverse(perspective(aspect));
    auto prev = projection_view(aspect, glm::mat4_cast(glm::angleAxis(theta, glm::vec3(0.f, 1.f, 0.f))));

    // under pure rotation the reprojection maps rays to rays, so 'z_ref' must not matter
    float drift_near = vierkant::projection_drift(prev, projection_inverse, glm::mat4(1), false, aspect, 1.f);
    float drift_far = vierkant::projection_drift(prev, projection_inverse, glm::mat4(1), false, aspect, 1000.f);
    EXPECT_NEAR(drift_near, drift_far, 1.e-5f);
}

TEST(ProjectionDrift, rotation_is_aspect_independent)
{
    const float theta = glm::radians(1.f);
    glm::mat4 prev_transform = glm::mat4_cast(glm::angleAxis(theta, glm::vec3(0.f, 1.f, 0.f)));

    // height-fractions, so a wider frame must report the same drift for the same rotation.
    // not exactly equal: the wider frame's corners sit further off-axis and stretch slightly more.
    float drift_square = vierkant::projection_drift(projection_view(1.f, prev_transform),
                                                    glm::inverse(perspective(1.f)), glm::mat4(1), false, 1.f, g_z_ref);
    float drift_wide =
            vierkant::projection_drift(projection_view(16.f / 9.f, prev_transform),
                                       glm::inverse(perspective(16.f / 9.f)), glm::mat4(1), false, 16.f / 9.f, g_z_ref);
    EXPECT_NEAR(drift_square, drift_wide, 0.05f * drift_square);
}

TEST(ProjectionDrift, translation_parallax_falls_off_with_distance)
{
    constexpr float aspect = 16.f / 9.f;
    auto projection_inverse = glm::inverse(perspective(aspect));

    // previous frame's camera, translated sideways by 1 unit
    glm::mat4 prev_transform = glm::translate(glm::mat4(1), glm::vec3(1.f, 0.f, 0.f));
    auto prev = projection_view(aspect, prev_transform);

    float drift_near = vierkant::projection_drift(prev, projection_inverse, glm::mat4(1), false, aspect, 10.f);
    float drift_far = vierkant::projection_drift(prev, projection_inverse, glm::mat4(1), false, aspect, 100.f);

    // parallax scales with 1/distance
    EXPECT_GT(drift_near, 0.f);
    EXPECT_NEAR(drift_near / drift_far, 10.f, 0.5f);
}

TEST(ProjectionDrift, ortho_rotation)
{
    constexpr float aspect = 1.f;
    const float theta = glm::radians(5.f);
    auto projection = vierkant::ortho_reverse_RH_ZO(-1.f, 1.f, -1.f, 1.f, 0.f, 100.f);

    float drift_static =
            vierkant::projection_drift(projection, glm::inverse(projection), glm::mat4(1), true, aspect, g_z_ref);
    EXPECT_LT(drift_static, 1.e-6f);

    glm::mat4 prev_transform = glm::mat4_cast(glm::angleAxis(theta, glm::vec3(0.f, 1.f, 0.f)));
    float drift = vierkant::projection_drift(projection * glm::inverse(prev_transform), glm::inverse(projection),
                                             glm::mat4(1), true, aspect, g_z_ref);

    // an ortho camera slides content by the rotated reference-plane's lateral offset,
    // sin(theta) * z_ref, expressed in half-heights of the [-1, 1] view-volume
    EXPECT_NEAR(drift, 0.5f * std::sin(theta) * g_z_ref, 0.05f * std::sin(theta) * g_z_ref);
}
