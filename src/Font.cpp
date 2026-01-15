#include "vierkant/Font.hpp"
#include <crocore/filesystem.hpp>
#include <unordered_map>

#define STB_RECT_PACK_IMPLEMENTATION

#include "stb_rect_pack.h"

#define STB_TRUETYPE_IMPLEMENTATION

#include "stb_truetype.h"

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

#include <codecvt>

std::wstring utf8_to_wstring(const std::string &str)
{
    // the UTF-8 / UTF-16 standard conversion facet
    std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>, wchar_t> utf16conv;
    return utf16conv.from_bytes(str);
}

#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

#define BITMAP_WIDTH(font_sz) font_sz > 50 ? 2048 : 1024

namespace vierkant
{

struct string_mesh_container
{
    std::string text;
    MeshPtr mesh = nullptr;
    uint64_t counter = 0;

    bool operator<(const string_mesh_container &other) const { return counter < other.counter; }
};

FontPtr Font::create(const vierkant::DevicePtr &device, const std::filesystem::path &path, size_t size, bool use_sdf)
{
    try
    {
        auto font_data = crocore::fs::read_binary_file(path);
        return create(device, font_data, size, use_sdf);
    } catch(const std::exception &e) { return nullptr; }
}

FontPtr Font::create(const DevicePtr &device, const std::vector<uint8_t> &font_data, size_t size, bool use_sdf)
{
    return FontPtr(new Font(device, font_data, size, use_sdf));
}

struct FontImpl
{
    vierkant::DevicePtr device;
    std::unique_ptr<stbtt_packedchar[]> char_data;
    uint32_t font_height;
    uint32_t line_height;

    crocore::Image_<uint8_t>::Ptr bitmap;
    vierkant::ImagePtr texture, sdf_texture;
    bool use_sdf;

    // how many string meshes are buffered at max
    size_t max_mesh_buffer_size;
    std::unordered_map<std::string, string_mesh_container> string_mesh_map;

    FontImpl() : max_mesh_buffer_size(500)
    {
        font_height = 64;
        line_height = 70;
        use_sdf = false;
    }

    static std::pair<crocore::Image_<uint8_t>::Ptr, std::unique_ptr<stbtt_packedchar[]>>
    create_bitmap(const std::vector<uint8_t> &the_font, float the_font_size, uint32_t the_bitmap_width,
                  uint32_t the_padding)
    {
        std::unique_ptr<stbtt_packedchar[]> c_data(new stbtt_packedchar[1024]);
        auto img = crocore::Image_<uint8_t>::create(the_bitmap_width, the_bitmap_width, 1);

        // rect packing
        stbtt_pack_context spc;
        stbtt_PackBegin(&spc, (uint8_t *) img->data(), static_cast<int>(img->width()), static_cast<int>(img->height()),
                        0, static_cast<int>(the_padding), nullptr);

        int num_chars = 768;
        stbtt_PackFontRange(&spc, const_cast<uint8_t *>(the_font.data()), 0, the_font_size, 32, num_chars,
                            c_data.get());
        stbtt_PackEnd(&spc);
        return std::make_pair(img, std::move(c_data));
    }

