#include <crete/cluster/common.h>
#include <crete/exception.h>

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>
#include <boost/uuid/uuid_generators.hpp>
#include <boost/iostreams/filtering_streambuf.hpp>
#include <boost/iostreams/filtering_stream.hpp>
#include <boost/iostreams/copy.hpp>
#include <boost/iostreams/filter/bzip2.hpp>
#include <boost/iostreams/device/back_inserter.hpp>
#include <boost/iostreams/device/array.hpp>

//#include <boost/algorithm/string/join.hpp>

#include <boost/process.hpp>

#include <iostream> // testing.

namespace fs = boost::filesystem;
namespace bp = boost::process;
namespace bui = boost::uuids;

namespace crete
{
namespace cluster
{

ImageInfo::ImageInfo(const boost::filesystem::path& image) :
    file_name_{image.filename().string()},
    last_write_time_{fs::last_write_time(image)}
{
}

/**
 * @brief archive_directory archives a directory into a single archived file
 *        of the same name. Please note that this is done in-place.
 * @param dir - path to the directory to be archived, in-place.
 * @note Compression is also done on the archive.
 */
auto archive_directory(const boost::filesystem::path& dir) -> void
{
    auto tmp = fs::path{dir}.replace_extension("tmp");

    CRETE_EXCEPTION_ASSERT(fs::exists(dir), err::file_missing{dir.string()});

    bp::context ctx;
    ctx.work_directory = dir.parent_path().string();
    ctx.environment = bp::self::get_environment();
    auto exe = bp::find_executable_in_path("tar");
    auto args = std::vector<std::string>{fs::path{exe}.filename().string(),
                                         "-zcf",
                                         tmp.filename().string(),
                                         dir.filename().string()
                                         };

    auto proc = bp::launch(exe, args, ctx);
    auto status = proc.wait();

    CRETE_EXCEPTION_ASSERT(status.exit_status() == 0, err::process_exit_status{exe});

    auto rcount = fs::remove_all(dir);

    CRETE_EXCEPTION_ASSERT(rcount > 0, err::file_remove{dir.string()});

    fs::rename(tmp,
               dir);
}

/**
 * @brief restore_directory restores a previously archived directory via archive_directory().
 *        Please note that this is done in-place.
 * @param dir - path to the directory to be restored, in-place.
 * @note Archive is decompressed.
 */
auto restore_directory(const boost::filesystem::path& dir) -> void
{
    auto tmp = fs::path{dir}.replace_extension("tmp");

    CRETE_EXCEPTION_ASSERT(fs::exists(dir), err::file_missing{dir.string()});

    fs::rename(dir,
               tmp);

    bp::context ctx;
    ctx.work_directory = dir.parent_path().string();
    ctx.environment = bp::self::get_environment();
    auto exe = bp::find_executable_in_path("tar");
    auto args = std::vector<std::string>{fs::path{exe}.filename().string(),
                                         "-xf",
                                         tmp.filename().string(),
                                         };

    auto proc = bp::launch(exe, args, ctx);
    auto status = proc.wait();

    CRETE_EXCEPTION_ASSERT(status.exit_status() == 0, err::process_exit_status{exe});

    auto rcount = fs::remove_all(tmp);

    CRETE_EXCEPTION_ASSERT(rcount > 0, err::file_remove{dir.string()});
}

auto GuestData::write_guest_config(const boost::filesystem::path &output) -> void
{
    fs::ofstream ofs(output.string());

    boost::archive::text_oarchive oa(ofs);
    oa << guest_config;
}

} // namespace cluster
} // namespace crete
