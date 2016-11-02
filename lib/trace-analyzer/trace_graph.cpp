#include <crete/trace_graph.h>

#include "breadth_first_search.hpp"

#include <boost/graph/graphviz.hpp> // testing
#include <boost/graph/mcgregor_common_subgraphs.hpp>
#include <boost/graph/depth_first_search.hpp>
//#include <boost/graph/breadth_first_search.hpp>
#include <boost/property_map/property_map.hpp>
#include <boost/range/adaptor/reversed.hpp>
#include <boost/regex.hpp>
#include <boost/filesystem/operations.hpp>
#include <boost/filesystem/fstream.hpp>

#include <iostream> // testing
#include <functional>

#include <crete/test_case.h>

using namespace boost;
namespace fs = boost::filesystem;

namespace crete
{

template <typename Trace>
struct FoundTrace
{
    FoundTrace(const Trace& trace) : trace_(trace) {}
    Trace trace_;
};

class bfs_execution_visitor: public default_bfs_visitor
{
public:
    bfs_execution_visitor(TraceGraph& tg) : tg_(tg) {}

    template < typename Vertex, typename Graph >
    void discover_vertex(Vertex u, const Graph& g) const
    {
        if(!tg_.executed(g[u].trace_id))
        {
            tg_.executed(Trace(g[u].trace_id, Trace::Blocks()), true);

            // Seems like a hack. Why doesn't BGL just allow for returning bool to stop the search?
            // Net answers suggest throwing an exception. Same effect, but unusal.
            throw FoundTrace<decltype(VertexProperty::trace_id)>(g[u].trace_id);
        }
    }

