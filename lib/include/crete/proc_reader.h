#ifndef CRETE_PROC_READER_H
#define CRETE_PROC_READER_H

#include <iostream>
#include <stdint.h>
#include <utility>
#include <string>
#include <map>
#include <vector>

#include <boost/filesystem/path.hpp>

#include <crete/dll.h>

namespace crete
{

class CRETE_DLL_EXPORT ProcMap
{
public:
    ProcMap();

    std::pair<uintptr_t, uintptr_t> address() const { return address_; }
    void set_address(const std::pair<uintptr_t, uintptr_t>& address) { address_ = address; }
    std::string permissions() const { return perms_; }
    void set_permissions(const std::string& permissions) { perms_ = permissions; }
    size_t offset() const { return offset_; }
    void set_offset(size_t offset) { offset_ = offset; }
    std::string device_number() const { return dev_num_; }
    void set_device_number(const std::string& dev_num) { dev_num_ = dev_num; }
    size_t inode() const { return inode_; }
    void set_inode(size_t inode) { inode_ = inode; }
    std::string path() const { return path_; }
    void set_path(const std::string& path) { path_ = path; }

private:
    std::pair<uintptr_t, uintptr_t> address_;
    std::string perms_;
    size_t offset_;
    std::string dev_num_;
    size_t inode_;
    std::string path_;
};

typedef std::vector<ProcMap> ProcMaps;

class CRETE_DLL_EXPORT ProcReader
{
public:
    ProcReader(); // Parses process's own maps.
    ProcReader(const boost::filesystem::path& proc_maps);

    ProcMaps find(std::string path) const; // Returns empty vector if none is found.
    ProcMaps find_all() const;

    std::string get_executable() const;

protected:
    void parse(const boost::filesystem::path& proc_maps);

private:
    std::multimap<std::string, ProcMap> proc_maps_;
    std::string executable_;
};

std::ostream& operator<<(std::ostream& os, const ProcMap& pm);

/// Combines maps from the same binaries into single entries. Example usage: m = condense(pr.find_all());
/// Warning: ignores nameless ProcMaps.
ProcMaps condense(const ProcMaps& maps);

} // namespace crete

#endif // CRETE_PROC_READER_H
