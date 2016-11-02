#include <crete/proc_reader.h>

#include <fstream>
#include <sstream>

#include <iostream> // testing

#include <boost/regex.hpp>
#include <boost/filesystem.hpp>

using namespace std;
using namespace boost;
namespace fs = boost::filesystem;

namespace crete
{

ProcMap::ProcMap() :
    address_(pair<uintptr_t, uintptr_t>(0, 0)),
    perms_("----"),
    offset_(0),
    inode_(0)
{
}

ProcReader::ProcReader()
{
    parse("/proc/self/maps");
}

ProcReader::ProcReader(const filesystem::path& proc_maps)
{
    parse(proc_maps);
}

string ProcReader::get_executable() const
{
    return executable_;
}

std::vector<ProcMap> ProcReader::find(string path) const
{
    vector<ProcMap> ret;

    pair<multimap<string, ProcMap>::const_iterator,
         multimap<string, ProcMap>::const_iterator> range = proc_maps_.equal_range(path);

    for(multimap<string, ProcMap>::const_iterator iter = range.first;
        iter != range.second;
        ++iter)
    {
        ret.push_back(iter->second);
    }

    return ret;
}

void ProcReader::parse(const boost::filesystem::path& proc_maps)
{
    if(!fs::exists(proc_maps))
    {
        throw std::runtime_error("[crete.ProcReader.parse] failed to find file: " + proc_maps.string());
    }

    ifstream ifs(proc_maps.generic_string().c_str());

    if(!ifs.good())
    {
        throw std::runtime_error("[crete.ProcReader.parse] failed to open file: " + proc_maps.string());
    }

    bool first_line = true;
    string line;
    while(getline(ifs, line))
    {
        ProcMap pm;
        regex e ("([0-9a-f]+)(-)([0-9a-f]+)"
                 "([[:space:]]+)"
                 "([\\-a-z]{4})"
                 "([[:space:]]+)"
                 "([0-9a-f]+)"
                 "([[:space:]]+)"
                 "([0-9a-f]+)(:)([0-9a-f]+)"
                 "([[:space:]]+)"
                 "([0-9a-f]+)"
                 "([[:space:]]+)"
                 "(.*)");
        std::string::const_iterator start, end;
        start = line.begin();
        end = line.end();
        boost::match_results<std::string::const_iterator> what;
        boost::match_flag_type flags = boost::match_default;

        regex_search(start, end, what, e, flags);

        assert(what.size() == 16);

        uintptr_t addr_begin;
        uintptr_t addr_end;
        (stringstream(string(what[1].first, what[1].second))) >> hex >> addr_begin;
        (stringstream(string(what[3].first, what[3].second))) >> hex >> addr_end;
        pm.set_address(pair<uintptr_t, uintptr_t>(addr_begin, addr_end));
        pm.set_permissions(string(what[5].first, what[5].second));
        size_t offset;
        (stringstream(string(what[7].first, what[7].second))) >> hex >> offset;
        pm.set_offset(offset);
        pm.set_device_number(string(what[9].first, what[9].second) +
                             string(what[10].first, what[10].second) +
                             string(what[11].first, what[11].second));
        size_t inode;
        (stringstream(string(what[13].first, what[13].second))) >> hex >> inode;
        pm.set_inode(inode);
        pm.set_path(string(what[15].first, what[15].second));

        proc_maps_.insert(pair<string, ProcMap>(pm.path(), pm));

        if(first_line) // Assumes executable is always first entry listed.
        {
            executable_ = pm.path();

            first_line = false;
        }
    }
}

std::vector<crete::ProcMap> crete::ProcReader::find_all() const
{
    std::vector<ProcMap> v;

    for(map<string, ProcMap>::const_iterator iter = proc_maps_.begin();
        iter != proc_maps_.end();
        ++iter)
    {
        v.push_back(iter->second);
    }

    return v;
}

ostream& operator<<(ostream& os, const crete::ProcMap& pm)
{
    cout << hex;
    os << "addr: " << pm.address().first << "-" << pm.address().second << endl;
    os << "perms: " << pm.permissions() << endl;
    os << "offset: " << pm.offset() << endl;
    os << "dev: " << pm.device_number() << endl;
    os << "inode: " << pm.inode() << endl;
    os << "path: " << pm.path() << endl;
    cout << dec;

    return os;
}

ProcMaps condense(const ProcMaps& maps)
{
    std::map<std::string, ProcMap> cmaps;

    for(ProcMaps::const_iterator it = maps.begin();
        it != maps.end();
        ++it)
    {
        std::string path = it->path();
        std::pair<uint64_t, uint64_t> addr = it->address();

        if(path.empty())
        {
            continue;
        }

        std::map<std::string, ProcMap>::iterator res = cmaps.find(path);
        if(res != cmaps.end())
        {
            ProcMap& pm = res->second;
            std::pair<uint64_t, uint64_t> addr_stored = pm.address();

            if(addr_stored.first > addr.first)
            {
                 pm.set_address(std::make_pair(addr.first, addr_stored.second));
            }
            if(addr_stored.second < addr.second)
            {
                pm.set_address(std::make_pair(addr_stored.first, addr.second));
            }
        }
        else
        {
            cmaps.insert(std::make_pair(path, *it));
        }
    }

    ProcMaps cmapsv;

    for(std::map<std::string, ProcMap>::const_iterator it = cmaps.begin();
        it != cmaps.end();
        ++it)
    {
        cmapsv.push_back(it->second);
    }

    return cmapsv;
}

} // namespace crete
