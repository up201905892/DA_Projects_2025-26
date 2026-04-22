/**
 * @file Allocator.cpp
 * @brief Implementation of register allocation algorithms.
 */

#include "Allocator.h"
#include <algorithm>
#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <stack>
#include <stdexcept>
#include <numeric>

// ---------------------------------------------------------------------------
// simplifyAndColor  (Chaitin-Briggs core)
// ---------------------------------------------------------------------------

AllocationResult Allocator::simplifyAndColor(const Graph& graph, int N,
                                             const std::vector<Web>& webs,
                                             int spillsLeft) {
    int W = graph.size();
    AllocationResult res;
    res.colors.assign(W, SPILL_COLOR);

    std::set<int> active;
    for (int i = 0; i < W; ++i) active.insert(i);

    std::stack<int> S;
    std::set<int>   spilled;

    // -----------------------------------------------------------------------
    // Phase 1: Simplification
    // -----------------------------------------------------------------------
    while (!active.empty()) {
        // Find any node with degree < N in the active subgraph
        int candidate = -1;
        for (int u : active) {
            if (graph.degree(u, active) < N) {
                candidate = u;
                break;
            }
        }

        if (candidate != -1) {
            S.push(candidate);
            active.erase(candidate);
        } else {
            // All remaining nodes have degree >= N
            if (spillsLeft <= 0) {
                // Spill everything that remains (hard failure for BASIC)
                for (int u : active) spilled.insert(u);
                active.clear();
            } else {
                int sv = pickSpill(active, graph, webs);
                spilled.insert(sv);
                active.erase(sv);
                --spillsLeft;
            }
        }
    }

    // -----------------------------------------------------------------------
    // Phase 2: Coloring
    // -----------------------------------------------------------------------
    while (!S.empty()) {
        int u = S.top(); S.pop();

        std::set<int> usedColors;
        for (int nb : graph.neighbors(u)) {
            if (res.colors[nb] != SPILL_COLOR)
                usedColors.insert(res.colors[nb]);
        }

        // Assign lowest available color
        int chosen = SPILL_COLOR;
        for (int c = 0; c < N; ++c) {
            if (!usedColors.count(c)) { chosen = c; break; }
        }
        res.colors[u] = chosen;
        if (chosen == SPILL_COLOR) spilled.insert(u); // shouldn't happen
    }

    // Mark spilled nodes
    for (int u : spilled) res.colors[u] = SPILL_COLOR;

    // Compute result metadata
    bool success = spilled.empty();
    int maxColor = -1;
    for (int c : res.colors)
        if (c != SPILL_COLOR && c > maxColor) maxColor = c;

    res.success       = success;
    res.registersUsed = (maxColor >= 0) ? maxColor + 1 : 0;
    return res;
}

// ---------------------------------------------------------------------------
// pickSpill
// ---------------------------------------------------------------------------

int Allocator::pickSpill(const std::set<int>& active, const Graph& graph,
                         const std::vector<Web>& webs) const {
    int best    = *active.begin();
    long bestScore = -1;

    for (int u : active) {
        int deg  = graph.degree(u, active);
        long rangeLen = (long)webs[u].lines.size();
        long score = (long)deg * rangeLen;
        if (score > bestScore) { bestScore = score; best = u; }
    }
    return best;
}

// ---------------------------------------------------------------------------
// pickSplitTarget
// ---------------------------------------------------------------------------

int Allocator::pickSplitTarget(const std::vector<Web>& webs,
                               const Graph& graph) const {
    int allActive = graph.size();
    std::set<int> all;
    for (int i = 0; i < allActive; ++i) all.insert(i);

    int best     = 0;
    long bestScore = -1;
    for (int u = 0; u < allActive; ++u) {
        int  deg   = graph.degree(u, all);
        long rlen  = (long)webs[u].lines.size();
        long score = (long)deg * rlen;
        if (score > bestScore && rlen >= 2) { bestScore = score; best = u; }
    }
    return best;
}

// ---------------------------------------------------------------------------
// splitWebInPlace
// ---------------------------------------------------------------------------

