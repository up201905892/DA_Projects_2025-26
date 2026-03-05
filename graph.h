/**
 * @file graph.h
 * @brief Generic flow network graph with Edmonds-Karp max-flow solver.
 *
 * Provides three distinct components:
 *   1. **Graph** — Generic directed graph with adjacency-list representation
 *      and the Edmonds-Karp algorithm (BFS-based Ford-Fulkerson).
 *   2. **NetworkBuilder** — Constructs the bipartite flow network from
 *      the parsed ProblemData, mapping domain entities to graph nodes.
 *   3. **Solver** — Orchestrates network construction, max-flow computation,
 *      and extraction of assignments from the flow solution.
 *
 * @par Separation of Concerns:
 * The Graph class knows nothing about submissions or reviewers — it only
 * works with integer node IDs and edge capacities. The NetworkBuilder
 * encapsulates the mapping between the problem domain and the graph.
 * This separation is critical for T2.2 (Risk Analysis), where we need
 * to rebuild/modify the network and re-run the solver.
 *
 * @author DA 2026 - Programming Project I
 * @date Spring 2026
 * @version 1.0
 */

#ifndef GRAPH_H
#define GRAPH_H

#include "domain.h"

#include <vector>
#include <queue>
#include <algorithm>
#include <climits>
#include <cassert>

// ============================================================================
// Edge structure
// ============================================================================

/**
 * @brief Represents a directed edge in the flow network.
 *
 * Each edge stores its destination, capacity, current flow, and an index
 * to its reverse edge in the destination's adjacency list. This reverse-edge
 * pattern is the standard representation for residual graphs used in
 * Ford-Fulkerson-family algorithms.
 *
 * @par Residual capacity:
 * \f$ c_f(u,v) = \text{capacity} - \text{flow} \f$
 *
 * @par Reverse edge convention:
 * When adding edge (u→v, cap), we simultaneously add (v→u, 0) as the
 * reverse. The `rev` field stores the index of the reverse edge in the
 * adjacency list of the destination node.
 */
struct Edge {
    int dest;       ///< Destination node index.
    int capacity;   ///< Maximum capacity of this edge.
    int flow;       ///< Current flow through this edge.
    int rev;        ///< Index of the reverse edge in adj[dest].

    /**
     * @brief Residual capacity of this edge.
     * @return capacity - flow.
     */
    [[nodiscard]] int residual() const { return capacity - flow; }
};

// ============================================================================
// Graph — Generic flow network with Edmonds-Karp
// ============================================================================

/**
 * @brief Directed graph with adjacency-list representation and Edmonds-Karp max-flow.
 *
 * This class is domain-agnostic — it only knows about integer node IDs
 * and edge capacities. The mapping between problem entities (reviewers,
 * submissions) and node IDs is handled externally by NetworkBuilder.
 *
 * @par Graph representation:
 * Adjacency list using `std::vector<std::vector<Edge>>`. Each edge stores
 * a pointer (index) to its reverse edge for O(1) residual updates.
 *
 * @par Complexity (Edmonds-Karp):
 * - Time:  \f$O(|V| \cdot |E|^2)\f$ (theoretical worst case)
 * - Space: \f$O(|V| + |E|)\f$ (adjacency list + BFS auxiliary structures)
 *
 * @par Why Edmonds-Karp over Dinic's?
 * For the expected input sizes (N ≤ 1000), Edmonds-Karp is sufficient and
 * simpler to implement correctly. Its O(VE²) bound, combined with the fact
 * that our graph has unit-capacity middle edges, makes it efficient in practice.
 */
class Graph {
public:
    /**
     * @brief Constructs an empty graph with a given number of nodes.
     *
     * @param numNodes Total number of nodes (including Source and Sink).
     *
     * @par Complexity
     * \f$O(|V|)\f$
     */
    explicit Graph(int numNodes) : adj_(numNodes) {}

    /**
     * @brief Returns the number of nodes in the graph.
     * @return Node count.
     */
    [[nodiscard]] int numNodes() const { return static_cast<int>(adj_.size()); }

    /**
     * @brief Returns the adjacency list of a given node (const).
     * @param u Node index.
     * @return Reference to the vector of edges leaving node u.
     */
    [[nodiscard]] const std::vector<Edge>& getAdj(int u) const { return adj_[u]; }

