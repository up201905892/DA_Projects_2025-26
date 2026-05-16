/**
 * @file Graph.h
 * @brief Interference graph and web construction for register allocation.
 *
 * Webs are built by merging overlapping live ranges of the same variable.
 * The interference graph has one node per web; edges connect webs whose
 * live ranges overlap at a program point where both values are simultaneously
 * alive.
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
 * @brief A live web — the union of all overlapping live ranges for one variable.
 *
 * A web represents a value or group of connected values of the same variable
 * whose live ranges intersect. Each web becomes a node in the interference graph.
 */
struct Web {
    int id;                         ///< 0-based index in the webs vector
    std::string varName;            ///< variable associated with this web
    std::vector<LiveRange> ranges;  ///< constituent live ranges
    std::set<int> lines;            ///< union of all live program points

    /**
     * @brief Add a live range to this web and update its line set.
     *
     * @param lr Live range to add.
     * @complexity O(R log L), where R is the number of points in the range and
     *             L is the number of points already stored in the web.
     */
    void addRange(const LiveRange& lr);

    /**
     * @brief Return a canonical string representation for output.
     *
     * The output preserves marker information:
     *  - '+' is printed only if the original range had a definition marker;
     *  - '-' is printed only if the original range had a last-use marker.
     *
     * @return Human-readable representation of this web.
     * @complexity O(L log L), where L is the total number of points in the web.
     */
    std::string toString() const;
};

// ---------------------------------------------------------------------------
// Web builder
// ---------------------------------------------------------------------------

/**
 * @brief Build webs from parsed range entries.
 *
 * Algorithm:
 *  1. Group live ranges by variable name.
 *  2. For each variable, repeatedly merge groups of ranges that share at least
 *     one program point.
 *  3. Each remaining group becomes one web.
 *
 * @param entries Raw parsed range entries.
 * @return Vector of webs with IDs assigned from 0 to W-1.
 * @complexity O(E^2 * L), where E is the number of range entries and L is the
 *             average number of points per range.
 */
std::vector<Web> buildWebs(const std::vector<RangeEntry>& entries);

// ---------------------------------------------------------------------------
// Graph
// ---------------------------------------------------------------------------

/**
 * @brief Undirected interference graph stored as an adjacency matrix.
 *
 * Node indices correspond to web indices in the webs vector.
 * An edge (u, v) means webs u and v cannot share the same register.
 */
class Graph {
public:
    /**
     * @brief Construct an empty graph with n nodes.
     *
     * @param n Number of graph nodes.
     * @complexity O(n^2), due to adjacency matrix allocation.
     */
    explicit Graph(int n);

    /**
     * @brief Add an undirected edge between two nodes.
     *
     * @param u First node.
     * @param v Second node.
     * @complexity O(1).
     */
    void addEdge(int u, int v);

    /**
     * @brief Check whether an edge exists.
     *
     * @param u First node.
     * @param v Second node.
     * @return true if an edge exists, false otherwise.
     * @complexity O(1).
     */
    bool hasEdge(int u, int v) const;

    /**
     * @brief Degree of node u considering only nodes in the active set.
     *
     * @param u Node whose degree is requested.
     * @param active Set of active nodes.
     * @return Number of active neighbors of u.
     * @complexity O(A), where A is the size of the active set.
     */
    int degree(int u, const std::set<int>& active) const;

    /**
     * @brief Full neighbor set of node u.
     *
     * @param u Node whose neighbors are requested.
     * @return Vector with all neighbors of u.
     * @complexity O(n), where n is the number of graph nodes.
     */
    std::vector<int> neighbors(int u) const;

    /**
     * @brief Number of graph nodes.
     *
     * @return Number of nodes.
     * @complexity O(1).
     */
    int size() const { return n_; }

private:
    int n_;                               ///< number of nodes
    std::vector<std::vector<bool>> adj_;  ///< adjacency matrix
};

// ---------------------------------------------------------------------------
// Graph builder
// ---------------------------------------------------------------------------

/**
 * @brief Build the interference graph from a web list.
 *
 * Two webs interfere if they are simultaneously alive at some program point.
 * If two webs only touch at a point where one explicitly ends with '-' and the
 * other explicitly starts with '+', that point does not create interference.
 *
 * @param webs Vector of webs.
 * @return Interference graph.
 * @complexity O(W^2 * L * R), where W is the number of webs, L is the average
 *             number of live points per web and R is the average number of
 *             ranges per web.
 */
Graph buildInterferenceGraph(const std::vector<Web>& webs);