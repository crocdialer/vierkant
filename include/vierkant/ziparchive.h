//
// Created by crocdialer on 25.08.22.
//

#pragma once

#include <filesystem>
#include <vector>

struct zip;
typedef struct zip zip_t;

namespace vierkant
{

class ziparchive
{
public:
    class istream : public std::istream
    {
    public:
        istream(std::shared_ptr<zip_t> _archive, const std::filesystem::path &file_path);
        ~istream() override;

    private:
        std::shared_ptr<zip_t> m_archive;
    };

    explicit ziparchive(const std::filesystem::path &archive_path);

    /**
     * @brief   'contents' can be used to retrieve a list of files/folders contained in a ziparchive.
     *
     * @return   a list of relative paths within the ziparchive
     */
    [[nodiscard]] std::vector<std::filesystem::path> contents() const;

    /**
     * @brief   'has_file' can be used to check for existance of a provided relative path within the ziparchive.
     *
     * @param   file_path   a relative path to check for
     * @return  true, if the path is contained in the ziparchive
     */
    [[nodiscard]] bool has_file(const std::filesystem::path &file_path) const;

    /**
     * @brief   'add_file' will add an external file to the ziparchive
     *
     * @param   file_path   a provided path to an existing file
     * @param   entry_path  an optional relative path, used as entry-name within the ziparchive.
     *                      if empty, file_path is used as-is.
     */
    void add_file(const std::filesystem::path &file_path, const std::filesystem::path &entry_path = {});

    /**
     * @brief   'open_file' will open a contained file within the ziparchive, referenced by it's relative file_path.
     *
     * @param   file_path    a relative path within the ziparchive
     * @return  a std::istream which can be used to read/deflate a contained file
     */
    [[nodiscard]] ziparchive::istream open_file(const std::filesystem::path &file_path) const;

private:
    std::shared_ptr<zip_t> m_archive;
};

}// namespace vierkant