    /**
     * @brief Returns the adjacency list of a given node (mutable).
     * @param u Node index.
     * @return Reference to the vector of edges leaving node u.
     */
    std::vector<Edge>& getAdj(int u) { return adj_[u]; }

    /**
     * @brief Adds a directed edge (u → v) with the given capacity,
     *        along with its reverse edge (v → u) with capacity 0.
     *
     * This is the standard way to build a flow network for Ford-Fulkerson:
     * the reverse edge with capacity 0 represents the "undo" possibility
     * in the residual graph.
     *
     * @param u   Source node of the edge.
     * @param v   Destination node of the edge.
     * @param cap Capacity of the forward edge.
     *
     * @pre 0 <= u, v < numNodes()
     * @pre cap >= 0
     *
     * @par Complexity
     * Amortized \f$O(1)\f$.
     */
    void addEdge(int u, int v, int cap) {
        assert(u >= 0 && u < numNodes());
        assert(v >= 0 && v < numNodes());
        assert(cap >= 0);

        // Forward edge: u → v with capacity cap
        Edge forward{v, cap, 0, static_cast<int>(adj_[v].size())};
        // Reverse edge: v → u with capacity 0
        Edge backward{u, 0, 0, static_cast<int>(adj_[u].size())};

        adj_[u].push_back(forward);
        adj_[v].push_back(backward);
    }

    /**
     * @brief Computes the maximum flow from source to sink using Edmonds-Karp.
     *
     * The Edmonds-Karp algorithm is a BFS-based implementation of the
     * Ford-Fulkerson method. By always choosing the shortest augmenting
     * path (in terms of number of edges), it guarantees polynomial time.
     *
     * @param source Index of the source node (S).
     * @param sink   Index of the sink node (T).
     * @return The maximum flow value \f$|f^*|\f$.
     *
     * @par Algorithm:
     * 1. While a BFS from source finds a path to sink in the residual graph:
     *    a. Compute bottleneck = min residual capacity along the path.
     *    b. Augment flow along the path by bottleneck.
     *    c. Update reverse edges (residual graph).
     * 2. Return total flow pushed.
     *
     * @par Complexity
     * - Time:  \f$O(|V| \cdot |E|^2)\f$
     * - Space: \f$O(|V| + |E|)\f$ for BFS parent tracking.
     *
     * @note After this method returns, the flow values on each edge
     *       represent the optimal flow. Use getAdj() to inspect them.
     */
    int edmondsKarp(int source, int sink) {
        int totalFlow = 0;

        // parent[v] = {node_u, edge_index_in_adj[u]} that leads to v in BFS
        std::vector<std::pair<int, int>> parent(numNodes());

        while (true) {
            // --- BFS to find shortest augmenting path ---
            int bottleneck = bfs(source, sink, parent);
            if (bottleneck == 0) break; // No augmenting path found

            // --- Augment flow along the path ---
            totalFlow += bottleneck;
            int v = sink;
            while (v != source) {
                auto [u, edgeIdx] = parent[v];
                adj_[u][edgeIdx].flow += bottleneck;
                adj_[v][adj_[u][edgeIdx].rev].flow -= bottleneck;
                v = u;
            }
        }

        return totalFlow;
    }

    /**
     * @brief Resets all flows to zero, allowing the graph to be re-solved.
     *
     * Useful for T2.2 Risk Analysis: after modifying the network (e.g.,
     * disabling a reviewer), we reset flows and re-run Edmonds-Karp.
     *
     * @par Complexity
     * \f$O(|V| + |E|)\f$
     */
    void resetFlows() {
        for (auto& edges : adj_) {
            for (auto& e : edges) {
                e.flow = 0;
            }
        }
    }

    /**
     * @brief Disables all edges from a specific node by setting capacity to 0.
     *
     * Used for Risk Analysis: to simulate reviewer r_j not doing any
     * reviews, we set C(S, R_j) = 0 (and reset flows), then re-run.
     *
     * @param node The node to disable.
     *
     * @par Complexity
     * \f$O(\text{deg}(node))\f$
     */
    void disableNode(int node) {
        for (auto& e : adj_[node]) {
            e.capacity = 0;
            e.flow = 0;
        }
        // Also disable incoming edges (reverse directions)
        // We need to find all edges pointing TO this node
        for (int u = 0; u < numNodes(); u++) {
            for (auto& e : adj_[u]) {
                if (e.dest == node) {
                    e.capacity = 0;
                    e.flow = 0;
                }
            }
        }
    }

