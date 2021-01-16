//
// Created by crocdialer on 1/16/21.
//

#include <vierkant/Visitor.hpp>
#include <vierkant/MeshNode.hpp>

namespace vierkant
{

MeshNodePtr MeshNode::create(vierkant::MeshPtr mesh)
{
    auto ret = MeshNodePtr(new MeshNode());
    ret->set_name("mesh_" + std::to_string(ret->id()));
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
    for(const auto &entry : mesh->entries){ aabb += entry.boundingbox.transform(entry.transform); }
    return aabb;
}

}// namespace vierkant