    std::vector<stbtt_aligned_quad> create_quads(const std::string &the_text, uint32_t *max_x, uint32_t *max_y)
    {
        // workaround for weirdness in stb_truetype (blank 1st characters on line)
        constexpr float start_x = 0.5f;
        float x = start_x;
        float y = 0.f;

        // converts the codepoints to 16bit
        auto wstr = utf8_to_wstring(the_text);

        // preallocate quads
        std::vector<stbtt_aligned_quad> quads(wstr.size());

        for(uint32_t i = 0; i < quads.size(); ++i)
        {
            stbtt_aligned_quad &quad = quads[i];
            wchar_t &codepoint = wstr[i];

            //new line
            if(codepoint == '\n')
            {
                x = start_x;
                y += static_cast<float>(line_height);
                continue;
            }
            stbtt_GetPackedQuad(char_data.get(), static_cast<int>(bitmap->width()), static_cast<int>(bitmap->height()),
                                codepoint - 32, &x, &y, &quad, 0);

            if(max_y && static_cast<float>(*max_y) < quad.y1 + static_cast<float>(font_height))
            {
                *max_y = static_cast<uint32_t>(quad.y1 + static_cast<float>(font_height));
            }
            if(max_x && static_cast<float>(*max_x) < quad.x1) { *max_x = static_cast<uint32_t>(quad.x1); }
        }
        return quads;
    }
};


Font::Font(const vierkant::DevicePtr &device, const std::vector<uint8_t> &data, size_t size, bool use_sdf)
    : m_impl(new FontImpl())
{
    m_impl->device = device;
    m_impl->string_mesh_map.clear();
    m_impl->font_height = size;
    m_impl->line_height = size;
    m_impl->use_sdf = use_sdf;

    auto img_quads_pair = m_impl->create_bitmap(data, static_cast<float>(size), BITMAP_WIDTH(size), use_sdf ? 6 : 2);

    m_impl->bitmap = img_quads_pair.first;
    m_impl->char_data = std::move(img_quads_pair.second);

    vierkant::Image::Format fmt;
    fmt.format = vierkant::format<uint8_t>();
    fmt.extent = {m_impl->bitmap->width(), m_impl->bitmap->height(), 1};
    fmt.component_swizzle = {VK_COMPONENT_SWIZZLE_ONE, VK_COMPONENT_SWIZZLE_ONE, VK_COMPONENT_SWIZZLE_ONE,
                             VK_COMPONENT_SWIZZLE_R};
    m_impl->texture = vierkant::Image::create(device, m_impl->bitmap->data(), fmt);
}

vierkant::ImagePtr Font::glyph_texture() const { return m_impl->texture; }

vierkant::ImagePtr Font::sdf_texture() const { return m_impl->sdf_texture; }

uint32_t Font::font_size() const { return m_impl->font_height; }

uint32_t Font::line_height() const { return m_impl->line_height; }

void Font::set_line_height(uint32_t the_line_height) { m_impl->line_height = the_line_height; }

bool Font::use_sdf() const { return m_impl->use_sdf; }

void Font::set_use_sdf(bool b) { m_impl->use_sdf = b; }

vierkant::AABB Font::create_aabb(const std::string &theText) const
{
    vierkant::AABB ret;
    uint32_t max_x = 0, max_y = 0;
    auto quads = m_impl->create_quads(theText, &max_x, &max_y);

    for(const auto &quad: quads)
    {
        ret += vierkant::AABB(glm::vec3(quad.x0, quad.y0, 0), glm::vec3(quad.x1, quad.y1, 0));
    }
    return ret;
}

crocore::ImagePtr Font::create_image(const std::string &theText) const
{
    uint32_t max_x = 0, max_y = 0;
    using area_pair_t = std::pair<crocore::Area_<uint32_t>, crocore::Area_<uint32_t>>;
    std::list<area_pair_t> area_pairs;
    auto quads = m_impl->create_quads(theText, &max_x, &max_y);

    auto w = static_cast<float>(m_impl->bitmap->width() - 1);
    auto h = static_cast<float>(m_impl->bitmap->height() - 1);

    for(auto &q: quads)
    {
        crocore::Area_<uint32_t> src = {static_cast<uint32_t>(q.s0 * w), static_cast<uint32_t>(q.t0 * h),
                                        static_cast<uint32_t>((q.s1 - q.s0) * w),
                                        static_cast<uint32_t>((q.t1 - q.t0) * h)};
        crocore::Area_<uint32_t> dst = {static_cast<uint32_t>(q.x0),
                                        static_cast<uint32_t>(static_cast<float>(m_impl->font_height) + q.y0),
                                        static_cast<uint32_t>(q.x1 - q.x0), static_cast<uint32_t>(q.y1 - q.y0)};

        area_pairs.emplace_back(src, dst);
    }
    auto dst_img = crocore::Image_<uint8_t>::create(max_x, max_y, 1);

    for(const auto &a: area_pairs)
    {
        m_impl->bitmap->roi = a.first;
        dst_img->roi = a.second;
        crocore::copy_image<uint8_t>(m_impl->bitmap, dst_img);
    }
    return dst_img;
}

vierkant::ImagePtr Font::create_texture(vierkant::DevicePtr device, const std::string &theText) const
{
    auto img = create_image(theText);

    vierkant::Image::Format fmt;
    fmt.format = vierkant::format<uint8_t>();
    fmt.extent = {img->width(), img->height(), 1};
    fmt.use_mipmap = true;
    fmt.component_swizzle = {VK_COMPONENT_SWIZZLE_ONE, VK_COMPONENT_SWIZZLE_ONE, VK_COMPONENT_SWIZZLE_ONE,
                             VK_COMPONENT_SWIZZLE_R};
    return vierkant::Image::create(std::move(device), img->data(), fmt);
}

vierkant::MeshPtr Font::create_mesh(const std::string &theText, const glm::vec4 & /*theColor*/, float /*extrude*/) const
{
    // look for an existing mesh
    auto mesh_iter = m_impl->string_mesh_map.find(theText);

    if(mesh_iter != m_impl->string_mesh_map.end())
    {
        mesh_iter->second.counter++;
        //        mesh_iter->second.mesh->set_transform(glm::mat4(1));
        return mesh_iter->second.mesh;
    }

    uint32_t max_y = 0;
    auto quads = m_impl->create_quads(theText, nullptr, &max_y);

    auto geom = vierkant::Geometry::create();
    auto &vertices = geom->positions;
    auto &colors = geom->colors;
    auto &tex_coords = geom->tex_coords;
    auto &indices = geom->indices;

    // reserve memory
    vertices.reserve(quads.size() * 4);
    colors.reserve(quads.size() * 4);
    tex_coords.reserve(quads.size() * 4);
    indices.reserve(quads.size() * 6);

    for(const auto &quad: quads)
    {
        // CREATE QUAD
        // create positions
        vertices.emplace_back(quad.x0, quad.y1, 0);
        vertices.emplace_back(quad.x1, quad.y1, 0);
        vertices.emplace_back(quad.x1, quad.y0, 0);
        vertices.emplace_back(quad.x0, quad.y0, 0);

        // create texcoords
        tex_coords.emplace_back(quad.s0, quad.t1);
        tex_coords.emplace_back(quad.s1, quad.t1);
        tex_coords.emplace_back(quad.s1, quad.t0);
        tex_coords.emplace_back(quad.s0, quad.t0);

        // create colors
        for(int i = 0; i < 4; i++) { colors.emplace_back(1.f); }
    }
    for(uint32_t i = 0; i < vertices.size(); i += 4)
    {
        indices.push_back(i);
        indices.push_back(i + 1);
        indices.push_back(i + 2);
        indices.push_back(i);
        indices.push_back(i + 2);
        indices.push_back(i + 3);
    }

    vierkant::Mesh::create_info_t mesh_create_info = {};
    auto mesh = vierkant::Mesh::create_from_geometry(m_impl->device, geom, mesh_create_info);

    // free the less frequent used half of our buffered string-meshes
    if(m_impl->string_mesh_map.size() >= m_impl->max_mesh_buffer_size)
    {
        std::list<string_mesh_container> tmp_list;

        for(auto &item: m_impl->string_mesh_map) { tmp_list.push_back(item.second); }
        tmp_list.sort();

        auto list_it = tmp_list.rbegin();
        m_impl->string_mesh_map.clear();

        for(uint32_t i = 0; i < tmp_list.size() / 2; i++, ++list_it)
        {
            list_it->counter--;
            m_impl->string_mesh_map[list_it->text] = *list_it;
        }
    }
    // insert the newly created mesh
    m_impl->string_mesh_map[theText] = {theText, mesh};
    return mesh;
}

}// namespace vierkant