void Allocator::splitWebInPlace(std::vector<Web>& webs, Graph& /*graph*/,
                                const std::vector<Web>::iterator& target) {
    Web original = *target;
    webs.erase(target);

    // Collect all lines in sorted order
    std::vector<int> sortedLines(original.lines.begin(), original.lines.end());
    std::sort(sortedLines.begin(), sortedLines.end());

    if (sortedLines.size() < 2) {
        // Cannot split — put back unchanged
        webs.push_back(original);
        return;
    }

    size_t mid = sortedLines.size() / 2;

    // Build two new live ranges from the halved line sets
    auto makeRange = [&](std::vector<int>::iterator beg,
                         std::vector<int>::iterator en) -> LiveRange {
        LiveRange lr;
        lr.start = *beg;
        lr.end   = *(en - 1);
        // Intermediates only exist when range has 3+ elements
        if (en - beg > 2) {
            for (auto it = beg + 1; it != en - 1; ++it)
                lr.intermediate.push_back(*it);
        }
        return lr;
    };

    LiveRange r1 = makeRange(sortedLines.begin(),
                             sortedLines.begin() + (int)mid);
    LiveRange r2 = makeRange(sortedLines.begin() + (int)mid,
                             sortedLines.end());

    int nextId = (int)webs.size();

    Web w1; w1.id = nextId++;     w1.varName = original.varName; w1.addRange(r1);
    Web w2; w2.id = nextId;       w2.varName = original.varName; w2.addRange(r2);

    webs.push_back(w1);
    webs.push_back(w2);
}

// ---------------------------------------------------------------------------
// Algorithm variants
// ---------------------------------------------------------------------------

AllocationResult Allocator::runBasic(std::vector<Web>& webs, const Graph& graph,
                                     int N) {
    // Zero spills allowed — fail hard if we must spill
    AllocationResult res = simplifyAndColor(graph, N, webs, 0);
    if (!res.success) {
        std::cerr << "[ERROR] Basic allocation failed: "
                  << std::count(res.colors.begin(), res.colors.end(), SPILL_COLOR)
                  << " web(s) could not be colored with " << N
                  << " register(s).\n";
    }
    return res;
}

AllocationResult Allocator::runSpilling(std::vector<Web>& webs,
                                        const Graph& graph, int N, int K) {
    AllocationResult res = simplifyAndColor(graph, N, webs, K);

    int spillCount = (int)std::count(res.colors.begin(), res.colors.end(),
                                     SPILL_COLOR);
    if (spillCount > K) {
        // More spills than the budget allows — true failure
        std::cerr << "[ERROR] Spilling allocation failed: needed " << spillCount
                  << " spill(s) but K=" << K << ".\n";
    } else if (spillCount > 0) {
        std::cerr << "[INFO] Spilling: " << spillCount
                  << " web(s) assigned to memory.\n";
        res.success = true; // within K budget → acceptable
    }
    return res;
}

AllocationResult Allocator::runSplitting(std::vector<Web>& webs, Graph& graph,
                                         int N, int K) {
    // Try coloring; if it fails, split and retry — up to K times
    for (int iter = 0; iter <= K; ++iter) {
        AllocationResult res = simplifyAndColor(graph, N, webs, 0);
        if (res.success) return res;
        if (iter == K) {
            std::cerr << "[ERROR] Splitting allocation still failed after "
                      << K << " split(s).\n";
            return res;
        }

        // Pick target and split
        int targetIdx = pickSplitTarget(webs, graph);
        splitWebInPlace(webs, graph, webs.begin() + targetIdx);

        // Reassign IDs and rebuild interference graph
        for (int i = 0; i < (int)webs.size(); ++i) webs[i].id = i;
        graph = buildInterferenceGraph(webs);
    }
    // Should not reach here
    AllocationResult dummy;
    dummy.success = false;
    dummy.registersUsed = 0;
    dummy.colors.assign(webs.size(), SPILL_COLOR);
    return dummy;
}

// ---------------------------------------------------------------------------
// FREE: DSATUR algorithm
// ---------------------------------------------------------------------------