    /**
     * @brief Restores all edge capacities from saved originals.
     *
     * Called after risk analysis to restore the graph to its original state.
     *
     * @param original The original graph (before any node disabling).
     *
     * @pre original.numNodes() == this->numNodes()
     *
     * @par Complexity
     * \f$O(|V| + |E|)\f$
     */
    void restoreFrom(const Graph& original) {
        assert(original.numNodes() == numNodes());
        for (int u = 0; u < numNodes(); u++) {
            assert(adj_[u].size() == original.adj_[u].size());
            for (size_t i = 0; i < adj_[u].size(); i++) {
                adj_[u][i].capacity = original.adj_[u][i].capacity;
                adj_[u][i].flow = 0;
            }
        }
    }

private:
    std::vector<std::vector<Edge>> adj_; ///< Adjacency list representation.

    /**
     * @brief BFS on the residual graph to find the shortest augmenting path.
     *
     * @param source  Source node.
     * @param sink    Sink node.
     * @param parent  Output: parent[v] = {u, edge_index} for path reconstruction.
     * @return Bottleneck capacity of the path (0 if no path exists).
     *
     * @par Complexity
     * \f$O(|V| + |E|)\f$
     */
    int bfs(int source, int sink, std::vector<std::pair<int, int>>& parent) {
        std::fill(parent.begin(), parent.end(), std::make_pair(-1, -1));
        parent[source] = {source, -1};

        // BFS queue stores {node, bottleneck_so_far}
        std::queue<std::pair<int, int>> q;
        q.push({source, INT_MAX});

        while (!q.empty()) {
            auto [u, pathFlow] = q.front();
            q.pop();

            for (int i = 0; i < static_cast<int>(adj_[u].size()); i++) {
                Edge& e = adj_[u][i];
                // Only traverse edges with positive residual capacity
                // and unvisited destinations
                if (parent[e.dest].first == -1 && e.residual() > 0) {
                    parent[e.dest] = {u, i};
                    int newFlow = std::min(pathFlow, e.residual());

                    if (e.dest == sink) {
                        return newFlow; // Found augmenting path
                    }
                    q.push({e.dest, newFlow});
                }
            }
        }

        return 0; // No augmenting path to sink
    }
};

// ============================================================================
// NetworkBuilder — Constructs the flow network from ProblemData
// ============================================================================

/**
 * @brief Builds the bipartite flow network from parsed problem data.
 *
 * This class encapsulates the mapping between the problem domain (reviewers,
 * submissions) and the graph node indices. It is the ONLY place where
 * the node-index convention is defined:
 *
 * @verbatim
 *   Node 0           → Super-Source (S)
 *   Node 1           → Super-Sink (T)
 *   Nodes [2, m+1]   → Reviewers  (offset = 2)
 *   Nodes [m+2, m+n+1] → Submissions (offset = m + 2)
 * @endverbatim
 *
 * @par Design decision: Direction S → Reviewers → Submissions → T
 * - C(S, R_j) = MaxRev enforces the per-reviewer upper bound.
 * - C(R_j, P_i) = 1 enforces binary assignment + domain matching.
 * - C(P_i, T) = MinRev enforces per-submission demand.
 */
class NetworkBuilder {
public:
    static constexpr int SOURCE = 0;   ///< Index of the super-source node.
    static constexpr int SINK   = 1;   ///< Index of the super-sink node.

