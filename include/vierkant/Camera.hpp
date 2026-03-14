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
{ return vierkant::inverse(camera->global_transform()); }

glm::mat4 projection_matrix(const vierkant::Object3D *camera);
float near(const vierkant::Object3D *camera);
float far(const vierkant::Object3D *camera);
vierkant::Frustum frustum(const vierkant::Object3D *camera);
vierkant::Ray calculate_ray(const vierkant::Object3D *camera, const glm::vec2 &pos, const glm::vec2 &extent);

}// namespace camera

class Camera : virtual public Object3D
{
public:
    vierkant::transform_t view_transform() const;

    virtual glm::mat4 projection_matrix() const = 0;

    virtual vierkant::Frustum frustum() const = 0;

    virtual float near() const = 0;

    virtual float far() const = 0;

    virtual vierkant::Ray calculate_ray(const glm::vec2 &pos, const glm::vec2 &extent) const = 0;

    vierkant::camera_params_variant_t &params() { return get_component<camera_component_t>().params; }

    const vierkant::camera_params_variant_t &params() const { return get_component<camera_component_t>().params; }

    void accept(class Visitor &v) override;
};

class CubeCamera : public Camera
{
public:
    static CubeCameraPtr create(entt::registry *registry, float near, float far)
    {
        auto ret = CubeCameraPtr(new CubeCamera(registry));
        physical_camera_params_t params = {};
        params.clipping_distances = {near, far};
        ret->add_component<camera_component_t>({params});
        return ret;
    };

    glm::mat4 projection_matrix() const override;

    vierkant::Frustum frustum() const override;

    float near() const override;

    float far() const override;

    vierkant::Ray calculate_ray(const glm::vec2 &pos, const glm::vec2 &extent) const override;

    glm::mat4 view_matrix(uint32_t the_face) const;

    std::vector<glm::mat4> view_matrices() const;

private:
    CubeCamera(entt::registry *registry);
};

}// namespace vierkant
