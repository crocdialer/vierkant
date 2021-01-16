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

    static MeshNodePtr create(vierkant::MeshPtr mesh);

    vierkant::MeshPtr mesh = nullptr;

    MeshNode(const Mesh &) = delete;

    MeshNode(Mesh &&) = delete;

    MeshNode &operator=(Mesh other) = delete;

    void accept(Visitor &visitor) override;

    vierkant::AABB aabb() const override;

private:

    MeshNode() = default;
};

}// namespace vierkant