    /**
     * @brief Builds the complete flow network.
     *
     * @param data Parsed problem data (submissions, reviewers, config).
     * @return A Graph ready for Edmonds-Karp, with all edges and capacities set.
     *
     * @par Edge construction summary:
     * 1. For each reviewer j:  S → R_j with capacity MaxRev.
     * 2. For each (reviewer j, submission i) where domain matches:
     *    R_j → P_i with capacity 1.
     * 3. For each submission i:  P_i → T with capacity MinRev.
     *
     * @par Complexity
     * - Time:  \f$O(m \cdot n)\f$ (iterating all reviewer-submission pairs).
     * - Space: \f$O(m + n + E_{\text{match}})\f$.
     */
    static Graph buildNetwork(const ProblemData& data) {
        int m = static_cast<int>(data.numReviewers());
        int n = static_cast<int>(data.numSubmissions());
        int totalNodes = 2 + m + n; // Source + Sink + Reviewers + Submissions

        Graph g(totalNodes);

        const Config& cfg = data.config;
        bool useSubSecondary = cfg.useSubmissionSecondary();
        bool useRevSecondary = cfg.useReviewerSecondary();

        // --- Layer 1: Source → Reviewers ---
        for (int j = 0; j < m; j++) {
            int revNode = reviewerNode(j);
            g.addEdge(SOURCE, revNode, cfg.maxReviewsPerReviewer);
        }

        // --- Layer 2: Reviewers → Submissions (compatibility edges) ---
        for (int j = 0; j < m; j++) {
            const Reviewer& rev = data.reviewers[j];
            int revNode = reviewerNode(j);

            for (int i = 0; i < n; i++) {
                const Submission& sub = data.submissions[i];

                if (isCompatible(rev, sub, useRevSecondary, useSubSecondary)) {
                    int subNode = submissionNode(i, m);
                    g.addEdge(revNode, subNode, 1);
                }
            }
        }

        // --- Layer 3: Submissions → Sink ---
        for (int i = 0; i < n; i++) {
            int subNode = submissionNode(i, m);
            g.addEdge(subNode, SINK, cfg.minReviewsPerSubmission);
        }

        return g;
    }

    // --- Index mapping functions ---

    /**
     * @brief Maps a reviewer's vector index to its graph node ID.
     * @param reviewerIdx Index in ProblemData::reviewers (0-based).
     * @return Graph node index.
     */
    static int reviewerNode(int reviewerIdx) {
        return 2 + reviewerIdx;
    }

    /**
     * @brief Maps a submission's vector index to its graph node ID.
     * @param submissionIdx Index in ProblemData::submissions (0-based).
     * @param numReviewers  Total number of reviewers (m).
     * @return Graph node index.
     */
    static int submissionNode(int submissionIdx, int numReviewers) {
        return 2 + numReviewers + submissionIdx;
    }

    /**
     * @brief Inverse: given a graph node ID, returns the reviewer vector index.
     * @param node Graph node ID.
     * @return Reviewer index (0-based), or -1 if node is not a reviewer.
     */
    static int nodeToReviewerIdx(int node, int numReviewers) {
        int idx = node - 2;
        if (idx >= 0 && idx < numReviewers) return idx;
        return -1;
    }

    /**
     * @brief Inverse: given a graph node ID, returns the submission vector index.
     * @param node Graph node ID.
     * @param numReviewers  Total number of reviewers (m).
     * @param numSubmissions Total number of submissions (n).
     * @return Submission index (0-based), or -1 if node is not a submission.
     */
    static int nodeToSubmissionIdx(int node, int numReviewers, int numSubmissions) {
        int idx = node - 2 - numReviewers;
        if (idx >= 0 && idx < numSubmissions) return idx;
        return -1;
    }

private:
    /**
     * @brief Determines if a reviewer can review a submission based on domain matching.
     *
     * The match depends on the current MatchMode (controlled by Config flags):
     * - A reviewer's primary (and optionally secondary) expertise is checked
     *   against the submission's primary (and optionally secondary) domain.
     * - A match exists if ANY active reviewer expertise equals ANY active
     *   submission domain.
     *
     * @param rev           The reviewer.
     * @param sub           The submission.
     * @param useRevSec     Consider reviewer's secondary expertise.
     * @param useSubSec     Consider submission's secondary domain.
     * @return true if at least one domain-expertise pair matches.
     *
     * @par Complexity
     * \f$O(1)\f$ — at most 4 comparisons (2 expertise × 2 domains).
     */
    static bool isCompatible(const Reviewer& rev, const Submission& sub,
                             bool useRevSec, bool useSubSec) {
        // Check primary expertise against submission domains
        if (sub.hasDomain(rev.primaryExpertise, true, useSubSec)) return true;

        // Check secondary expertise against submission domains
        if (useRevSec && rev.secondaryExpertise.has_value()) {
            if (sub.hasDomain(rev.secondaryExpertise.value(), true, useSubSec)) return true;
        }

        return false;
    }
};

