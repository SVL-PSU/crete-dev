#ifndef ELF_READER_H
#define ELF_READER_H

#include <gelf.h>

#include <string>
#include <vector>
#include <stdint.h>

#include <boost/filesystem/path.hpp>

namespace crete
{

struct Entry
{
    Entry() : addr(0), size(0) {}
    Entry(uintptr_t address, uintptr_t e_size) : addr(address), size(e_size) {}
    Entry(uintptr_t address, uintptr_t e_size, const std::string& e_name) : addr(address), size(e_size), name(e_name) {}

    template <class Archive>
    void serialize(Archive& ar, const unsigned int version)
    {
        (void)version;

        ar & addr;
        ar & size;
        ar & name;
    }

    uintptr_t addr;
    uintptr_t size;
    std::string name;
};

bool operator<(const Entry& lhs, const Entry& rhs);

class ELFReader
{
public:
    typedef std::vector<Entry> Entries; // TODO: should be a set that uses address range of uniqueness comparison (ordered according to address).

public:
    ELFReader(const boost::filesystem::path& file);
    ~ELFReader();

    int get_class(); // Architecture (32/64). Compare with ELFCLASS32/64.
    Elf64_Half get_type(); // Type of file. Compare with ET_* - ET_REL, ET_EXEC, ET_DYN, ET_CORE. 1, 2, 3, 4 specify relocatable, executable, shared, or core, respectively.
    Elf64_Half get_machine(); // Instruction set architecture. Compare with EM_* - EM_386, EM_X86_64, etc.
    Elf64_Addr get_entry_address(); // Starting address. e.g., 0x0804800
    Entry get_section_entry(const std::string& section, const std::string& entry); // Returns Entry::addr = 0 if not found
    Entries get_section_entries(const std::string &section);
    Entry get_section(const std::string& section);
    std::vector<uint8_t> get_section_data(const std::string& section);


protected:
    void open_file(const boost::filesystem::path& file);
    void init_elf();
    void proc_header();
    void proc_sections();

private:
    int fd_;
    Elf* elf_;
    Elf_Kind ekind_;
    GElf_Ehdr eheader_;
    std::vector<std::pair<Elf_Scn*, GElf_Shdr> > section_headers_;
    size_t section_header_str_idx_;
};

} // namespace crete

#endif // ELF_READER_H
