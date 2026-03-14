#pragma once

#include "Object3D.hpp"
#include "camera_params.hpp"

namespace vierkant
{

DEFINE_CLASS_PTR(Camera)
DEFINE_CLASS_PTR(CubeCamera)

/**
 * @brief   Extract the near- and far-clipping distances from a projection matrix.
 *
 * @param   projection  a provided 4x4 projection matrix.
 * @return  a glm::vec2 containing (near, far) distances
 */
glm::vec2 clipping_distances(const glm::mat4 &projection);

struct camera_component_t
{
    VIERKANT_ENABLE_AS_COMPONENT();
    vierkant::camera_params_variant_t params;
};

namespace camera
{

inline vierkant::transform_t view_transform(const vierkant::Object3D *camera)
{
    auto t = vierkant::inverse(camera->global_transform());
    t.scale = {1.f, 1.f, 1.f};
    return t;
}

glm::mat4 projection_matrix(const vierkant::Object3D *camera);
float near(const vierkant::Object3D *camera);
float far(const vierkant::Object3D *camera);
vierkant::Frustum frustum(const vierkant::Object3D *camera);
vierkant::Ray calculate_ray(const vierkant::Object3D *camera, const glm::vec2 &pos, const glm::vec2 &extent);

}// namespace camera

class CubeCamera
{
public:
    CubeCamera(float near, float far) { m_params.clipping_distances = {near, far}; };

    [[nodiscard]] glm::mat4 projection_matrix() const;

    [[nodiscard]] glm::mat4 view_matrix(uint32_t the_face) const;

    [[nodiscard]] std::vector<glm::mat4> view_matrices() const;

private:
    physical_camera_params_t m_params = {};
};

}// namespace vierkant
