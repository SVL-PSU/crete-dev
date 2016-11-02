#include <crete/cluster/node_options.h>

namespace pt = boost::property_tree;

namespace crete {
    namespace cluster {
        namespace node {
            namespace option {

Master::Master(const pt::ptree& tree)
{
    auto m_opt = tree.get_child_optional("crete.master");

    if(m_opt)
    {
        auto& m = tree.get_child("crete.master");

        ip = m.get<std::string>("ip", ip);
        port = m.get<Port>("port", port);
    }
}

} // namespace option
} // namespace node
} // namespace cluster
} // namespace crete
