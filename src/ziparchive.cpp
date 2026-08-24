//
// Created by crocdialer on 25.08.22.
//

#include <fstream>
#include <functional>
#include <zip.h>

#include <vierkant/ziparchive.h>

namespace vierkant
{

// maaaybe need those later
//zip_int64_t zip_source_callback(void *_Nullable, void *_Nullable, zip_uint64_t, zip_source_cmd_t);
//
//void zip_progress_callback(zip_t *archive, double, void *_Nullable);
//
//int zip_cancel_callback(zip_t *archive, void *_Nullable);

struct zipstreambuffer : public std::streambuf
{
    zipstreambuffer(zip_t *_archive, const std::filesystem::path &file_path) : archive(_archive)
    { zip_file = {zip_fopen(archive, file_path.string().c_str(), 0), zip_fclose}; }

    std::streamsize xsgetn(char *s, std::streamsize n) override
    {
        if(zip_file) { return zip_fread(zip_file.get(), s, n); }
        return EOF;
    }

    zip_t *archive = nullptr;
    std::unique_ptr<struct zip_file, std::function<int(struct zip_file *)>> zip_file;
};

ziparchive::istream::istream(std::shared_ptr<zip_t> _archive, const std::filesystem::path &file_path)
    : std::istream(new zipstreambuffer(_archive.get(), file_path)), m_archive(std::move(_archive))
{}

ziparchive::istream::~istream() { delete rdbuf(); }

ziparchive::ziparchive(const std::filesystem::path &archive_path)
{
    int errorp;

    int flags = std::filesystem::exists(archive_path) ? 0 : ZIP_EXCL | ZIP_CREATE;
    m_archive = {zip_open(archive_path.string().c_str(), flags, &errorp), zip_close};

    if(!m_archive)
    {
        zip_error_t ziperror;
        zip_error_init_with_code(&ziperror, errorp);
        throw std::runtime_error("Failed to open archive: " + archive_path.string() + ": " +
                                 zip_error_strerror(&ziperror));
    }
}

void ziparchive::add_file(const std::filesystem::path &file_path, const std::filesystem::path &entry_path)
{
    std::unique_ptr<zip_source_t, std::function<void(zip_source_t *)>> source;
    source = {zip_source_file(m_archive.get(), file_path.string().c_str(), 0, -1), zip_source_close};
    auto entry_name = (entry_path.empty() ? file_path : entry_path).generic_string();
    auto file_index = zip_file_add(m_archive.get(), entry_name.c_str(), source.get(), ZIP_FL_OVERWRITE);
    constexpr uint32_t zstd_lvl = 10;// 1-22
    zip_set_file_compression(m_archive.get(), file_index, ZIP_CM_ZSTD, zstd_lvl);
}

bool ziparchive::has_file(const std::filesystem::path &file_path) const
{ return m_archive && zip_name_locate(m_archive.get(), file_path.string().c_str(), 0) != -1; }

ziparchive::istream ziparchive::open_file(const std::filesystem::path &file_path) const
{ return {m_archive, file_path}; }

std::vector<std::filesystem::path> ziparchive::contents() const
{
    if(m_archive)
    {
        uint32_t num_entries = zip_get_num_entries(m_archive.get(), 0);
        std::vector<std::filesystem::path> ret(num_entries);
        zip_stat_t zip_stat;

        for(uint32_t i = 0; i < num_entries; i++)
        {
            if(zip_stat_index(m_archive.get(), i, 0, &zip_stat) == 0) { ret[i] = zip_stat.name; }
        }
        return ret;
    }
    return {};
}
}// namespace vierkant