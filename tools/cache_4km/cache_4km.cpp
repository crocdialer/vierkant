//
// cache_4km - bake self-contained '.4km' asset-bundles (mesh-buffers/meshlets/lods + texture-compression)
// from model-files, optionally storing them into a compressed zip-archive.
//

#include <filesystem>
#include <thread>

#include <crocore/ThreadPoolClassic.hpp>
#include <cxxopts.hpp>
#include <spdlog/spdlog.h>
#include <spdlog/stopwatch.h>

#include <vierkant/bundle.hpp>

int main(int argc, char *argv[])
{
    cxxopts::Options options(argv[0], "bake self-contained '.4km' asset-bundles from model-files\n");
    options.positional_help("<model-file>...");
    // clang-format off
    options.add_options()
        ("files", "input model-files (.gltf, .glb, .obj)", cxxopts::value<std::vector<std::string>>())
        ("o,output-dir", "output-directory for baked bundles", cxxopts::value<std::string>()->default_value("cache/models"))
        ("lods", "generate level-of-detail meshes")
        ("meshlets", "generate meshlets")
        ("no-pack-vertices", "disable vertex-packing")
        ("c,compress", "block-compress (BC7/BC5) all textures")
        ("omm", "bake opacity-micromaps for alpha-masked geometry")
        ("z,zip", "store bundles zstd-compressed into the given zip-archive", cxxopts::value<std::string>())
        ("j,threads", "number of worker-threads", cxxopts::value<uint32_t>())
        ("v,verbose", "verbose logging")
        ("h,help", "print this help message");
    // clang-format on
    options.parse_positional("files");

    cxxopts::ParseResult result;
    try
    {
        result = options.parse(argc, argv);
    } catch(const std::exception &e)
    {
        spdlog::error(e.what());
        return EXIT_FAILURE;
    }

    if(result.count("help"))
    {
        spdlog::set_pattern("%v");
        spdlog::info("\n{}", options.help());
        return EXIT_SUCCESS;
    }

    spdlog::set_level(result.count("verbose") ? spdlog::level::debug : spdlog::level::info);

    if(!result.count("files"))
    {
        spdlog::error("no input-files provided\n{}", options.help());
        return EXIT_FAILURE;
    }

    // bake-parameters (defaults mirror vierkant_ed)
    vierkant::bundle_params_t bundle_params = {};
    bundle_params.mesh_buffer_params.optimize_vertex_cache = true;
    bundle_params.mesh_buffer_params.pack_vertices = !result.count("no-pack-vertices");
    bundle_params.mesh_buffer_params.generate_lods = result.count("lods") > 0;
    bundle_params.mesh_buffer_params.generate_meshlets = result.count("meshlets") > 0;
    bundle_params.compress_textures = result.count("compress") > 0;
    if(result.count("omm")) { bundle_params.omm_params = vierkant::model::omm_gen_params_t{}; }

    uint32_t num_threads =
            result.count("threads") ? result["threads"].as<uint32_t>() : std::thread::hardware_concurrency();
    crocore::ThreadPoolClassic pool(num_threads);
    bundle_params.pool = &pool;

    const std::filesystem::path output_dir = result["output-dir"].as<std::string>();
    std::optional<std::filesystem::path> zip_archive;
    if(result.count("zip")) { zip_archive = result["zip"].as<std::string>(); }

    int num_failed = 0;
    for(const auto &file: result["files"].as<std::vector<std::string>>())
    {
        spdlog::stopwatch sw;
        auto assets = vierkant::create_model_bundle(file, bundle_params);
        if(!assets)
        {
            ++num_failed;
            continue;
        }
        auto bundle_path = output_dir / vierkant::model_bundle_filename(file, bundle_params.mesh_buffer_params,
                                                                               bundle_params.compress_textures,
                                                                               bundle_params.omm_params);
        vierkant::save_bundle_file(*assets, bundle_path, zip_archive);
        spdlog::info("baked '{}' -> '{}' ({})", file, bundle_path.string(), sw.elapsed());
    }

    if(num_failed) { spdlog::warn("{} file(s) failed", num_failed); }
    return num_failed ? EXIT_FAILURE : EXIT_SUCCESS;
}
