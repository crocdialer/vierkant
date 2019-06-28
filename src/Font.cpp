#include <utility>

// __ ___ ____ _____ ______ _______ ________ _______ ______ _____ ____ ___ __
//
// Copyright (C) 2012-2016, Fabian Schmidt <crocdialer@googlemail.com>
//
// It is distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
//  http://www.boost.org/LICENSE_1_0.txt
// __ ___ ____ _____ ______ _______ ________ _______ ______ _____ ____ ___ __

//  Font.cpp
//
//  Created by Fabian on 3/9/13.

#include <crocore/filesystem.hpp>
#include "Font.hpp"

#define STB_RECT_PACK_IMPLEMENTATION

#include "stb_rect_pack.h"

#define STB_TRUETYPE_IMPLEMENTATION

#include "stb_truetype.h"

#include <codecvt>

std::wstring utf8_to_wstring(const std::string &str)
{
    // the UTF-8 / UTF-16 standard conversion facet
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> utf16conv;
    return utf16conv.from_bytes(str);
}

#define BITMAP_WIDTH(font_sz) font_sz > 50 ? 2048 : 1024

namespace vierkant {

struct string_mesh_container
{
    std::string text;
    MeshPtr mesh;
    uint64_t counter;

    string_mesh_container() : counter(0) {};

    string_mesh_container(const std::string &t, const MeshPtr &m) : text(t), mesh(m), counter(0) {}

    bool operator<(const string_mesh_container &other) const { return counter < other.counter; }
};

struct FontImpl
{
    std::string path;
    std::unique_ptr<stbtt_packedchar[]> char_data;
    uint32_t font_height;
    uint32_t line_height;

    crocore::Image_<uint8_t>::Ptr bitmap;
    vierkant::ImagePtr texture, sdf_texture;
    bool use_sdf;

    // how many string meshes are buffered at max
    size_t max_mesh_buffer_size;
    std::unordered_map<std::string, string_mesh_container> string_mesh_map;

    FontImpl() :
            max_mesh_buffer_size(500)
    {
        font_height = 64;
        line_height = 70;
        use_sdf = false;
    }

    std::tuple<crocore::Image_<uint8_t>::Ptr, std::unique_ptr<stbtt_packedchar[]>>
    create_bitmap(const std::vector<uint8_t> &the_font, float the_font_size,
                  uint32_t the_bitmap_width, uint32_t the_padding)
    {
        std::unique_ptr<stbtt_packedchar[]> c_data(new stbtt_packedchar[1024]);
        auto img = crocore::Image_<uint8_t>::create(the_bitmap_width, the_bitmap_width, 1);

        // rect packing
        stbtt_pack_context spc;
        stbtt_PackBegin(&spc, (uint8_t *)img->data(), img->width(), img->height(), 0, the_padding, nullptr);

        int num_chars = 768;
        stbtt_PackFontRange(&spc, const_cast<uint8_t *>(the_font.data()), 0, the_font_size, 32,
                            num_chars, c_data.get());
        stbtt_PackEnd(&spc);
        return std::make_tuple(img, std::move(c_data));
    }

