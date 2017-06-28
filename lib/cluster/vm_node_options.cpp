#include <crete/cluster/vm_node_options.h>
#include <crete/exception.h>

namespace pt = boost::property_tree;
namespace fs = boost::filesystem;

namespace crete
{
namespace cluster
{
namespace node
{
namespace option
{

VMNode::VMNode(const pt::ptree& tree)
    : master(tree)
    , vm(tree)
    , translator(tree)
{
}

VM::VM(const pt::ptree& tree)
{
    auto opt_vm = tree.get_child_optional("crete.vm");

    if(opt_vm)
    {
        auto& vme = *opt_vm;

        path.x86 = vme.get<std::string>("path.x86", path.x86);
        path.x64 = vme.get<std::string>("path.x64", path.x64);
        count = vme.get<uint32_t>("count", count);

        if(!path.x86.empty())
        {
            CRETE_EXCEPTION_ASSERT(fs::exists(path.x86), err::file_missing{path.x86});

            path.x86 = fs::canonical(path.x86).string();
        }
        if(!path.x64.empty())
        {
            CRETE_EXCEPTION_ASSERT(fs::exists(path.x64), err::file_missing{path.x64});

            path.x64 = fs::canonical(path.x64).string();
        }

        if(count == 0)
        {
            BOOST_THROW_EXCEPTION(Exception{} << err::arg_invalid_str{"crete.vm.count"});
        }
    }
}

Translator::Translator(const pt::ptree& tree)
{
    auto opt_trans = tree.get_child_optional("crete.translator");

    if(opt_trans)
    {
        auto& trans = *opt_trans;

        path.x86 = trans.get<std::string>("path.x86", path.x86);
        path.x64 = trans.get<std::string>("path.x64", path.x64);

        auto proc = [](const std::string& p)
        {
            if(!p.empty())
            {
                CRETE_EXCEPTION_ASSERT(fs::exists(p), err::file_missing{p});

                auto t = fs::path{p};

                if(t.is_relative())
                {
                    // Don't use fs::read_link or fs::canonical.
                    // They resolve to the underlying link which isn't desired, so we can find .bc in the symlink dir.
                    // Instead, manually resolve relative path.
                    auto s = fs::current_path();

                    for(const auto& f : t)
                    {
                        if(f.string() == "..")
                            s = s.parent_path();
                        else
                            s /= f;
                    }

                    return s.string();
                }
            }

            return p;
        };

        path.x86 = proc(path.x86);
        path.x64 = proc(path.x64);
    }
}

} // namespace option
} // namespace node
} // namespace cluster
} // namespace crete