    TraceGraph& tg_;
};

std::string parse_trace_number(const Trace::ID& id)
{
    auto s = id;
    smatch m;
    regex e ("(runtime-dump-)([[:digit:]]+)");

    boost::regex_search(s, m, e);
    auto num = *(m.begin() + 2);

    return num;
}

std::string parse_iteration_number(const Trace::ID& id)
{
    auto s = id;
    smatch m;
    regex e ("(runtime-dump-)([[:digit:]]+)(-)([[:digit:]]+\\.[[:digit:]]+)");

    boost::regex_search(s, m, e);
    auto num = *(m.begin() + 4);

    return num;
}

std::string parse_tb_number(const Trace::ID& id)
{
    auto s = id;
    smatch m;
    regex e ("(tb-)([[:digit:]]+)");

    boost::regex_search(s, m, e);
    auto num = *(m.begin() + 2);

    return num;
}

std::string parse_trace_value(const Trace::ID& id)
{
    auto value_path = fs::path{id}/"concrete_inputs.bin";

    fs::fstream ifs(value_path);
    auto tc = read_test_case(ifs);
    auto elems = tc.get_elements();

    assert(elems.size() > 0);

    if(elems.size() > 1)
        return "multi-elems";

    auto v = std::vector<char>{};
    auto elem = elems.at(0);
    auto size = elem.data.size();
    v.resize(size * 2 + 1);

    for(auto i = 0u; i < size; ++i)
    {
        sprintf(&(v.data()[i*2]), "%02x", elem.data[i]);
    }
    v[size * 2] = '\0';

    return v.data();
}

template <class Name>
class label_writer
{
public:
  label_writer(const TraceGraph& trace_graph,
               Name _name,
               const TraceGraph::TraceScoreMap& trace_scores,
               const std::set<TraceGraph::Score>& scores,
               const std::map<AddressRange, Entry>& elf_entries) :
      tg_(trace_graph),
      name(_name),
      trace_scores_(trace_scores),
      scores_(scores),
      elf_entries_(elf_entries)
  {
  }
  template <class Vertex>
  void operator()(std::ostream& out, const Vertex& v) const
  {
      TraceGraph::Score score{0u};
      auto trace_id = name[v].trace_id;
      auto block_id = name[v].unique_hash;

      auto it = trace_scores_.find(Trace{trace_id, Trace::Blocks()});
      if(it != trace_scores_.end())
      {
          score = it->second;
      }

      auto elf_it = std::find_if(elf_entries_.begin(),
                                 elf_entries_.end(),
                                 [&block_id](const std::pair<AddressRange, Entry>& p) {
          return is_in(p.first, block_id);
      });
      auto fname = std::string{};
      if(elf_it != elf_entries_.end())
      {
          fname = elf_it->second.name;
      }

    out << "[";

    // <label>
    out << "label=\""

        << "[" // Trace number.
        << parse_trace_number(trace_id)
        << "]\\n"
        << "{" // Trace value.
        << parse_trace_value(trace_id) // TODO: should this be an option in dispatch.ini?
        << "}\\n"
        << "(" // Block Id/TB address
        << std::hex << block_id << std::dec
        << ")\\n";

    if(fname != "")
    {
        out << "\'"
            << fname
            << "\'\\n";
    }

    out << "<" // Block weight.
        <<  score
        << ">"
           ;

    out << "\""; // End quote for label=\"
    // </label>

    // <shape>
    if(name[v].cache)
    {
        out << " shape=\"diamond\" ";
    }
    // </shape>

    // <color>
    if(tg_.executed(trace_id))
    {
//        out << " color=\"red\" ";
    }
    // </color>

    // <fillcolor>
    if(tg_.last_selected() == trace_id)
    {
        out << " style=\"filled\" ";
        out << " fillcolor=\"red\" ";
    }
    else if(tg_.executed(trace_id))
    {
        out << " style=\"filled\" ";
        out << " fillcolor=\"pink\" ";
    }
    else
    {
        auto it_score = scores_.find(score);
        if(it_score != scores_.end())
        {
            std::stringstream color_ss;
            color_ss << "0.400 ";

            auto pos = std::distance(scores_.begin(), it_score);
            auto score = static_cast<float>(pos) / scores_.size();

            color_ss << score;
            color_ss << " 1.000 ";

            out << " style=\"filled\" ";
            out << " fillcolor=\""
                << color_ss.str()
                << "\" ";
        }
    }
    // </fillcolor>

    out << "]"; // Final ending bracket.
  }
private:
  const TraceGraph& tg_;
  Name name;
  const TraceGraph::TraceScoreMap& trace_scores_;
  const std::set<TraceGraph::Score>& scores_;
  const std::map<AddressRange, Entry>& elf_entries_;
};



void write_file(const TraceGraph& trace_graph,
                const TraceGraph::Graph& graph,
                const TraceGraph::TraceScoreMap& trace_scores,
                const std::set<TraceGraph::Score>& scores,
                const std::map<AddressRange, Entry>& elf_entries,
                const filesystem::path& root,
                const std::string& file)
{
    if(!filesystem::exists(root))
        filesystem::create_directories(root);

    boost::filesystem::ofstream ofsg(root / file);
    label_writer<TraceGraph::Graph> vwriter(trace_graph, graph, trace_scores, scores, elf_entries);
    write_graphviz(ofsg, graph, vwriter, default_writer());
}

void write_file_total(const TraceGraph& trace_graph,
                      const TraceGraph::Graph& graph,
                      const TraceGraph::TraceScoreMap& trace_scores,
                      const std::set<TraceGraph::Score>& scores,
                      const std::map<AddressRange, Entry>& elf_entries)
{
    static size_t n = 0;

    write_file(trace_graph,
               graph,
               trace_scores,
               scores,
               elf_entries,
               filesystem::path("graph") / "tot", to_string(n) + ".dot");
    ++n;
}

void write_file_sub(const TraceGraph& trace_graph,
                    const TraceGraph::Graph& graph,
                    const Trace& trace)
{
    (void)trace_graph;
    (void)graph;
    (void)trace;
//    auto n = parse_trace_number(trace.get_id());

//    write_file(trace_graph, graph, filesystem::path("graph") / "sub", n + ".dot");
}

TraceGraph::TraceGraph()
{

}

bool TraceGraph::insert(const Trace& trace)
{
    auto g = make_graph(trace);

    if(traces_.size() == 0)
    {
        traces_.insert(trace);
        executed_traces_.insert({trace, false});
        graph_ = g;
        return true;
    }

// BO: xxx to disable duplication check on trace from trace_graph_
//    if(merge_trace(g))
    if(true)
    {
        traces_.insert(trace);
        executed_traces_.insert({trace, false});
    }
    else
    {
        return false;
    }

    mark_cache_vertices();

    return true;
}

void TraceGraph::insert_callback(std::function<void (Trace::ID)> cb)
{
    redundant_trace_cbs_.push_back(cb);
}

boost::optional<Trace> TraceGraph::next_bfs()
{
    if(graph_.vertex_set().empty())
    {
        return boost::optional<Trace>();
    }

    boost::optional<Trace> trace;

    try
    {
        bfs_execution_visitor vis(*this);
        breadth_first_search(graph_, vertex(0, graph_), visitor(vis));
    }
    catch(FoundTrace<decltype(VertexProperty::trace_id)>& found)
    {
        trace = boost::optional<Trace>(Trace(found.trace_, Trace::Blocks()));
        last_selected_ = trace->get_id();
    }

    return trace;
}

void TraceGraph::submit_executed(const Trace& trace)
{
    executed(trace, true);
    last_selected_ = trace.get_id();
}

void TraceGraph::print_graph(bool only_branches,
                             const TraceScoreMap& trace_scores,
                             const std::map<AddressRange, Entry>& elf_entries) const
{
    std::set<TraceGraph::Score> scores;

    for(const auto& t : trace_scores)
    {
        scores.insert(t.second);
    }

    if(only_branches)
    {
        auto branch_only_graph = compress_to_branches(graph_);
        write_file_total(*this, branch_only_graph, trace_scores, scores, elf_entries);
    }
    else
    {
        write_file_total(*this, graph_, trace_scores, scores, elf_entries);
    }
}

std::string TraceGraph::last_selected() const
{
    return last_selected_;
}

optional<Trace> TraceGraph::select_by_recursive_score(std::function<Score(const Blocks&)> calculate_score) const
{
    auto vs = vertices(graph_);

    auto root = *vs.first;

    auto trace = select_by_recursive_score(calculate_score, root, graph_);

    return trace;
}

optional<Trace> TraceGraph::select_by_recursive_score(std::function<Score(const Blocks&)> calculate_score,
                                            const Vertex& root,
                                            const Graph& graph) const
{
    auto odegree = out_degree(root, graph);

    auto leaf = bool{odegree == 0};
    if(leaf)
    {
        auto trace = Trace{ graph[root].trace_id , Blocks{} };

        if(!executed(trace))
            return trace;

        return optional<Trace>{};
    }

    auto edges = out_edges(root, graph);

    auto nonbranch = bool{odegree == 1};
    if(nonbranch)
    {
        return select_by_recursive_score(calculate_score,
                                         target(*edges.first, graph),
                                         graph);
    }

    auto branch_scores = BranchScoreMap{};

    for(;
        edges.first != edges.second;
        ++edges.first)
    {
        auto tv = target(*edges.first, graph);

        auto blocks = Blocks{};

        collect_descendant_blocks(tv, graph, blocks);

        auto score = calculate_score(blocks);

        branch_scores.insert(std::make_pair(score,
                                            tv));
    }


    for(const auto& bs : branch_scores)
    {
        auto lowest_scoring = bs.second;

        auto trace = select_by_recursive_score(calculate_score, lowest_scoring, graph);
        if(trace)
            return trace;
    }

    return optional<Trace>{}; // All child traces have been executed.
}

void TraceGraph::collect_descendant_blocks(const TraceGraph::Vertex& root,
                                           const TraceGraph::Graph& graph,
                                           TraceGraph::Blocks& blocks) const
{
    const auto& block = graph[root].unique_hash;

    blocks.push_back(block);

    auto edges = out_edges(root, graph);

    for(;
        edges.first != edges.second;
        ++edges.first)
    {
        auto tv = target(*edges.first, graph);

        collect_descendant_blocks(tv, graph, blocks);
    }
}

bool TraceGraph::is_subgraph(const TraceGraph::Graph& trace, const TraceGraph::Vertex& divergence)
{
    if(out_degree(divergence, trace) == 0)
        return true;
    return false;
}

bool TraceGraph::is_supergraph_prev_trace(const TraceGraph::Graph& trace,
                                          const TraceGraph::Vertex& trace_diverge,
                                          const TraceGraph::Vertex& graph_diverge)
{
    if(out_degree(graph_diverge, graph_) == 0 &&
       out_degree(trace_diverge, trace) != 0)
        return true;

    return false;
}

void TraceGraph::back_insert(const std::string& trace_id,
                             const std::string& prev_trace_id,
                             const std::vector<Vertex>& path)
{
    for(auto& v : adaptors::reverse(path))
    {
        if(graph_[v].trace_id == prev_trace_id)
        {
            graph_[v].trace_id = trace_id;
//            graph_[v].block = 0; //"superceded<block_name_here>"; // TODO: extract the new, corresponding trace[v].block.
        }
        else
        {
            return;
        }
    }
}

bool TraceGraph::merge_trace(const TraceGraph::Graph& trace)
{
//    using IndexMap = property_map<Graph, vertex_index_t>::type;
//    IndexMap index = get(vertex_index, graph);

    auto tvs = vertices(trace);
    auto evs = vertices(graph_);

    if(tvs.first == tvs.second ||
       evs.first == evs.second)
        return false;

    auto tv = *tvs.first;
    auto ev = *evs.first;

    assert(trace[tv] == graph_[ev]); // First vertex is incorrect (should never happen, if my thinking is correct).

    std::vector<Vertex> path;
    Vertex trace_diverge, existing_diverge;
    std::tie(trace_diverge, existing_diverge) = find_trace_divergence(trace, tv, ev, path);

    if(is_subgraph(trace, trace_diverge))
    {
        return false;
    }
    else if(is_supergraph_prev_trace(trace, trace_diverge, existing_diverge))
    {
        auto prev_trace_id = graph_[existing_diverge].trace_id;

        back_insert(trace[trace_diverge].trace_id, prev_trace_id, path);
        remove(Trace(prev_trace_id, Trace::Blocks()));
        std::cout << "Supergraph found. Need to remove subtrace from pool: " << prev_trace_id << " replaced by: " << trace[trace_diverge].trace_id << std::endl;
        for(auto& cb : redundant_trace_cbs_)
        {
            cb(prev_trace_id);
        }
    }

    auto nexttv = target(*out_edges(trace_diverge, trace).first, trace);

    merge_trace(trace, nexttv, existing_diverge);

//    back_insert_block_info(trace, path);

    return true;
}

void TraceGraph::merge_trace(const TraceGraph::Graph& trace,
                             const TraceGraph::Vertex& trace_diverge,
                             const TraceGraph::Vertex& existing_diverge)
{
    // Note: maybe there's a provided way to merge the trace to graph_, but I didn't find it.
    auto u = add_vertex(graph_);
    graph_[u] = trace[trace_diverge];
    add_edge(existing_diverge, u, graph_);

    auto tedges = out_edges(trace_diverge, trace);

    if(tedges.first == tedges.second)
        return;

    auto nextv = target(*tedges.first, trace);

    merge_trace(trace, nextv, u);
}

std::pair<TraceGraph::Vertex, TraceGraph::Vertex> TraceGraph::find_trace_divergence(
        const TraceGraph::Graph& trace,
        Vertex& tv,
        Vertex& ev,
        std::vector<Vertex>& path)
{
    path.push_back(ev);
    auto tv_edges = out_edges(tv, trace);

    if(tv_edges.first == tv_edges.second) // At the end of trace - no child vertex. Trace is a subgraph!
        return {tv, ev};

    auto nexttv = target(*tv_edges.first, trace);
    auto edges = out_edges(ev, graph_);

    for(; edges.first != edges.second; ++edges.first)
    {
        auto targetv = target(*edges.first, graph_);
        if(trace[nexttv] == graph_[targetv])
        {

            return find_trace_divergence(trace, nexttv, targetv, path);
        }
    }

    return {tv, ev}; // Reached when trace is not a subgraph.
}

void TraceGraph::mark_cache_vertices()
{
    auto vs = vertices(graph_);

    for(; vs.first != vs.second; ++vs.first)
    {
        auto v = *vs.first;
        if(out_degree(v, graph_) > 1)
        {
            graph_[v].cache = true;
        }
    }
}

void TraceGraph::remove(const Trace& trace)
{
    traces_.erase(std::find(traces_.begin(), traces_.end(), trace));
    assert(executed_traces_.erase(trace) != 0);
}

TraceGraph::Graph TraceGraph::compress_to_branches(const TraceGraph::Graph& g) const
{
    auto vs = vertices(g);
    auto vfirst = *vs.first;
    auto compressed = Graph{};

    if(is_leaf_branch(vfirst, g))
    {
        auto nv = add_vertex(compressed);
        compressed[nv] = g[vfirst];
        return compressed;
    }

    auto next_v = find_next_branch_or_leaf(vfirst, g);
    auto nv = add_vertex(compressed);
    compressed[nv] = g[next_v];

    auto edges = out_edges(next_v, g);

    for(; edges.first != edges.second; ++edges.first)
    {
        compress_to_branches(nv,
                             compressed,
                             target(*edges.first, g),
                             g);
    }

    return compressed;
}

void TraceGraph::compress_to_branches(const TraceGraph::Vertex& parent_comp,
                                      TraceGraph::Graph& g_comp,
                                      const Vertex& parent_orig,
                                      const Graph& g_orig) const
{
    if(is_leaf_branch(parent_orig, g_orig))
    {
        auto nv = add_vertex(g_comp);
        g_comp[nv] = g_orig[parent_orig];
        add_edge(parent_comp, nv, g_comp);
        return;
    }

    auto next_v = find_next_branch_or_leaf(parent_orig, g_orig);

    auto nv = add_vertex(g_comp);
    g_comp[nv] = g_orig[next_v];
    add_edge(parent_comp, nv, g_comp);

    auto edges = out_edges(next_v, g_orig);

    for(; edges.first != edges.second; ++edges.first)
    {
        auto t = target(*edges.first, g_orig);
        compress_to_branches(nv,
                             g_comp,
                             t,
                             g_orig);
    }
}

TraceGraph::Vertex TraceGraph::find_next_branch_or_leaf(const TraceGraph::Vertex& parent,
                                                        const TraceGraph::Graph& g) const
{
    auto degree = out_degree(parent, g);

    if(degree != 1)
        return parent;

    auto only_child = target(*out_edges(parent, g).first, g);

    return find_next_branch_or_leaf(only_child, g);
}

bool TraceGraph::is_leaf_branch(const TraceGraph::Vertex& v, const TraceGraph::Graph& g) const
{
    auto odegree = out_degree(v, g);
    if(odegree == 0)
        return true;
    else if(odegree != 1)
        return false;

    auto only_child = target(*out_edges(v, g).first, g);

    return is_leaf_branch(only_child, g);
}

//void TraceGraph::back_insert_block_info(const Trace& trace,
//                                        const std::vector<TraceGraph::Vertex>& path)
//{
//    const auto& blocks = trace.get_blocks();

//    assert(blocks.size() == path.size());

//    auto size = blocks.size();

//    for(auto i = 0u; i < size; ++i)
//    {
//        graph_[path[i]].block = blocks[i];
//    }
//}

bool operator==(const VertexProperty& rhs, const VertexProperty& lhs)
{
    return rhs.unique_hash == lhs.unique_hash;
}

bool operator!=(const VertexProperty& rhs, const VertexProperty& lhs)
{
    return !(rhs == lhs);
}

TraceGraph::Graph make_graph(const Trace& trace)
{
    auto size = trace.size();

    TraceGraph::Graph g(size);

    static auto count = 0u; // TODO: temporary - use trace.get_id() instead.

    for(size_t i = 1; i < size; ++i)
    {
        add_edge(i - 1, i, g);
    }
    for(size_t i = 0; i < size; ++i)
    {
        g[i].unique_hash = trace.get_blocks()[i];


//        g[i].trace_id = std::to_string(count); // TODO: temporary. Should use the actual id: trace.get_id();.
        g[i].trace_id = trace.get_id();
    }

    ++count;

    return g;
}

size_t crete::TraceHash::operator()(const crete::Trace& trace) const
{
    return std::hash<std::string>()(trace.get_id());
}

} // namespace crete
