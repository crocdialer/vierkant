//
// Created by crocdialer on 1/16/21.
//

#include <vierkant/Visitor.hpp>
#include <vierkant/MeshNode.hpp>

namespace vierkant
{

MeshNodePtr MeshNode::create(const vierkant::MeshPtr& mesh, const std::shared_ptr<entt::registry> &registry)
{
    auto ret = MeshNodePtr(new MeshNode(registry));
    ret->name = "mesh_" + std::to_string(ret->id());

    if(mesh){ ret->add_component<vierkant::MeshPtr>(mesh); }
    if(!mesh->node_animations.empty()){ ret->add_component<vierkant::animation_state_t>(); }

    // 'object as component' hack -> resolve with not having this subclass
    ret->add_component<vierkant::Object3D*>(ret.get());
    return ret;
}

void MeshNode::accept(Visitor &visitor)
{
    visitor.visit(*this);
}

vierkant::AABB MeshNode::aabb() const
{
    vierkant::AABB aabb;
    const auto &mesh = get_component<vierkant::MeshPtr>();
    for(const auto &entry: mesh->entries){ aabb += entry.bounding_box.transform(entry.transform); }
    return aabb;
}

}// namespace vierkant