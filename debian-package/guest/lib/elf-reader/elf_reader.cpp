#include <iostream>
#include <cassert>
#include <fcntl.h>
#include <stdexcept>

#include <crete/elf_reader.h>

using namespace std;

namespace crete
{

ELFReader::ELFReader(const boost::filesystem::path& file) :
    fd_(-1),
    elf_(0),
    ekind_(ELF_K_NONE),
    section_header_str_idx_(0)
{
    open_file(file);
    init_elf();
    proc_header();
    proc_sections();
}

ELFReader::~ELFReader()
{
    if(elf_ != 0)
        (void)elf_end(elf_);
    if(fd_ != -1)
        (void)close(fd_);
}

void ELFReader::open_file(const boost::filesystem::path& file)
{
    if((fd_ = open(file.generic_string().c_str(), O_RDONLY, 0)) < 0)
        throw runtime_error("failed to open: " + file.generic_string());
}

void ELFReader::init_elf()
{
    if(elf_version(EV_CURRENT) == EV_NONE)
        throw runtime_error(string("ELF library initialization failed: ") + elf_errmsg(-1));

    if((elf_ = elf_begin(fd_, ELF_C_READ, NULL)) == NULL)
        throw runtime_error(string("elf_begin failed: ") + elf_errmsg(-1));

    ekind_ = elf_kind(elf_);

    if(ekind_ != ELF_K_ELF)
        throw runtime_error(string("not an ELF object!"));
}

void ELFReader::proc_header()
{
    if(gelf_getehdr(elf_, &eheader_) == NULL)
        throw runtime_error(string("getehdr failed: ") + elf_errmsg(-1));
}

void ELFReader::proc_sections()
{
    Elf_Scn* scan = NULL;

    if(elf_getshdrstrndx(elf_, &section_header_str_idx_) != 0)
        throw runtime_error(string("elf_getshdrstrndx failed: ") + elf_errmsg(-1));

    GElf_Shdr header;

    while((scan = elf_nextscn(elf_, scan)) != NULL)
    {
        if(gelf_getshdr(scan, &header) != &header)
            throw runtime_error(string("getshdr failed: ") + elf_errmsg(-1));

        section_headers_.push_back(pair<Elf_Scn*, GElf_Shdr>(scan, header));
    }
}

Entry ELFReader::get_section_entry(const string& section, const string& entry)
{
    for(vector<std::pair<Elf_Scn*, GElf_Shdr> >::iterator iter = section_headers_.begin();
        iter != section_headers_.end();
        ++iter)
    {
        char* name;
        GElf_Shdr& shdr = iter->second;

        if((name = elf_strptr(elf_, section_header_str_idx_, shdr.sh_name)) == NULL)
            throw runtime_error(string("elf_strptr failed: ") + elf_errmsg(-1));

        if(section == string(name))
        {
            Elf_Data *data;

            if((data = elf_getdata(iter->first, NULL)) == NULL)
            {
                throw runtime_error("elf_getdata failed");
            }
            if(data->d_size <= 0)
                throw runtime_error(section + " section data size is 0");

            
            if(shdr.sh_entsize == 0)
                continue;

            uint64_t section_count = static_cast<uint64_t>(shdr.sh_size / shdr.sh_entsize);

            for(uint64_t j = 0; j < section_count; j++)
            {
                GElf_Sym sym;
                if(gelf_getsym(data, j, &sym) != &sym)
                    throw runtime_error(string("gelf_getsym failed: ") + elf_errmsg(-1));

                if(entry == (name = elf_strptr(elf_, shdr.sh_link, sym.st_name)))
                {
                    uintptr_t addr = static_cast<uintptr_t>(sym.st_value);
                    uintptr_t st_size = static_cast<uintptr_t>(sym.st_size);

                    return Entry(addr, st_size, entry);
                }
            }
        }
    }

    return Entry();
}

vector<Entry> ELFReader::get_section_entries(const string& section)
{
    vector<Entry> entries;

    for(vector<std::pair<Elf_Scn*, GElf_Shdr> >::iterator iter = section_headers_.begin();
        iter != section_headers_.end();
        ++iter)
    {
        char* name;
        GElf_Shdr& shdr = iter->second;

        if((name = elf_strptr(elf_, section_header_str_idx_, shdr.sh_name)) == NULL)
            throw runtime_error(string("elf_strptr failed: ") + elf_errmsg(-1));

        if(section == string(name))
        {
            Elf_Data *data;

            if((data = elf_getdata(iter->first, NULL)) == NULL)
            {
                throw runtime_error("elf_getdata failed");
            }
            if(data->d_size <= 0)
                throw runtime_error(section + " section data size is 0");

            if(shdr.sh_entsize == 0)
                continue;

            uint64_t section_count = static_cast<uint64_t>(shdr.sh_size / shdr.sh_entsize);

            for(uint64_t j = 0; j < section_count; j++)
            {
                GElf_Sym sym;
                if(gelf_getsym(data, j, &sym) != &sym)
                    throw runtime_error(string("gelf_getsym failed: ") + elf_errmsg(-1));

                uintptr_t addr = static_cast<uintptr_t>(sym.st_value);
                uintptr_t st_size = static_cast<uintptr_t>(sym.st_size);
                string name = elf_strptr(elf_, shdr.sh_link, sym.st_name);

                entries.push_back(Entry(addr, st_size, name));
            }
        }
    }

    return entries;
}

Entry ELFReader::get_section(const string& section)
{
    for(vector<std::pair<Elf_Scn*, GElf_Shdr> >::iterator iter = section_headers_.begin();
        iter != section_headers_.end();
        ++iter)
    {
        char* name;
        GElf_Shdr& shdr = iter->second;

        if((name = elf_strptr(elf_, section_header_str_idx_, shdr.sh_name)) == NULL)
            throw runtime_error(string("elf_strptr failed: ") + elf_errmsg(-1));

        if(section == string(name))
            return Entry(shdr.sh_addr, shdr.sh_size, section);
    }

    return Entry(0,0);
}

vector<uint8_t> ELFReader::get_section_data(const string& section)
{
    for(vector<std::pair<Elf_Scn*, GElf_Shdr> >::iterator iter = section_headers_.begin();
        iter != section_headers_.end();
        ++iter)
    {
        char* name;
        GElf_Shdr& shdr = iter->second;

        if((name = elf_strptr(elf_, section_header_str_idx_, shdr.sh_name)) == NULL)
            throw runtime_error(string("elf_strptr failed: ") + elf_errmsg(-1));

        if(section == string(name))
        {
            Elf_Data *data;

            if((data = elf_getdata(iter->first, NULL)) == NULL)
            {
                throw runtime_error("elf_getdata failed");
            }
            if(data->d_size <= 0)
                return vector<uint8_t>();

            if(section == ".bss") // Uninitialized data, none contained in ELF file, so make it all 0s.
                return vector<uint8_t>(data->d_size, 0);

            return vector<uint8_t>(static_cast<uint8_t*>(data->d_buf),
                                   static_cast<uint8_t*>(data->d_buf) + data->d_size);
        }
    }

    throw runtime_error("failed to find elf section named: " + section);
}

Elf64_Half ELFReader::get_type()
{
    return eheader_.e_type;
}

Elf64_Half ELFReader::get_machine()
{
    return eheader_.e_machine;
}

Elf64_Addr ELFReader::get_entry_address()
{
    return eheader_.e_entry;
}

int ELFReader::get_class()
{
    int c;

    if((c = gelf_getclass(elf_)) == ELFCLASSNONE)
        throw runtime_error(string("getclass failed: ") + elf_errmsg(-1));

    return c;
}

bool operator<(const Entry& lhs, const Entry& rhs)
{
    return lhs.addr < rhs.addr;
}

} // namespace crete
