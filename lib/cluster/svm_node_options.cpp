#include <boost/property_tree/ptree.hpp>
#include <boost/filesystem.hpp>
#include <boost/algorithm/string/predicate.hpp>

#include <algorithm>

#include <crete/cluster/svm_node_options.h>
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

SVMNode::SVMNode(const pt::ptree& tree)
    : master(tree)
    , svm(tree)
    , translator(tree)
{
}

SVM::SVM(const pt::ptree& tree)
{
    auto opt_svm = tree.get_child_optional("crete.svm");

    if(opt_svm)
    {
        auto& svm = *opt_svm;

        path.concolic = svm.get<std::string>("path.concolic", path.concolic);
        path.symbolic = svm.get<std::string>("path.symbolic", path.symbolic);
        count = svm.get<uint32_t>("count", count);

        if(!path.concolic.empty())
        {
            CRETE_EXCEPTION_ASSERT(fs::exists(path.concolic), err::file_missing{path.concolic});

            path.concolic = fs::canonical(path.concolic).string();
        }
        if(!path.symbolic.empty())
        {
            CRETE_EXCEPTION_ASSERT(fs::exists(path.symbolic), err::file_missing{path.symbolic});

            path.symbolic = fs::canonical(path.symbolic).string();
        }

        if(count == 0)
        {
            BOOST_THROW_EXCEPTION(Exception{} << err::arg_invalid_str{"crete.svm.count"});
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
