#include "vierkant/Scene.hpp"
#include "vierkant/Visitor.hpp"
#include "vierkant/cubemap_utils.hpp"

namespace vierkant
{

struct range_item_t
{
    Object3D *object = nullptr;

    float distance = 0.f;

    bool operator<(const range_item_t &other) const{ return distance < other.distance; }
};

class UpdateVisitor : public Visitor
{
public:
    explicit UpdateVisitor(float time_step) : m_time_step(time_step){};

    void visit(vierkant::Object3D &object) override
    {
        object.update(m_time_step);
        Visitor::visit(static_cast<vierkant::Object3D &>(object));
    };

    void visit(vierkant::MeshNode &node) override
    {
        if(node.mesh && node.mesh->animation_index < node.mesh->node_animations.size())
        {
            // update node animation
            vierkant::update_animation(node.mesh->node_animations[node.mesh->animation_index],
                                       m_time_step,
                                       node.mesh->animation_speed);

            // apply animation to entry-transforms
            node.mesh->update_entry_transforms();
        }
        visit(static_cast<vierkant::Object3D &>(node));
    }

    bool should_visit(vierkant::Object3D &object) override
    {
        return object.enabled();
    }

private:

    float m_time_step;
};

ScenePtr Scene::create()
{
    return ScenePtr(new Scene());
}

void Scene::add_object(const Object3DPtr &object)
{
    m_root->add_child(object);
}

void Scene::remove_object(const Object3DPtr &object)
{
    m_root->remove_child(object, true);
}

void Scene::clear()
{
    m_root = vierkant::Object3D::create("scene root");
//    m_skybox.reset();
}

void Scene::update(double time_delta)
{
    UpdateVisitor uv(time_delta);
    m_root->accept(uv);
}

Object3DPtr Scene::pick(const Ray &ray, bool high_precision,
                        const std::set<std::string> &tags) const
{
    Object3DPtr ret;
    SelectVisitor<Object3D> sv(tags);
    m_root->accept(sv);

    std::list<range_item_t> clicked_items;

    for(const auto &object : sv.objects)
    {
        if(object == m_root.get()){ continue; }

//        vierkant::Ray ray_in_object_space = ray.transform(glm::inverse(object->global_transform()));
//        if(auto ray_hit = object->obb().intersect(ray_in_object_space))
        if(auto ray_hit = object->obb().transform(object->global_transform()).intersect(ray))
        {
//            if(high_precision)
//            {
//                const vierkant::Mesh *m = dynamic_cast<const vierkant::Mesh *>(object);
//                if(!m){ continue; }
//
////                if()
//                {
//                    vierkant::Ray ray_in_object_space = ray.transform(glm::inverse(object->global_transform()));
//                    const auto &vertices = m->geometry()->vertices();
//                    const auto &indices = m->geometry()->indices();
//
//                    for(const auto &e : m->entries)
//                    {
////                            if(e.primitive_type && e.primitive_type != GL_TRIANGLES){ continue; }
//
//                        for(uint32_t i = 0; i < e.num_indices; i += 3)
//                        {
//                            vierkant::Triangle t(vertices[indices[i + e.base_index] + e.base_vertex],
//                                                 vertices[indices[i + e.base_index + 1] + e.base_vertex],
//                                                 vertices[indices[i + e.base_index + 2] + e.base_vertex]);
//
//                            if(ray_triangle_intersection ray_tri_hit = t.intersect(ray_in_object_space))
//                            {
//                                float distance_scale = glm::length(
//                                        object->global_scale() * ray_in_object_space.direction);
//                                ray_tri_hit.distance *= distance_scale;
//                                clicked_items.push_back(range_item_t(object, ray_tri_hit.distance));
//                                LOG_TRACE_2 << "hit distance: " << ray_tri_hit.distance;
//                                break;
//                            }
//                        }
//                    }
//                }
//            }
//            else
            {
                clicked_items.push_back({object, ray_hit.distance});
            }
        }
    }
    if(!clicked_items.empty())
    {
        clicked_items.sort();
        ret = clicked_items.front().object->shared_from_this();
        LOG_TRACE << "ray hit id " << ret->id() << " (" << clicked_items.size() << " total)";
    }
    return ret;
}

vierkant::Object3DPtr Scene::object_by_name(const std::string &name) const
{
    vierkant::SelectVisitor<vierkant::Object3D> sv({}, false);
    root()->accept(sv);
    for(vierkant::Object3D *o : sv.objects)
    {
        if(o->name() == name){ return o->shared_from_this(); }
    }
    return nullptr;
}

std::vector<vierkant::Object3DPtr> Scene::objects_by_tags(const std::set<std::string> &tags) const
{
    std::vector<vierkant::Object3DPtr> ret;
    vierkant::SelectVisitor<vierkant::Object3D> sv(tags, false);
    root()->accept(sv);

    for(vierkant::Object3D *o : sv.objects){ ret.push_back(o->shared_from_this()); }
    return ret;
}

std::vector<vierkant::Object3DPtr> Scene::objects_by_tag(const std::string &tag) const
{
    return objects_by_tags({tag});
}

void Scene::set_enironment(const vierkant::ImagePtr &img)
{
    if(!img){ return; }

//    // derive sane resolution for cube from panorama-width
//    float res = crocore::next_pow_2(std::max(img->width(), img->height()) / 4);
//    m_skybox = vierkant::cubemap_from_panorama(img, {res, res}, true);

    m_skybox = img;
}

}//namespace