    std::list<stbtt_aligned_quad> create_quads(const std::string &the_text,
                                               uint32_t *the_max_x, uint32_t *the_max_y)
    {
        // workaround for weirdness in stb_truetype (blank 1st characters on line)
        constexpr float start_x = 0.5f;
        float x = start_x;
        float y = 0.f;
        stbtt_aligned_quad q;
        std::list<stbtt_aligned_quad> quads;

        // converts the codepoints to 16bit
        auto wstr = utf8_to_wstring(the_text);

        for(wchar_t &codepoint : wstr)
        {
            //new line
            if(codepoint == '\n')
            {
                x = start_x;
                y += line_height;
                continue;
            }
            stbtt_GetPackedQuad(char_data.get(), bitmap->width(), bitmap->height(),
                                codepoint - 32, &x, &y, &q, 0);

            if(the_max_y && *the_max_y < q.y1 + font_height){ *the_max_y = q.y1 + font_height; }
            if(the_max_x && *the_max_x < q.x1){ *the_max_x = q.x1; }
            quads.push_back(q);
        }
        return quads;
    }
};


Font::Font() : m_impl(new FontImpl())
{

}

const std::string Font::path() const
{
    return m_impl->path;
}

vierkant::ImagePtr Font::glyph_texture() const
{
    return m_impl->texture;
}

vierkant::ImagePtr Font::sdf_texture() const
{
    return m_impl->sdf_texture;
}

uint32_t Font::font_size() const
{
    return m_impl->font_height;
}

uint32_t Font::line_height() const
{
    return m_impl->line_height;
}

void Font::set_line_height(uint32_t the_line_height)
{
    m_impl->line_height = the_line_height;
}

bool Font::use_sdf() const { return m_impl->use_sdf; }

void Font::set_use_sdf(bool b) { m_impl->use_sdf = b; }

void Font::load(vierkant::DevicePtr device, const std::string &thePath, size_t theSize, bool use_sdf)
{
    try
    {
        auto p = crocore::fs::search_file(thePath);
        std::vector<uint8_t> font_data = crocore::fs::read_binary_file(p);
        m_impl->path = p;
        m_impl->string_mesh_map.clear();
        m_impl->font_height = theSize;
        m_impl->line_height = theSize;
        m_impl->use_sdf = use_sdf;

        auto tuple = m_impl->create_bitmap(font_data, theSize, BITMAP_WIDTH(theSize),
                                           use_sdf ? 6 : 2);

        m_impl->bitmap = std::get<0>(tuple);
        m_impl->char_data = std::move(std::get<1>(tuple));

//        // signed distance field
//        if(use_sdf)
//        {
//            auto dist_img = compute_distance_field(m_impl->bitmap, 5);
//            dist_img = dist_img->blur();
//            m_impl->sdf_texture = create_texture_from_image(dist_img, true);
//        }

        vierkant::Image::Format fmt;
        fmt.format = vierkant::format<uint8_t>();
        fmt.extent = {m_impl->bitmap->width(), m_impl->bitmap->height(), 1};
        fmt.component_swizzle = {VK_COMPONENT_SWIZZLE_ONE, VK_COMPONENT_SWIZZLE_ONE, VK_COMPONENT_SWIZZLE_ONE,
                                 VK_COMPONENT_SWIZZLE_R};
        m_impl->texture = vierkant::Image::create(std::move(device), m_impl->bitmap->data(), fmt);
    }catch(const std::exception &e)
    {
        LOG_ERROR << e.what();
    }
}

vierkant::AABB Font::create_aabb(const std::string &theText) const
{
    vierkant::AABB ret;
    uint32_t max_x = 0, max_y = 0;
    auto quads = m_impl->create_quads(theText, &max_x, &max_y);

    for(const auto &quad : quads)
    {
        ret += vierkant::AABB(glm::vec3(quad.x0, quad.y0, 0),
                              glm::vec3(quad.x1, quad.y1, 0));
    }
    return ret;
}

crocore::ImagePtr Font::create_image(const std::string &theText, const glm::vec4 &theColor) const
{
    uint32_t max_x = 0, max_y = 0;
    using area_pair_t = std::pair<crocore::Area_<uint32_t>, crocore::Area_<uint32_t>>;
    std::list<area_pair_t> area_pairs;
    auto quads = m_impl->create_quads(theText, &max_x, &max_y);

    for(auto &q : quads)
    {
        crocore::Area_<uint32_t> src = {static_cast<uint32_t>(q.s0 * m_impl->bitmap->width()),
                                        static_cast<uint32_t>(q.t0 * m_impl->bitmap->height()),
                                        static_cast<uint32_t>((q.s1 - q.s0) * m_impl->bitmap->width()),
                                        static_cast<uint32_t>((q.t1 - q.t0) * m_impl->bitmap->height())};
        crocore::Area_<uint32_t> dst = {static_cast<uint32_t>(q.x0),
                                        static_cast<uint32_t>(m_impl->font_height + q.y0),
                                        static_cast<uint32_t>(q.x1 - q.x0),
                                        static_cast<uint32_t>(q.y1 - q.y0)};

        area_pairs.emplace_back(src, dst);
    }
    auto dst_img = crocore::Image_<uint8_t>::create(max_x, max_y, 1);

    for(const auto &a : area_pairs)
    {
        m_impl->bitmap->roi = a.first;
        dst_img->roi = a.second;
        crocore::copy_image<uint8_t>(m_impl->bitmap, dst_img);
    }
    return dst_img;
}

vierkant::ImagePtr Font::create_texture(vierkant::DevicePtr device, const std::string &theText,
                                        const glm::vec4 &theColor) const
{
    auto img = create_image(theText, theColor);

    vierkant::Image::Format fmt;
    fmt.format = vierkant::format<uint8_t>();
    fmt.extent = {img->width(), img->height(), 1};
    fmt.component_swizzle = {VK_COMPONENT_SWIZZLE_ONE, VK_COMPONENT_SWIZZLE_ONE, VK_COMPONENT_SWIZZLE_ONE,
                             VK_COMPONENT_SWIZZLE_R};
    return vierkant::Image::create(std::move(device), img->data(), fmt);
}

//gl::MeshPtr Font::create_mesh(const std::string &theText, const glm::vec4 &theColor) const
//{
//    // look for an existing mesh
//    auto mesh_iter = m_impl->string_mesh_map.find(theText);
//
//    if(mesh_iter != m_impl->string_mesh_map.end())
//    {
//        mesh_iter->second.counter++;
//        mesh_iter->second.mesh->set_transform(mat4());
//        return mesh_iter->second.mesh;
//    }
//
//    // create a new mesh object
//    GeometryPtr geom = Geometry::create();
//    geom->set_primitive_type(GL_TRIANGLES);
//    gl::MaterialPtr mat = gl::Material::create();
//    mat->set_diffuse(theColor);
//    mat->set_blending(true);
//
//    if(m_impl->use_sdf)
//    {
//        mat->set_shader(gl::create_shader(ShaderType::SDF_FONT));
//        mat->add_texture(m_impl->sdf_texture, Texture::Usage::COLOR);
//        mat->uniform("u_buffer", 0.725f);
//        mat->uniform("u_gamma", 0.05f);
//    }else{ mat->add_texture(m_impl->texture, Texture::Usage::COLOR); }
//
//    MeshPtr ret = gl::Mesh::create(geom, mat);
//    ret->entries().clear();
//
//    std::vector<glm::vec3> &vertices = geom->vertices();
//    std::vector<glm::vec2> &tex_coords = geom->tex_coords();
//    std::vector<glm::vec4> &colors = geom->colors();
//
//    uint32_t max_y = 0;
//    auto quads = m_impl->create_quads(theText, nullptr, &max_y);
//
//    // reserve memory
//    vertices.reserve(quads.size() * 4);
//    tex_coords.reserve(quads.size() * 4);
//    colors.reserve(quads.size() * 4);
//
//    for(const auto &quad : quads)
//    {
//        float h = quad.y1 - quad.y0;
//
//        stbtt_aligned_quad adjusted = {};
//        adjusted.x0 = quad.x0;
//        adjusted.y0 = max_y - (m_impl->font_height + quad.y0);
//        adjusted.x1 = quad.x1;
//        adjusted.y1 = max_y - (m_impl->font_height + quad.y0 + h);
//        adjusted.s0 = quad.s0;
//        adjusted.t0 = 1 - quad.t0;
//        adjusted.s1 = quad.s1;
//        adjusted.t1 = 1 - quad.t1;
//
//        // CREATE QUAD
//        // create vertices
//        vertices.emplace_back(adjusted.x0, adjusted.y1, 0);
//        vertices.emplace_back(adjusted.x1, adjusted.y1, 0);
//        vertices.emplace_back(adjusted.x1, adjusted.y0, 0);
//        vertices.emplace_back(adjusted.x0, adjusted.y0, 0);
//
//        // create texcoords
//        tex_coords.emplace_back(adjusted.s0, adjusted.t1);
//        tex_coords.emplace_back(adjusted.s1, adjusted.t1);
//        tex_coords.emplace_back(adjusted.s1, adjusted.t0);
//        tex_coords.emplace_back(adjusted.s0, adjusted.t0);
//
//        // create colors
//        for(int i = 0; i < 4; i++){ colors.emplace_back(1); }
//    }
//    for(uint32_t i = 0; i < vertices.size(); i += 4)
//    {
//        geom->append_face(i, i + 1, i + 2);
//        geom->append_face(i, i + 2, i + 3);
//    }
//    geom->compute_face_normals();
//    geom->compute_aabb();
//
//    // free the less frequent used half of our buffered string-meshes
//    if(m_impl->string_mesh_map.size() >= m_impl->max_mesh_buffer_size)
//    {
//        LOG_TRACE << "font-mesh buffersize: " << m_impl->max_mesh_buffer_size << " -> clearing ...";
//        std::list<string_mesh_container> tmp_list;
//
//        for(auto &item : m_impl->string_mesh_map){ tmp_list.push_back(item.second); }
//        tmp_list.sort();
//
//        std::list<string_mesh_container>::reverse_iterator list_it = tmp_list.rbegin();
//        m_impl->string_mesh_map.clear();
//
//        for(uint32_t i = 0; i < tmp_list.size() / 2; i++, ++list_it)
//        {
//            list_it->counter--;
//            m_impl->string_mesh_map[list_it->text] = *list_it;
//        }
//    }
//    // insert the newly created mesh
//    m_impl->string_mesh_map[theText] = string_mesh_container(theText, ret);
//    return ret;
//}

//vierkant::Object3DPtr Font::create_text_object(std::list<std::string> the_lines,
//                                         Align the_align,
//                                         uint32_t the_linewidth,
//                                         uint32_t the_lineheight) const
//{
//    if(!the_lineheight){ the_lineheight = line_height(); }
//    auto root = vierkant::Object3D::create();
//    auto parent = root;
//
//    glm::vec2 line_offset;
//    bool reformat = false;
//
//    for(auto it = the_lines.begin(); it != the_lines.end(); ++it)
//    {
//        if(!reformat){ parent = root; }
//        std::string &l = *it;
//
//        // center line_mesh
//        auto line_aabb = create_aabb(l);
//
//        reformat = false;
//        auto insert_it = it;
//        insert_it++;
//
//        //split line, if necessary
//        while(the_linewidth && (line_aabb.width() > the_linewidth))
//        {
//            size_t indx = l.find_last_of(' ');
//            if(indx == std::string::npos){ break; }
//
//            std::string last_word = l.substr(indx + 1);
//            l = l.substr(0, indx);
//
//            if(!reformat)
//            {
//                parent = vierkant::Object3D::create();
//                root->add_child(parent);
//                reformat = true;
//                insert_it = the_lines.insert(insert_it, last_word);
//            }else if(insert_it != the_lines.end())
//            {
//                *insert_it = last_word + " " + *insert_it;
//            }else{ the_lines.push_back(last_word); }
//
//            // new aabb
//            line_aabb = create_aabb(l);
//        }
//
//        switch(the_align)
//        {
//            case Align::LEFT:
//                line_offset.x = 0.f;
//                break;
//            case Align::CENTER:
//                line_offset.x = (the_linewidth - line_aabb.width()) / 2.f;
//                break;
//            case Align::RIGHT:
//                line_offset.x = (the_linewidth - line_aabb.width());
//                break;
//        }
//        auto line_mesh = create_mesh(l)->copy();
//        line_mesh->set_position(vec3(line_offset.x, line_offset.y - line_aabb.height(), 0.f));
//        line_mesh->material()->set_blending();
//
//        // advance offset
//        line_offset.y -= line_height();
//
//        // add line
//        parent->add_child(line_mesh);
//    }
//    return root;
//}

vierkant::Object3DPtr Font::create_text_object(const std::string &the_text,
                                               Align the_align,
                                               uint32_t the_linewidth,
                                               uint32_t the_lineheight) const
{
    // create text meshes (1 per line)
    auto lines = crocore::split(the_text, '\n', false);
    return create_text_object(std::list<std::string>(lines.begin(), lines.end()),
                              the_align, the_linewidth, the_lineheight);
}


}// namespace
