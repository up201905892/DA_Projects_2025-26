/**
 * @file Graph.cpp
 * @brief Implementation of Web, Graph, and construction helpers.
 */

#include "Graph.h"
#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <map>
#include <set>

// ---------------------------------------------------------------------------
// Web
// ---------------------------------------------------------------------------

/**
 * @brief Add a live range to a web.
 *
 * The range is stored in the web and all its program points are inserted into
 * the union line set used later for interference detection.
 *
 * @param lr Live range to add.
 * @complexity O(R log L), where R is the number of points in the range and L is
 *             the number of points already stored in the web.
 */
void Web::addRange(const LiveRange& lr) {
    ranges.push_back(lr);

    lines.insert(lr.start);
    lines.insert(lr.end);

    for (int l : lr.intermediate)
        lines.insert(l);
}

/**
 * @brief Convert a web to the output format required by the statement.
 *
 * Marker information is preserved:
 *  - '+' is printed only when the parsed range had an explicit start marker;
 *  - '-' is printed only when the parsed range had an explicit end marker.
 *
 * @return String representation of the web.
 * @complexity O(L log L), where L is the number of points in the web.
 */
std::string Web::toString() const {
    // Sort ranges by their first program point for deterministic output.
    std::vector<LiveRange> sorted = ranges;
    std::sort(sorted.begin(), sorted.end(),
              [](const LiveRange& a, const LiveRange& b) {
                  if (a.start != b.start) return a.start < b.start;
                  return a.end < b.end;
              });

    std::string out;

    for (size_t i = 0; i < sorted.size(); ++i) {
        if (i > 0) out += ',';

        const LiveRange& lr = sorted[i];

        // Reconstruct the program points of this range.
        std::vector<int> allLines;
        allLines.push_back(lr.start);

        for (int l : lr.intermediate)
            allLines.push_back(l);

        allLines.push_back(lr.end);

        std::sort(allLines.begin(), allLines.end());
        allLines.erase(std::unique(allLines.begin(), allLines.end()),
                       allLines.end());

        for (size_t j = 0; j < allLines.size(); ++j) {
            if (j > 0) out += ',';

            int line = allLines[j];
            out += std::to_string(line);

            if (lr.hasStartMarker && line == lr.start)
                out += '+';

            if (lr.hasEndMarker && line == lr.end)
                out += '-';
        }
    }

    return out;
}

// ---------------------------------------------------------------------------
// Live range helpers
// ---------------------------------------------------------------------------

/**
 * @brief Return all program points belonging to a live range.
 *
 * @param lr Live range.
 * @return Set of program points.
 * @complexity O(L log L), where L is the number of points in the range.
 */
static std::set<int> rangeLines(const LiveRange& lr) {
    std::set<int> result;

    result.insert(lr.start);
    result.insert(lr.end);

    for (int l : lr.intermediate)
        result.insert(l);

    return result;
}

/**
 * @brief Check if two live ranges share at least one program point.
 *
 * This is used for web construction. Ranges of the same variable are merged
 * whenever they overlap at any point.
 *
 * @param a First live range.
 * @param b Second live range.
 * @return true if the ranges share at least one program point.
 * @complexity O(L log L), where L is the average number of points per range.
 */
static bool rangesOverlap(const LiveRange& a, const LiveRange& b) {
    std::set<int> setA = rangeLines(a);
    std::set<int> setB = rangeLines(b);

    const std::set<int>& smaller = (setA.size() <= setB.size()) ? setA : setB;
    const std::set<int>& larger = (setA.size() <= setB.size()) ? setB : setA;

    for (int l : smaller) {
        if (larger.count(l))
            return true;
    }

    return false;
}

/**
 * @brief Check whether a web has an explicit '+' marker at a line.
 *
 * @param w Web to inspect.
 * @param line Program point.
 * @return true if some range in the web explicitly starts at this line.
 * @complexity O(R), where R is the number of ranges in the web.
 */
static bool webHasExplicitStartAt(const Web& w, int line) {
    for (const LiveRange& lr : w.ranges) {
        if (lr.hasStartMarker && lr.start == line)
            return true;
    }

    return false;
}

/**
 * @brief Check whether a web has an explicit '-' marker at a line.
 *
 * @param w Web to inspect.
 * @param line Program point.
 * @return true if some range in the web explicitly ends at this line.
 * @complexity O(R), where R is the number of ranges in the web.
 */
static bool webHasExplicitEndAt(const Web& w, int line) {
    for (const LiveRange& lr : w.ranges) {
        if (lr.hasEndMarker && lr.end == line)
            return true;
    }

    return false;
}

/**
 * @brief Check if a common point is only a non-interfering boundary.
 *
 * According to the statement, two webs do not interfere if the only reason they
 * share a program point is that one value is last used at that instruction and
 * the other value is defined at that same instruction.
 *
 * @param a First web.
 * @param b Second web.
 * @param line Shared program point.
 * @return true if this shared point should not create an interference edge.
 * @complexity O(R), where R is the number of ranges per web.
 */
static bool isNonInterferingBoundary(const Web& a, const Web& b, int line) {
    bool aStarts = webHasExplicitStartAt(a, line);
    bool aEnds = webHasExplicitEndAt(a, line);

    bool bStarts = webHasExplicitStartAt(b, line);
    bool bEnds = webHasExplicitEndAt(b, line);

    bool aEndsAndBStarts = aEnds && !aStarts && bStarts && !bEnds;
    bool bEndsAndAStarts = bEnds && !bStarts && aStarts && !aEnds;

    return aEndsAndBStarts || bEndsAndAStarts;
}

