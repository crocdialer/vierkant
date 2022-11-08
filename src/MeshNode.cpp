//
// Created by crocdialer on 1/16/21.
//

#include <vierkant/Visitor.hpp>
#include <vierkant/MeshNode.hpp>

namespace vierkant
{

MeshNodePtr MeshNode::create(vierkant::MeshPtr mesh, const vierkant::SceneConstPtr &scene)
{
    auto ret = MeshNodePtr(new MeshNode(scene));
    ret->set_name("mesh_" + std::to_string(ret->id()));

    if(!mesh->node_animations.empty())
    {
        vierkant::animation_state_t anim_state = {};
        ret->add_component(anim_state);
    }
    ret->mesh = std::move(mesh);
    return ret;
}

void MeshNode::accept(Visitor &visitor)
{
    visitor.visit(*this);
}

vierkant::AABB MeshNode::aabb() const
{
    vierkant::AABB aabb;
    for(const auto &entry: mesh->entries){ aabb += entry.bounding_box.transform(entry.transform); }
    return aabb;
}

}// namespace vierkant