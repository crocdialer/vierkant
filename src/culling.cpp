//
// Created by crocdialer on 6/14/20.
//

#include "vierkant/Visitor.hpp"
#include "vierkant/culling.hpp"

namespace vierkant
{

class CullVisitor : public vierkant::Visitor
{
public:
    CullVisitor() : vierkant::Visitor(){};

    void visit(vierkant::Mesh &mesh) override
    {
        // TODO: create drawables
        auto mesh_ptr = std::dynamic_pointer_cast<vierkant::Mesh>(mesh.shared_from_this());
//        vierkant::Renderer::create_drawables(mesh_ptr);
        Visitor::visit(static_cast<vierkant::Object3D&>(mesh));
    }

    cull_result_t cull_result;
};

cull_result_t cull(const SceneConstPtr &scene, const CameraPtr &cam)
{
    CullVisitor cull_visitor;
    scene->root()->accept(cull_visitor);
    return std::move(cull_visitor.cull_result);
}

}