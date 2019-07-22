#include <cmath>

//
// Created by crocdialer on 4/20/18.
//

#include "vierkant/imgui/imgui_util.h"
#include "vierkant/imgui/ImGuizmo.h"

using namespace crocore;

namespace vierkant::gui {

class EditorColorScheme
{
// 0xRRGGBBAA
    inline static int BackGroundColor = 0x25213100;
    inline static int TextColor = 0xF4F1DE00;
    inline static int MainColor = 0xDA115E00;
    inline static int MainAccentColor = 0x79235900;
    inline static int HighlightColor = 0xC7EF0000;
    inline static int Black = 0x00000000;
    inline static int White = 0xFFFFFF00;

    inline static int AlphaTransparent = 0x00;
    inline static int Alpha20 = 0x33;
    inline static int Alpha40 = 0x66;
    inline static int Alpha50 = 0x80;
    inline static int Alpha60 = 0x99;
    inline static int Alpha80 = 0xCC;
    inline static int Alpha90 = 0xE6;
    inline static int AlphaFull = 0xFF;

    static float GetR(int colorCode) { return (float)((colorCode & 0xFF000000) >> 24) / (float)(0xFF); }

    static float GetG(int colorCode) { return (float)((colorCode & 0x00FF0000) >> 16) / (float)(0xFF); }

    static float GetB(int colorCode) { return (float)((colorCode & 0x0000FF00) >> 8) / (float)(0xFF); }

    static float GetA(int alphaCode) { return ((float)alphaCode / (float)0xFF); }

    static ImVec4 GetColor(int c, int a = Alpha80) { return ImVec4(GetR(c), GetG(c), GetB(c), GetA(a)); }

    static ImVec4 Darken(ImVec4 c, float p)
    {
        return ImVec4(std::fmax(0.f, c.x - 1.0f * p), std::fmax(0.f, c.y - 1.0f * p), std::fmax(0.f, c.z - 1.0f * p), c.w);
    }

    static ImVec4 Lighten(ImVec4 c, float p)
    {
        return ImVec4(std::fmax(0.f, c.x + 1.0f * p), std::fmax(0.f, c.y + 1.0f * p), std::fmax(0.f, c.z + 1.0f * p), c.w);
    }

    static ImVec4 Disabled(ImVec4 c) { return Darken(c, 0.6f); }

    static ImVec4 Hovered(ImVec4 c) { return Lighten(c, 0.2f); }

    static ImVec4 Active(ImVec4 c) { return Lighten(ImVec4(c.x, c.y, c.z, 1.0f), 0.1f); }

    static ImVec4 Collapsed(ImVec4 c) { return Darken(c, 0.2f); }

public:

    static void SetColors(int backGroundColor, int textColor, int mainColor, int mainAccentColor, int highlightColor)
    {
        BackGroundColor = backGroundColor;
        TextColor = textColor;
        MainColor = mainColor;
        MainAccentColor = mainAccentColor;
        HighlightColor = highlightColor;
    }