// ============================================================================
// Solver — Orchestrates network construction, max-flow, and extraction
// ============================================================================

/**
 * @brief High-level solver that ties together parsing, network building,
 *        max-flow computation, and result extraction.
 *
 * Provides the main entry point for T2.1 (basic assignment) and T2.2
 * (single-reviewer risk analysis).
 */
class Solver {
public:
    /**
     * @brief Solves the assignment problem for the given data.
     *
     * Builds the flow network, runs Edmonds-Karp, and extracts the
     * assignment from the flow values.
     *
     * @param data Parsed problem data.
     * @return SolverResult with assignments, unfulfilled list, and max-flow value.
     *
     * @par Complexity
     * - Network build: \f$O(m \cdot n)\f$
     * - Edmonds-Karp:  \f$O(|V| \cdot |E|^2)\f$
     * - Extraction:    \f$O(m \cdot \text{deg}_{\text{avg}})\f$
     */
    static SolverResult solve(const ProblemData& data) {
        int m = static_cast<int>(data.numReviewers());
        int n = static_cast<int>(data.numSubmissions());

        Graph g = NetworkBuilder::buildNetwork(data);
        int maxFlow = g.edmondsKarp(NetworkBuilder::SOURCE, NetworkBuilder::SINK);

        return extractResult(g, data, m, n, maxFlow);
    }

    /**
     * @brief Performs Risk Analysis with parameter K = 1.
     *
     * For each reviewer r_j, simulates their absence by disabling their
     * node, resetting flows, and re-running Edmonds-Karp. If the new
     * max-flow < n * MinRev, the reviewer is "risky".
     *
     * @param data Parsed problem data.
     * @return SolverResult with the riskyReviewers field populated.
     *
     * @par Algorithm:
     * 1. Build the network once and save a copy of the original.
     * 2. For each reviewer j (0..m-1):
     *    a. Disable node R_j (set all its edge capacities to 0).
     *    b. Reset all flows to 0.
     *    c. Run Edmonds-Karp.
     *    d. If maxFlow < requiredFlow, mark reviewer as risky.
     *    e. Restore the graph from the original copy.
     *
     * @par Complexity
     * \f$O(m \cdot |V| \cdot |E|^2)\f$ — one full Edmonds-Karp per reviewer.
     * In practice, much faster because most iterations terminate early.
     */
    static SolverResult solveWithRiskAnalysis(const ProblemData& data) {
        int m = static_cast<int>(data.numReviewers());
        int n = static_cast<int>(data.numSubmissions());
        int requiredFlow = n * data.config.minReviewsPerSubmission;

        // First: solve the base problem
        Graph g = NetworkBuilder::buildNetwork(data);
        int baseFlow = g.edmondsKarp(NetworkBuilder::SOURCE, NetworkBuilder::SINK);

        SolverResult result = extractResult(g, data, m, n, baseFlow);

        // If base assignment already fails, no point in risk analysis
        if (baseFlow < requiredFlow) {
            return result;
        }

        // Save original graph for restoration
        Graph original = NetworkBuilder::buildNetwork(data);

        // Test each reviewer's removal
        for (int j = 0; j < m; j++) {
            // Rebuild fresh graph for each test
            Graph testGraph = NetworkBuilder::buildNetwork(data);

            // Disable reviewer j
            int revNode = NetworkBuilder::reviewerNode(j);
            testGraph.disableNode(revNode);

            // Run max-flow without this reviewer
            int newFlow = testGraph.edmondsKarp(NetworkBuilder::SOURCE, NetworkBuilder::SINK);

            if (newFlow < requiredFlow) {
                result.riskyReviewers.push_back(data.reviewers[j].id);
            }
        }

        // Sort risky reviewers for consistent output
        std::sort(result.riskyReviewers.begin(), result.riskyReviewers.end());

        return result;
    }

private:
    /**
     * @brief Extracts the assignment from the solved flow network.
     *
     * Iterates over all reviewer nodes, inspects their outgoing edges
     * to submission nodes, and records those with flow = 1 as assignments.
     * Also identifies submissions that received fewer reviews than MinRev.
     *
     * @param g       The solved graph (with flows set by Edmonds-Karp).
     * @param data    Parsed problem data (for ID mapping).
     * @param m       Number of reviewers.
     * @param n       Number of submissions.
     * @param maxFlow The max-flow value.
     * @return SolverResult with populated assignments and unfulfilled lists.
     *
     * @par Complexity
     * \f$O(m \cdot \text{deg}_{\text{avg}} + n)\f$
     */
    static SolverResult extractResult(const Graph& g, const ProblemData& data,
                                      int m, int n, int maxFlow) {
        SolverResult result;
        result.maxFlow = maxFlow;

        // Track how many reviews each submission received
        std::vector<int> reviewCount(n, 0);

        // Scan all reviewer nodes for edges with positive flow
        for (int j = 0; j < m; j++) {
            int revNode = NetworkBuilder::reviewerNode(j);
            const auto& edges = g.getAdj(revNode);

            for (const Edge& e : edges) {
                // Only forward edges to submission nodes with flow > 0
                if (e.flow <= 0 || e.capacity == 0) continue;

                int subIdx = NetworkBuilder::nodeToSubmissionIdx(e.dest, m, n);
                if (subIdx < 0) continue; // Not a submission node (e.g., reverse to Source)

                // Determine the matched domain
                DomainCode matchedDomain = findMatchedDomain(
                    data.reviewers[j], data.submissions[subIdx], data.config);

                Assignment asgn;
                asgn.submissionId  = data.submissions[subIdx].id;
                asgn.reviewerId    = data.reviewers[j].id;
                asgn.matchedDomain = matchedDomain;
                result.assignments.push_back(asgn);

                reviewCount[subIdx]++;
            }
        }

        // Identify unfulfilled submissions
        for (int i = 0; i < n; i++) {
            int missing = data.config.minReviewsPerSubmission - reviewCount[i];
            if (missing > 0) {
                UnfulfilledSubmission u;
                u.submissionId  = data.submissions[i].id;
                u.domain        = data.submissions[i].primaryDomain;
                u.missingReviews = missing;
                result.unfulfilled.push_back(u);
            }
        }

        // Sort for deterministic output
        std::sort(result.assignments.begin(), result.assignments.end());
        std::sort(result.unfulfilled.begin(), result.unfulfilled.end());

        return result;
    }