AllocationResult Allocator::runFree(std::vector<Web>& /*webs*/, const Graph& graph,
                                    int N) {
    int W = graph.size();
    AllocationResult res;
    res.colors.assign(W, SPILL_COLOR);

    // saturation[u] = number of distinct colors among colored neighbors
    std::vector<std::set<int>> neighborColors(W);
    std::vector<bool> colored(W, false);

    // DSATUR: repeatedly pick uncolored node with max saturation,
    // breaking ties by degree in full graph.
    std::set<int> uncolored;
    for (int i = 0; i < W; ++i) uncolored.insert(i);

    while (!uncolored.empty()) {
        // Pick node with highest saturation (ties broken by degree)
        int best = -1, bestSat = -1, bestDeg = -1;
        std::set<int> all;
        for (int i = 0; i < W; ++i) all.insert(i);

        for (int u : uncolored) {
            int sat = (int)neighborColors[u].size();
            int deg = graph.degree(u, all);
            if (sat > bestSat || (sat == bestSat && deg > bestDeg)) {
                bestSat = sat; bestDeg = deg; best = u;
            }
        }

        // Assign lowest color not in neighborColors[best]
        int chosen = SPILL_COLOR;
        for (int c = 0; c < N; ++c) {
            if (!neighborColors[best].count(c)) { chosen = c; break; }
        }

        res.colors[best] = chosen;
        colored[best]    = true;
        uncolored.erase(best);

        if (chosen == SPILL_COLOR) continue; // spilled

        // Update saturation of neighbors
        for (int nb : graph.neighbors(best))
            neighborColors[nb].insert(chosen);
    }

    int maxColor = -1;
    int spillCount = 0;
    for (int c : res.colors) {
        if (c == SPILL_COLOR) ++spillCount;
        else if (c > maxColor) maxColor = c;
    }

    res.success       = (spillCount == 0);
    res.registersUsed = (maxColor >= 0) ? maxColor + 1 : 0;
    return res;
}

// ---------------------------------------------------------------------------
// allocate  (dispatcher)
// ---------------------------------------------------------------------------

AllocationResult Allocator::allocate(std::vector<Web>& webs, Graph& graph,
                                     const AllocConfig& config) {
    switch (config.type) {
        case AlgorithmType::BASIC:
            return runBasic(webs, graph, config.numRegisters);
        case AlgorithmType::SPILLING:
            return runSpilling(webs, graph, config.numRegisters, config.K);
        case AlgorithmType::SPLITTING:
            return runSplitting(webs, graph, config.numRegisters, config.K);
        case AlgorithmType::FREE:
            return runFree(webs, graph, config.numRegisters);
    }
    // Unreachable
    AllocationResult dummy;
    dummy.success = false;
    dummy.registersUsed = 0;
    dummy.colors.assign(webs.size(), SPILL_COLOR);
    return dummy;
}

// ---------------------------------------------------------------------------
// writeOutput
// ---------------------------------------------------------------------------

void writeOutput(const std::string& filename,
                 const std::vector<Web>& webs,
                 const AllocationResult& result,
                 int numRegisters) {
    std::ofstream f(filename);
    if (!f.is_open())
        throw std::runtime_error("Cannot open output file: " + filename);

    int W = (int)webs.size();

    // ---- Header: web list ----
    f << "webs: " << W << '\n';
    for (int i = 0; i < W; ++i)
        f << "web" << i << ": " << webs[i].toString() << '\n';

    // ---- Registers section ----
    if (!result.success && result.registersUsed == 0 &&
        std::all_of(result.colors.begin(), result.colors.end(),
                    [](int c) { return c == SPILL_COLOR; })) {
        // Complete failure
        f << "registers: 0\n";
        for (int i = 0; i < W; ++i)
            f << "M: web" << i << '\n';
        return;
    }

    f << "registers: " << numRegisters << '\n';

    // Group webs by color
    std::map<int, std::vector<int>> byColor;
    for (int i = 0; i < W; ++i)
        byColor[result.colors[i]].push_back(i);

    // Emit register assignments
    for (int c = 0; c < numRegisters; ++c) {
        auto it = byColor.find(c);
        if (it == byColor.end()) continue;
        for (int wi : it->second)
            f << 'r' << c << ": web" << wi << '\n';
    }

    // Emit memory assignments
    auto it = byColor.find(SPILL_COLOR);
    if (it != byColor.end()) {
        for (int wi : it->second)
            f << "M: web" << wi << '\n';
    }
}
