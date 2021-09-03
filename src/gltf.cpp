//
// Created by crocdialer on 9/3/21.
//

#define TINYGLTF_IMPLEMENTATION

#include <tiny_gltf.h>

#include <vierkant/gltf.hpp>

namespace vierkant::model
{

////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

mesh_assets_t gltf(const std::filesystem::path &path)
{
    using namespace tinygltf;

    Model model;
    TinyGLTF loader;
    std::string err;
    std::string warn;

    bool ret = false;

    auto ext_str = path.extension();

    if(ext_str == ".gltf")
    {
        ret = loader.LoadASCIIFromFile(&model, &err, &warn, path);
    }
    else if(ext_str == ".glb")
    {
        ret = loader.LoadBinaryFromFile(&model, &err, &warn, path);
    }

    if(!warn.empty())
    {
        printf("Warn: %s\n", warn.c_str());
    }

    if(!err.empty())
    {
        printf("Err: %s\n", err.c_str());
    }


    if(ret)
    {
        for(const auto &ext : model.extensionsUsed)
        {
            LOG_DEBUG << "model using extension: " << ext;
        }
    }
    return {};
}

}// namespace vierkant::model

