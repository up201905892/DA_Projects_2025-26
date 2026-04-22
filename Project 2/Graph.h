/**
 * @file Graph.h
 * @brief Interference graph and web construction for register allocation.
 *
 * Webs are built by merging overlapping live ranges of the same variable.
 * The interference graph has one node per web; edges connect webs whose
 * live-line sets intersect (i.e., they are simultaneously alive).
 */

#pragma once
#include "Parser.h"
#include <set>
#include <vector>
#include <string>
#include <unordered_map>

// ---------------------------------------------------------------------------
// Web
// ---------------------------------------------------------------------------

/**
 * @brief A "live web" — the union of all overlapping live ranges for one
 *        variable that form a single connected liveness interval.
 *
 * Ranges within the web are sorted ascending by start line.
 */
struct Web {
    int                    id;      ///< 0-based index in the webs vector
    std::string            varName;
    std::vector<LiveRange> ranges;  ///< constituent live ranges (sorted)
    std::set<int>          lines;   ///< union of all live line numbers

    /**
     * @brief Add a live range and update the lines set.
     * @complexity O(R log R) where R = lines in the new range.
     */
    void addRange(const LiveRange& lr);

    /**
     * @brief Return a canonical string representation for output.
     *
     * Format: "start+,mid,...,end-" for each range, comma-joined.
     * @complexity O(L) where L = total lines in the web.
     */
    std::string toString() const;
};

// ---------------------------------------------------------------------------
// Web builder
// ---------------------------------------------------------------------------

/**
 * @brief Build webs from a flat list of range entries.
 *
 * Algorithm:
 *  1. Group entries by variable name — O(E log E).
 *  2. For each group, repeatedly merge any two ranges whose line sets
 *     intersect, until a fixed point is reached — O(R^2 * L) per variable
 *     where R = number of ranges and L = lines per range.
 *
 * @param entries Raw parsed range entries.
 * @return Vector of webs with IDs assigned in encounter order.
 * @complexity O(E^2 * L) worst case (all ranges for one variable overlap).
 */
std::vector<Web> buildWebs(const std::vector<RangeEntry>& entries);

// ---------------------------------------------------------------------------
// Graph
// ---------------------------------------------------------------------------

/**
 * @brief Undirected interference graph stored as an adjacency matrix.
 *
 * Node indices correspond to web indices in the webs vector.
 * An edge (u, v) means webs u and v are simultaneously alive at some line
 * and therefore cannot share a register.
 *
 * @note Adjacency matrix is chosen for O(1) edge queries, acceptable for
 *       typical web counts (< 200).
 */
class Graph {
public:
    /**
     * @brief Construct an empty graph with n nodes.
     * @complexity O(n^2) for matrix allocation.
     */
    explicit Graph(int n);

    /**
     * @brief Add an undirected edge between u and v.
     * @complexity O(1).
     */
    void addEdge(int u, int v);

    /**
     * @brief Check whether an edge exists.
     * @complexity O(1).
     */
    bool hasEdge(int u, int v) const;

    /**
     * @brief Degree of node u considering only nodes in `active`.
     * @complexity O(n).
     */
    int degree(int u, const std::set<int>& active) const;

    /**
     * @brief Full neighbor set of node u (regardless of active set).
     * @complexity O(n).
     */
    std::vector<int> neighbors(int u) const;

    /** @brief Number of nodes. @complexity O(1). */
    int size() const { return n_; }

private:
    int                          n_;
    std::vector<std::vector<bool>> adj_; ///< n_ × n_ adjacency matrix
};

// ---------------------------------------------------------------------------
// Graph builder
// ---------------------------------------------------------------------------

/**
 * @brief Build the interference graph from a web list.
 *
 * Two webs interfere iff their line sets share at least one element.
 *
 * @complexity O(W^2 * L) where W = number of webs, L = average lines per web.
 */
Graph buildInterferenceGraph(const std::vector<Web>& webs);