    static void ApplyTheme(ImGuiStyle& style)
    {
        ImVec4 *colors = style.Colors;

        colors[ImGuiCol_Text] = GetColor(TextColor);
        colors[ImGuiCol_TextDisabled] = Disabled(colors[ImGuiCol_Text]);
        colors[ImGuiCol_WindowBg] = GetColor(BackGroundColor);
        colors[ImGuiCol_ChildBg] = GetColor(Black, Alpha20);
        colors[ImGuiCol_PopupBg] = GetColor(BackGroundColor, Alpha90);
        colors[ImGuiCol_Border] = Lighten(GetColor(BackGroundColor), 0.4f);
        colors[ImGuiCol_BorderShadow] = GetColor(Black);
        colors[ImGuiCol_FrameBg] = GetColor(MainAccentColor, Alpha40);
        colors[ImGuiCol_FrameBgHovered] = Hovered(colors[ImGuiCol_FrameBg]);
        colors[ImGuiCol_FrameBgActive] = Active(colors[ImGuiCol_FrameBg]);
        colors[ImGuiCol_TitleBg] = GetColor(BackGroundColor, Alpha80);
        colors[ImGuiCol_TitleBgActive] = Active(colors[ImGuiCol_TitleBg]);
        colors[ImGuiCol_TitleBgCollapsed] = Collapsed(colors[ImGuiCol_TitleBg]);
        colors[ImGuiCol_MenuBarBg] = Darken(GetColor(BackGroundColor), 0.2f);
        colors[ImGuiCol_ScrollbarBg] = Lighten(GetColor(BackGroundColor, Alpha50), 0.4f);
        colors[ImGuiCol_ScrollbarGrab] = Lighten(GetColor(BackGroundColor), 0.3f);
        colors[ImGuiCol_ScrollbarGrabHovered] = Hovered(colors[ImGuiCol_ScrollbarGrab]);
        colors[ImGuiCol_ScrollbarGrabActive] = Active(colors[ImGuiCol_ScrollbarGrab]);
        colors[ImGuiCol_CheckMark] = GetColor(HighlightColor);
        colors[ImGuiCol_SliderGrab] = GetColor(HighlightColor);
        colors[ImGuiCol_SliderGrabActive] = Active(colors[ImGuiCol_SliderGrab]);
        colors[ImGuiCol_Button] = GetColor(MainColor, Alpha80);
        colors[ImGuiCol_ButtonHovered] = Hovered(colors[ImGuiCol_Button]);
        colors[ImGuiCol_ButtonActive] = Active(colors[ImGuiCol_Button]);
        colors[ImGuiCol_Header] = GetColor(MainAccentColor, Alpha80);
        colors[ImGuiCol_HeaderHovered] = Hovered(colors[ImGuiCol_Header]);
        colors[ImGuiCol_HeaderActive] = Active(colors[ImGuiCol_Header]);
        colors[ImGuiCol_Separator] = colors[ImGuiCol_Border];
        colors[ImGuiCol_SeparatorHovered] = Hovered(colors[ImGuiCol_Separator]);
        colors[ImGuiCol_SeparatorActive] = Active(colors[ImGuiCol_Separator]);
        colors[ImGuiCol_ResizeGrip] = GetColor(MainColor, Alpha20);
        colors[ImGuiCol_ResizeGripHovered] = Hovered(colors[ImGuiCol_ResizeGrip]);
        colors[ImGuiCol_ResizeGripActive] = Active(colors[ImGuiCol_ResizeGrip]);
        colors[ImGuiCol_Tab] = GetColor(MainColor, Alpha60);
        colors[ImGuiCol_TabHovered] = Hovered(colors[ImGuiCol_Tab]);
        colors[ImGuiCol_TabActive] = Active(colors[ImGuiCol_Tab]);
        colors[ImGuiCol_TabUnfocused] = colors[ImGuiCol_Tab];
        colors[ImGuiCol_TabUnfocusedActive] = colors[ImGuiCol_TabActive];
//        colors[ImGuiCol_DockingPreview] = Darken(colors[ImGuiCol_HeaderActive], 0.2f);
//        colors[ImGuiCol_DockingEmptyBg] = Darken(colors[ImGuiCol_HeaderActive], 0.6f);
        colors[ImGuiCol_PlotLines] = GetColor(HighlightColor);
        colors[ImGuiCol_PlotLinesHovered] = Hovered(colors[ImGuiCol_PlotLines]);
        colors[ImGuiCol_PlotHistogram] = GetColor(HighlightColor);
        colors[ImGuiCol_PlotHistogramHovered] = Hovered(colors[ImGuiCol_PlotHistogram]);
        colors[ImGuiCol_TextSelectedBg] = GetColor(HighlightColor, Alpha40);
        colors[ImGuiCol_DragDropTarget] = GetColor(HighlightColor, Alpha80);;
        colors[ImGuiCol_NavHighlight] = GetColor(White);
        colors[ImGuiCol_NavWindowingHighlight] = GetColor(White, Alpha80);
        colors[ImGuiCol_NavWindowingDimBg] = GetColor(White, Alpha20);
        colors[ImGuiCol_ModalWindowDimBg] = GetColor(Black, Alpha60);

        ImGui::GetStyle().WindowMenuButtonPosition = ImGuiDir_Right;
    }
};


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

void draw_images_ui(const std::vector<vierkant::ImagePtr> &images)
{
    ImGui::Begin("textures");

    const float w = ImGui::GetContentRegionAvailWidth();
    const ImVec2 uv_0(0, 0), uv_1(1, 1);

    for(const auto &tex : images)
    {
        if(tex)
        {
            ImVec2 sz(w, w / (tex->width() / (float)tex->height()));
            ImGui::Image((ImTextureID)(tex.get()), sz, uv_0, uv_1);
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();
        }
    }
    ImGui::End();
}

}// namespace vierkant::gui

