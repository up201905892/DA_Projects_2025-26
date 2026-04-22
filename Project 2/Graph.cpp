/**
 * @file Graph.cpp
 * @brief Implementation of Web, Graph, and construction helpers.
 */

#include "Graph.h"
#include <algorithm>
#include <sstream>
#include <stdexcept>
#include <map>

// ---------------------------------------------------------------------------
// Web
// ---------------------------------------------------------------------------

void Web::addRange(const LiveRange& lr) {
    ranges.push_back(lr);
    lines.insert(lr.start);
    lines.insert(lr.end);
    for (int l : lr.intermediate) lines.insert(l);
}

std::string Web::toString() const {
    // Sort ranges by start line for deterministic output
    std::vector<LiveRange> sorted = ranges;
    std::sort(sorted.begin(), sorted.end(),
              [](const LiveRange& a, const LiveRange& b) {
                  return a.start < b.start;
              });

    std::string out;
    for (size_t i = 0; i < sorted.size(); ++i) {
        if (i > 0) out += ',';
        const LiveRange& lr = sorted[i];

        // Reconstruct original token order: start+, intermediates, end-
        // Collect all lines in order
        std::vector<int> allLines;
        allLines.push_back(lr.start);
        for (int l : lr.intermediate) allLines.push_back(l);
        allLines.push_back(lr.end);
        std::sort(allLines.begin(), allLines.end());
        // Remove duplicate if start == end (single-line range)
        allLines.erase(std::unique(allLines.begin(), allLines.end()), allLines.end());

        for (size_t j = 0; j < allLines.size(); ++j) {
            if (j > 0) out += ',';
            out += std::to_string(allLines[j]);
            if (allLines[j] == lr.start)  out += '+';
            if (allLines[j] == lr.end)    out += '-';
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// buildWebs
// ---------------------------------------------------------------------------

/**
 * @brief Check if two live ranges share at least one program line.
 * @complexity O(min(|A|, |B|) * log max(|A|,|B|)).
 */
static bool rangesOverlap(const LiveRange& a, const LiveRange& b) {
    // Build line sets and check intersection
    std::set<int> setA, setB;
    setA.insert(a.start); setA.insert(a.end);
    for (int l : a.intermediate) setA.insert(l);
    setB.insert(b.start); setB.insert(b.end);
    for (int l : b.intermediate) setB.insert(l);

    for (int l : setA)
        if (setB.count(l)) return true;
    return false;
}

std::vector<Web> buildWebs(const std::vector<RangeEntry>& entries) {
    // Group ranges by variable name
    std::map<std::string, std::vector<LiveRange>> byVar;
    for (const auto& e : entries)
        byVar[e.varName].push_back(e.range);

    std::vector<Web> result;
    int nextId = 0;

    for (auto& [varName, ranges] : byVar) {
        // Start with each range as its own "proto-web"
        // Repeatedly merge any two that overlap until fixpoint
        std::vector<std::vector<LiveRange>> groups;
        for (const auto& r : ranges) {
            groups.push_back({r});
        }

        bool changed = true;
        while (changed) {
            changed = false;
            for (size_t i = 0; i < groups.size() && !changed; ++i) {
                for (size_t j = i + 1; j < groups.size() && !changed; ++j) {
                    // Check if any range in group i overlaps any in group j
                    bool overlap = false;
                    for (const auto& ra : groups[i]) {
                        for (const auto& rb : groups[j]) {
                            if (rangesOverlap(ra, rb)) { overlap = true; break; }
                        }
                        if (overlap) break;
                    }
                    if (overlap) {
                        // Merge group j into group i
                        for (const auto& r : groups[j])
                            groups[i].push_back(r);
                        groups.erase(groups.begin() + (int)j);
                        changed = true;
                    }
                }
            }
        }

        // Each remaining group is a web
        for (const auto& g : groups) {
            Web w;
            w.id      = nextId++;
            w.varName = varName;
            for (const auto& r : g) w.addRange(r);
            result.push_back(w);
        }
    }

    return result;
}

// ---------------------------------------------------------------------------
// Graph
// ---------------------------------------------------------------------------

Graph::Graph(int n) : n_(n), adj_(n, std::vector<bool>(n, false)) {}

void Graph::addEdge(int u, int v) {
    if (u == v) return;
    adj_[u][v] = adj_[v][u] = true;
}

bool Graph::hasEdge(int u, int v) const {
    return adj_[u][v];
}

int Graph::degree(int u, const std::set<int>& active) const {
    int deg = 0;
    for (int v : active)
        if (v != u && adj_[u][v]) ++deg;
    return deg;
}

std::vector<int> Graph::neighbors(int u) const {
    std::vector<int> nb;
    for (int v = 0; v < n_; ++v)
        if (v != u && adj_[u][v]) nb.push_back(v);
    return nb;
}

// ---------------------------------------------------------------------------
// buildInterferenceGraph
// ---------------------------------------------------------------------------

Graph buildInterferenceGraph(const std::vector<Web>& webs) {
    int n = (int)webs.size();
    Graph g(n);

    for (int i = 0; i < n; ++i) {
        for (int j = i + 1; j < n; ++j) {
            // Interfere if line sets share any element
            const std::set<int>& li = webs[i].lines;
            const std::set<int>& lj = webs[j].lines;
            // Walk the smaller set and probe the larger
            const std::set<int>& smaller = (li.size() <= lj.size()) ? li : lj;
            const std::set<int>& larger  = (li.size() <= lj.size()) ? lj : li;
            for (int l : smaller) {
                if (larger.count(l)) {
                    g.addEdge(i, j);
                    break;
                }
            }
        }
    }
    return g;
}
