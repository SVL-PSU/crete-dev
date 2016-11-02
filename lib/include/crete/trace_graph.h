#ifndef CRETE_TRACE_GRAPH_H
#define CRETE_TRACE_GRAPH_H

#include <boost/graph/adjacency_list.hpp>
#include <boost/filesystem/path.hpp>
#include <boost/unordered_set.hpp>
#include <boost/unordered_map.hpp>

#include <functional>

#include <crete/trace.h>
#include <crete/elf_reader.h>
#include <crete/addr_range.h>

namespace crete
{

struct VertexProperty
{
    Trace::Block unique_hash;
    std::string trace_id; // TODO: placing the trace ID for every vertex is expensive. Do the math. Say each ID is 32bytes. If a trace is 1 million blocks, that's 32MB per trace (minus merged portion). If necessary, use a UUID instead. It's only 16 bytes.
    bool cache = false; // TODO: remove. Redundant now that only using brcond blocks.
};

// For use by unordered_map.
struct TraceHash
{
    size_t operator()(const Trace& trace) const;
};

bool operator==(const VertexProperty& rhs, const VertexProperty& lhs);
bool operator!=(const VertexProperty& rhs, const VertexProperty& lhs);

class bfs_execution_visitor;

class TraceGraph
{
public:
    using Graph = boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS, VertexProperty>;
    using Vertex = boost::graph_traits<Graph>::vertex_descriptor;
    using Edge = boost::graph_traits<Graph>::edge_descriptor;
    using EdgeRange = std::pair<boost::graph_traits<Graph>::out_edge_iterator, boost::graph_traits<Graph>::out_edge_iterator>;
    using Traces = boost::unordered_set<Trace, TraceHash>;
    using ExecutedTraces = boost::unordered_map<Trace, bool, TraceHash>;
    using Weight = uint64_t;
    using Score = double;
    using TraceScoreMap = std::map<Trace, Score>;
    using Blocks = Trace::Blocks;
    using BranchScoreMap = std::map<Score, Vertex>;
    using Weights = std::vector<Weight>;

    friend class bfs_execution_visitor;

public:
    TraceGraph();

    bool insert(const Trace& trace);
    void insert_callback(std::function<void(Trace::ID)> cb);
    boost::optional<Trace> next_bfs(); // TODO: don't mark as executed. Let sumbit_executed be called for that. Marking the trace as executed implies that we know the returned trace will be executed.
    boost::optional<Trace> select_by_recursive_score(std::function<Score(const Blocks&)> calculate_score) const;
    void submit_executed(const Trace& trace);
    bool executed(const Trace& trace) const { return executed_traces_.at(trace); }
    bool executed(const decltype(VertexProperty::trace_id)& trace_id) const { return executed_traces_.at(Trace(trace_id, Trace::Blocks())); }
    void executed(const Trace& trace, bool exec) { executed_traces_.at(trace) = exec; }
    std::string last_selected() const;
    boost::optional<Trace> select_by_recursive_score(std::function<Score(const Blocks&)> calculate_score, const Vertex& root, const Graph& graph) const;
    void collect_descendant_blocks(const Vertex& root, const Graph& graph, Blocks& blocks) const;
    // Debugging
    void print_graph(bool only_branches, const TraceScoreMap& trace_scores, const std::map<AddressRange, Entry>& elf_entries) const;

protected:
    bool is_subgraph(const Graph& trace, const Vertex& divergence); // If the point of divergence is when the trace graph ends, then it's a subgraph.
    bool is_supergraph_prev_trace(const Graph& trace, const Vertex& trace_diverge, const Vertex& graph_diverge);
    void back_insert(const std::string& trace_id, const std::string& prev_trace_id, const std::vector<Vertex>& path); // Replaces id of trace over existing trace in graph, as the previous trace over that path has been superceded.
    bool merge_trace(const Graph& trace); // Doesn't merge if it's redundant or a subgraph.
    void merge_trace(const Graph& trace, const Vertex& trace_diverge, const Vertex& existing_diverge);
    std::pair<Vertex, Vertex> find_trace_divergence(const TraceGraph::Graph& trace, Vertex& tv, Vertex& ev, std::vector<Vertex>& path); // Returns <trace_vertex, graph_vertex> where the graphs diverge.
    void mark_cache_vertices();
    void remove(const Trace& trace);
    Graph compress_to_branches(const Graph& g) const;
    void compress_to_branches(const Vertex& parent_comp,
                              Graph& g_comp,
                              const Vertex& parent_orig,
                              const Graph& g_orig) const;
    Vertex find_next_branch_or_leaf(const Vertex& parent, const Graph& g) const;
    bool is_leaf_branch(const Vertex& v, const Graph& g) const;
//        void back_insert_block_info(const Trace& trace, const std::vector<Vertex>& path); // TODO: is back_insert a misnomer? I _could_ back insert, but I could also forward insert under the current impl.

private:
    Graph graph_;
    Traces traces_;
    ExecutedTraces executed_traces_; // Only relevant if bfs selection enabled.
    std::vector<std::function<void(Trace::ID)>> redundant_trace_cbs_;
    std::string last_selected_; // Debugging info.
};

TraceGraph::Graph make_graph(const Trace& trace);
std::string parse_trace_number(const Trace::ID& id);
std::string parse_iteration_number(const Trace::ID& id);
std::string parse_tb_number(const Trace::ID& id);

} // namespace crete

#endif // CRETE_TRACE_GRAPH_H
