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

    vierkant::MeshPtr mesh = nullptr;

//    //! animation playstate
//    vierkant::animation_state_t animation_state = {};

    static MeshNodePtr create(vierkant::MeshPtr mesh,
                              const vierkant::SceneConstPtr &scene = nullptr);

    void accept(Visitor &visitor) override;

    vierkant::AABB aabb() const override;

private:

    explicit MeshNode(const vierkant::SceneConstPtr &scene) : Object3D(scene){};
};

}// namespace vierkant