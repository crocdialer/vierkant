//
// Created by crocdialer on 1/16/21.
//

#pragma once

#include <vierkant/Object3D.hpp>
#include <vierkant/Mesh.hpp>

namespace vierkant
{

DEFINE_CLASS_PTR(MeshNode)

class MeshNode : public vierkant::Object3D
{
public:

    static MeshNodePtr create(const vierkant::MeshPtr& mesh,
                              const std::weak_ptr<entt::registry> &registry = {});

    void accept(Visitor &visitor) override;

    vierkant::AABB aabb() const override;

private:

    explicit MeshNode(const std::weak_ptr<entt::registry> &registry) : Object3D(registry){};
};

}// namespace vierkant