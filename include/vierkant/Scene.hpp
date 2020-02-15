#pragma once

#include <vierkant/Object3D.hpp>
#include <vierkant/Mesh.hpp>
#include <vierkant/Camera.hpp>
#include <vierkant/Image.hpp>

namespace vierkant
{

DEFINE_CLASS_PTR(Scene);

class Scene
{
public:

    static ScenePtr create();

    void update(float time_delta);

    Object3DPtr pick(const Ray &ray, bool high_precision = false,
                     const std::set<std::string> &the_tags = {}) const;

    void add_object(const Object3DPtr &the_object);

    void remove_object(const Object3DPtr &the_object);

    void clear();

    vierkant::Object3DPtr object_by_name(const std::string &name) const;

    std::vector<vierkant::Object3DPtr> objects_by_tags(const std::set<std::string> &tags) const;

    std::vector<vierkant::Object3DPtr> objects_by_tag(const std::string &tag) const;

    inline const Object3DPtr &root() const{ return m_root; };

    inline Object3DPtr &root(){ return m_root; };

    const vierkant::MeshPtr &skybox() const{ return m_skybox; }

    void set_skybox(const vierkant::ImagePtr &img);

private:

    Scene() = default;

    vierkant::MeshPtr m_skybox = nullptr;

    Object3DPtr m_root = Object3D::create("scene root");
};

}//namespace