    /**
     * @brief Determines which domain code caused the match between a reviewer and submission.
     *
     * Checks in priority order: primary expertise vs primary domain, then secondary
     * combinations based on the active MatchMode.
     *
     * @param rev  The reviewer.
     * @param sub  The submission.
     * @param cfg  Configuration (determines which domains are active).
     * @return The domain code that caused the match.
     *
     * @par Complexity
     * \f$O(1)\f$
     */
    static DomainCode findMatchedDomain(const Reviewer& rev, const Submission& sub,
                                        const Config& cfg) {
        // Priority 1: Primary expertise matches primary domain
        if (rev.primaryExpertise == sub.primaryDomain) {
            return sub.primaryDomain;
        }

        // Priority 2: Primary expertise matches secondary domain
        if (cfg.useSubmissionSecondary() &&
            sub.secondaryDomain.has_value() &&
            rev.primaryExpertise == sub.secondaryDomain.value()) {
            return sub.secondaryDomain.value();
        }

        // Priority 3: Secondary expertise matches primary domain
        if (cfg.useReviewerSecondary() &&
            rev.secondaryExpertise.has_value() &&
            rev.secondaryExpertise.value() == sub.primaryDomain) {
            return sub.primaryDomain;
        }

        // Priority 4: Secondary expertise matches secondary domain
        if (cfg.useReviewerSecondary() && cfg.useSubmissionSecondary() &&
            rev.secondaryExpertise.has_value() &&
            sub.secondaryDomain.has_value() &&
            rev.secondaryExpertise.value() == sub.secondaryDomain.value()) {
            return sub.secondaryDomain.value();
        }

        // Fallback (should not happen if the edge exists)
        return sub.primaryDomain;
    }
};

#endif // GRAPH_H