/**
 * @brief Decide whether two webs interfere.
 *
 * The webs interfere if they share at least one program point that is not merely
 * a non-interfering boundary between a last use '-' and a new definition '+'.
 *
 * @param a First web.
 * @param b Second web.
 * @return true if the webs interfere.
 * @complexity O(L * R), where L is the number of shared candidate points and R
 *             is the number of ranges per web.
 */
static bool websInterfere(const Web& a, const Web& b) {
    const std::set<int>& smaller =
        (a.lines.size() <= b.lines.size()) ? a.lines : b.lines;

    const std::set<int>& larger =
        (a.lines.size() <= b.lines.size()) ? b.lines : a.lines;

    for (int line : smaller) {
        if (!larger.count(line))
            continue;

        if (!isNonInterferingBoundary(a, b, line))
            return true;
    }

    return false;
}

// ---------------------------------------------------------------------------
// buildWebs
// ---------------------------------------------------------------------------

/**
 * @brief Build live webs by merging overlapping ranges of the same variable.
 *
 * Ranges are first grouped by variable name. Inside each variable group, any two
 * range groups that share a program point are merged. This process repeats until
 * no more groups can be merged.
 *
 * @param entries Parsed range entries.
 * @return Vector of constructed webs.
 * @complexity O(E^2 * L), where E is the number of range entries and L is the
 *             average number of points per range.
 */
std::vector<Web> buildWebs(const std::vector<RangeEntry>& entries) {
    // Group ranges by variable name.
    std::map<std::string, std::vector<LiveRange>> byVar;

    for (const auto& e : entries)
        byVar[e.varName].push_back(e.range);

    std::vector<Web> result;
    int nextId = 0;

    for (auto& [varName, ranges] : byVar) {
        // Start with each range as an independent proto-web.
        std::vector<std::vector<LiveRange>> groups;

        for (const auto& r : ranges)
            groups.push_back({r});

        // Repeatedly merge groups that overlap.
        bool changed = true;

        while (changed) {
            changed = false;

            for (size_t i = 0; i < groups.size() && !changed; ++i) {
                for (size_t j = i + 1; j < groups.size() && !changed; ++j) {
                    bool overlap = false;

                    for (const auto& ra : groups[i]) {
                        for (const auto& rb : groups[j]) {
                            if (rangesOverlap(ra, rb)) {
                                overlap = true;
                                break;
                            }
                        }

                        if (overlap)
                            break;
                    }

                    if (overlap) {
                        for (const auto& r : groups[j])
                            groups[i].push_back(r);

                        groups.erase(groups.begin() + static_cast<int>(j));
                        changed = true;
                    }
                }
            }
        }

        // Each remaining group is one web.
        for (const auto& group : groups) {
            Web w;
            w.id = nextId++;
            w.varName = varName;

            for (const auto& r : group)
                w.addRange(r);

            result.push_back(w);
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
// Graph
// ---------------------------------------------------------------------------

/**
 * @brief Construct an empty undirected graph with n nodes.
 *
 * @param n Number of nodes.
 * @complexity O(n^2).
 */
Graph::Graph(int n) : n_(n), adj_(n, std::vector<bool>(n, false)) {}

/**
 * @brief Add an undirected edge between two nodes.
 *
 * @param u First node.
 * @param v Second node.
 * @complexity O(1).
 */
void Graph::addEdge(int u, int v) {
    if (u == v)
        return;

    adj_[u][v] = true;
    adj_[v][u] = true;
}

/**
 * @brief Check if an edge exists between two nodes.
 *
 * @param u First node.
 * @param v Second node.
 * @return true if the edge exists, false otherwise.
 * @complexity O(1).
 */
bool Graph::hasEdge(int u, int v) const {
    return adj_[u][v];
}

/**
 * @brief Compute the degree of a node inside an active subgraph.
 *
 * @param u Node whose degree is computed.
 * @param active Set of active nodes.
 * @return Number of active neighbors.
 * @complexity O(A), where A is the number of active nodes.
 */
int Graph::degree(int u, const std::set<int>& active) const {
    int deg = 0;

    for (int v : active) {
        if (v != u && adj_[u][v])
            ++deg;
    }

    return deg;
}

/**
 * @brief Get all neighbors of a node.
 *
 * @param u Node whose neighbors are requested.
 * @return Vector of neighbor node indices.
 * @complexity O(n), where n is the number of graph nodes.
 */
std::vector<int> Graph::neighbors(int u) const {
    std::vector<int> nb;

    for (int v = 0; v < n_; ++v) {
        if (v != u && adj_[u][v])
            nb.push_back(v);
    }

    return nb;
}

// ---------------------------------------------------------------------------
// buildInterferenceGraph
// ---------------------------------------------------------------------------

/**
 * @brief Build an interference graph from the computed webs.
 *
 * An edge is added when two webs are alive at the same program point, except
 * when the shared point is only a boundary where one web ends with '-' and the
 * other starts with '+'.
 *
 * @param webs Vector of webs.
 * @return Interference graph.
 * @complexity O(W^2 * L * R), where W is the number of webs, L is the average
 *             number of points per web and R is the average number of ranges
 *             per web.
 */
Graph buildInterferenceGraph(const std::vector<Web>& webs) {
    int n = static_cast<int>(webs.size());
    Graph g(n);

    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            if (websInterfere(webs[i], webs[j]))
                g.addEdge(i, j);
        }
    }

    return g;
}