//
// Created by crocdialer on 4/20/18.
//

#include "vierkant/imgui/imgui_util.h"
#include "vierkant/imgui/ImGuizmo.h"

using namespace crocore;

namespace vierkant::gui {


// int
void draw_property_ui(const Property_<int>::Ptr &the_property)
{
    std::string prop_name = the_property->name();

    if(auto ranged_prop = std::dynamic_pointer_cast<RangedProperty<int>>(the_property))
    {
        if(ImGui::SliderInt(prop_name.c_str(), &ranged_prop->value(), ranged_prop->range().first,
                            ranged_prop->range().second))
        {
            the_property->notify_observers();
        }
    }else
    {
        if(ImGui::InputInt(prop_name.c_str(), &the_property->value(), 1, 10, ImGuiInputTextFlags_EnterReturnsTrue))
        {
            the_property->notify_observers();
        }
    }
}

void draw_property_ui(const Property_<uint32_t>::Ptr &the_property)
{
    std::string prop_name = the_property->name();
    int val = *the_property;

    if(auto ranged_prop = std::dynamic_pointer_cast<RangedProperty<uint32_t>>(the_property))
    {
        if(ImGui::SliderInt(prop_name.c_str(), &val, ranged_prop->range().first,
                            ranged_prop->range().second))
        {
            *the_property = val;
        }
    }else
    {
        if(ImGui::InputInt(prop_name.c_str(), &val, 1, 10, ImGuiInputTextFlags_EnterReturnsTrue))
        {
            *the_property = val;
        }
    }
}

// float
void draw_property_ui(const Property_<float>::Ptr &the_property)
{
    std::string prop_name = the_property->name();

    if(auto ranged_prop = std::dynamic_pointer_cast<RangedProperty<float>>(the_property))
    {
        if(ImGui::SliderFloat(prop_name.c_str(), &ranged_prop->value(), ranged_prop->range().first,
                              ranged_prop->range().second))
        {
            the_property->notify_observers();
        }
    }else
    {
        if(ImGui::InputFloat(prop_name.c_str(), &the_property->value(), 0.f, 0.f,
                             (std::abs(the_property->value()) < 1.f) ? 5 : 2,
                             ImGuiInputTextFlags_EnterReturnsTrue))
        {
            the_property->notify_observers();
        }
    }
}

// bool
void draw_property_ui(const Property_<bool>::Ptr &the_property)
{
    std::string prop_name = the_property->name();

    if(ImGui::Checkbox(prop_name.c_str(), &the_property->value()))
    {
        the_property->notify_observers();
    }
}

// gl::vec2
void draw_property_ui(const Property_<glm::vec2>::Ptr &the_property)
{
    std::string prop_name = the_property->name();

    if(ImGui::InputFloat2(prop_name.c_str(), &the_property->value()[0], 2, ImGuiInputTextFlags_EnterReturnsTrue))
    {
        the_property->notify_observers();
    }
}

// gl::vec3
void draw_property_ui(const Property_<glm::vec3>::Ptr &the_property)
{
    std::string prop_name = the_property->name();

    if(ImGui::InputFloat3(prop_name.c_str(), &the_property->value()[0], 2, ImGuiInputTextFlags_EnterReturnsTrue))
    {
        the_property->notify_observers();
    }
}

// gl::ivec2
void draw_property_ui(const Property_<glm::ivec2>::Ptr &the_property)
{
    std::string prop_name = the_property->name();

    if(ImGui::InputInt2(prop_name.c_str(), &the_property->value()[0], ImGuiInputTextFlags_EnterReturnsTrue))
    {
        the_property->notify_observers();
    }
}

// gl::ivec3
void draw_property_ui(const Property_<glm::ivec3>::Ptr &the_property)
{
    std::string prop_name = the_property->name();

    if(ImGui::InputInt3(prop_name.c_str(), &the_property->value()[0], ImGuiInputTextFlags_EnterReturnsTrue))
    {
        the_property->notify_observers();
    }
}

// gl::Color (a.k.a. gl::vec4)
void draw_property_ui(const Property_<glm::vec4>::Ptr &the_property)
{
    std::string prop_name = the_property->name();

    if(ImGui::ColorEdit4(prop_name.c_str(), (float *)&the_property->value()))
    {
        the_property->notify_observers();
    }
}

// std::string
void draw_property_ui(const Property_<std::string>::Ptr &the_property)
{
    std::string prop_name = the_property->name();
    constexpr size_t buf_size = 512;
    char text_buf[buf_size];
    strcpy(text_buf, the_property->value().c_str());

    if(ImGui::InputText(prop_name.c_str(), text_buf, buf_size, ImGuiInputTextFlags_EnterReturnsTrue))
    {
        the_property->value(text_buf);
    }
}

// std::vector<std::string>>
void draw_property_ui(const Property_<std::vector<std::string>>::Ptr &the_property)
{
    std::string prop_name = the_property->name();
    std::vector<std::string> &array = the_property->value();

    if(ImGui::TreeNode(prop_name.c_str()))
    {
        for(size_t i = 0; i < array.size(); ++i)
        {
            size_t buf_size = array[i].size() + 4;
            char text_buf[buf_size];

            strcpy(text_buf, array[i].c_str());

            if(ImGui::InputText(to_string(i).c_str(), text_buf, buf_size,
                                ImGuiInputTextFlags_EnterReturnsTrue))
            {
                array[i] = text_buf;
                the_property->notify_observers();
            }
        }
        ImGui::TreePop();
    }
}

// std::vector<float>>
void draw_property_ui(const Property_<std::vector<float>>::Ptr &the_property)
{
    std::string prop_name = the_property->name();
    std::vector<float> &array = the_property->value();

    if(ImGui::TreeNode(prop_name.c_str()))
    {
        for(size_t i = 0; i < array.size(); ++i)
        {
            if(ImGui::InputFloat(std::to_string(i).c_str(), &array[i], 0.f, 0.f,
                                 (std::abs(array[i]) < 1.f) ? 5 : 2,
                                 ImGuiInputTextFlags_EnterReturnsTrue))
            {
                the_property->notify_observers();
            }
        }
        ImGui::TreePop();
    }
}

// generic
void draw_property_ui(const PropertyPtr &the_property)
{
    std::string prop_name = the_property->name();

    if(ImGui::TreeNode(prop_name.c_str()))
    {
        ImGui::Text("%s", prop_name.c_str());
        ImGui::TreePop();
    }

}

void draw_component_ui(const ComponentConstPtr &the_component)
{
    ImGui::Begin(the_component->name().c_str());

    for(auto &p : the_component->get_property_list())
    {
        // skip non-tweakable properties
        if(!p->tweakable()){ continue; }

        if(p->is_of_type<bool>())
        {
            auto cast_prop = std::dynamic_pointer_cast<Property_<bool>>(p);
            draw_property_ui(cast_prop);
        }else if(p->is_of_type<int>())
        {
            auto cast_prop = std::dynamic_pointer_cast<Property_<int>>(p);
            draw_property_ui(cast_prop);
        }else if(p->is_of_type<uint32_t>())
        {
            auto cast_prop = std::dynamic_pointer_cast<Property_<uint32_t>>(p);
            draw_property_ui(cast_prop);
        }else if(p->is_of_type<float>())
        {
            auto cast_prop = std::dynamic_pointer_cast<Property_<float>>(p);
            draw_property_ui(cast_prop);
        }else if(p->is_of_type<glm::vec2>())
        {
            auto cast_prop = std::dynamic_pointer_cast<Property_<glm::vec2>>(p);
            draw_property_ui(cast_prop);
        }else if(p->is_of_type<glm::vec3>())
        {
            auto cast_prop = std::dynamic_pointer_cast<Property_<glm::vec3>>(p);
            draw_property_ui(cast_prop);
        }else if(p->is_of_type<glm::ivec2>())
        {
            auto cast_prop = std::dynamic_pointer_cast<Property_<glm::ivec2>>(p);
            draw_property_ui(cast_prop);
        }else if(p->is_of_type<glm::ivec3>())
        {
            auto cast_prop = std::dynamic_pointer_cast<Property_<glm::ivec3>>(p);
            draw_property_ui(cast_prop);
        }else if(p->is_of_type<glm::vec4>())
        {
            auto cast_prop = std::dynamic_pointer_cast<Property_<glm::vec4>>(p);
            draw_property_ui(cast_prop);
        }else if(p->is_of_type<std::string>())
        {
            auto cast_prop = std::dynamic_pointer_cast<Property_<std::string>>(p);
            draw_property_ui(cast_prop);
        }else if(p->is_of_type<std::vector<std::string>>())
        {
            auto cast_prop = std::dynamic_pointer_cast<Property_<std::vector<std::string>>>(p);
            draw_property_ui(cast_prop);
        }else if(p->is_of_type<std::vector<float>>())
        {
            auto cast_prop = std::dynamic_pointer_cast<Property_<std::vector<float>>>(p);
            draw_property_ui(cast_prop);
        }else{ draw_property_ui(p); }
    }

    ImGui::End();
}

}// namespace vierkant::gui

