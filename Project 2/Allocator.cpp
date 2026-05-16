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
// Output metadata helper
// ---------------------------------------------------------------------------

/**
 * @brief Write processing-friendly metadata for alternative algorithms.
 *
 * This metadata is intentionally line-based to make it easy to parse
 * automatically and easy to justify during the project demo.
 *
 * @param f Output stream.
 * @param result Allocation result containing metadata.
 *
 * @complexity O(S + P), where S is the number of spilled webs and P is the
 *             number of split records.
 */
static void writeAlgorithmMetadata(std::ofstream& f, const AllocationResult& result) {
    if (result.algorithm == AlgorithmType::SPILLING) {
        f << "# algorithm metadata\n";
        f << "algorithm: spilling\n";
        f << "spill_budget: " << result.K << '\n';
        f << "spilled_webs: " << result.spilledWebs.size() << '\n';

        for (int webId : result.spilledWebs)
            f << "spilled: web" << webId << '\n';
    }

    if (result.algorithm == AlgorithmType::SPLITTING) {
        f << "# algorithm metadata\n";
        f << "algorithm: splitting\n";
        f << "split_budget: " << result.K << '\n';
        f << "splits: " << result.splitRecords.size() << '\n';

        int splitIndex = 1;

        for (const SplitRecord& split : result.splitRecords) {
            f << "split: " << splitIndex
              << " original=web" << split.originalWebId
              << " created=web" << split.firstNewWebId
              << ",web" << split.secondNewWebId << '\n';

            ++splitIndex;
        }
    }

    if (result.algorithm == AlgorithmType::FREE) {
        f << "# algorithm metadata\n";
        f << "algorithm: free\n";
        f << "spilled_webs: " << result.spilledWebs.size() << '\n';

        for (int webId : result.spilledWebs)
            f << "spilled: web" << webId << '\n';
    }
}

// ---------------------------------------------------------------------------
// simplifyAndColor  (Chaitin-Briggs core)
// ---------------------------------------------------------------------------

/**
 * @brief Simplify and color the interference graph using a Chaitin-Briggs style approach.
 *
 * The algorithm has two phases:
 *  - Simplification: nodes with degree lower than the number of registers are
 *    pushed onto a stack and temporarily removed from the active graph.
 *  - Coloring: nodes are popped from the stack and assigned the lowest available
 *    register not used by their already-colored neighbors.
 *
 * If no node with degree lower than N exists, the algorithm either spills one
 * selected web, if spill budget is available, or marks the remaining nodes as
 * spilled.
 *
 * @param graph Interference graph.
 * @param N Number of available registers.
 * @param webs Vector of webs, used by the spill heuristic.
 * @param spillsLeft Maximum number of allowed spills.
 * @return AllocationResult with color assignment and allocation metadata.
 *
 * @complexity O(W^2), where W is the number of webs.
 */
AllocationResult Allocator::simplifyAndColor(const Graph& graph, int N, const std::vector<Web>& webs, int spillsLeft) {
    int W = graph.size();

    AllocationResult res;
    res.success = false;
    res.registersUsed = 0;
    res.colors.assign(W, SPILL_COLOR);

    std::set<int> active;
    for (int i = 0; i < W; ++i) active.insert(i);

    std::stack<int> S;
    std::set<int> spilled;

    // -----------------------------------------------------------------------
    // Phase 1: Simplification
    // -----------------------------------------------------------------------
    while (!active.empty()) {
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
            if (spillsLeft <= 0) {
                for (int u : active)
                    spilled.insert(u);

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
        int u = S.top();
        S.pop();

        std::set<int> usedColors;

        for (int nb : graph.neighbors(u)) {
            if (res.colors[nb] != SPILL_COLOR)
                usedColors.insert(res.colors[nb]);
        }

        int chosen = SPILL_COLOR;

        for (int c = 0; c < N; ++c) {
            if (!usedColors.count(c)) {
                chosen = c;
                break;
            }
        }

        res.colors[u] = chosen;

        if (chosen == SPILL_COLOR)
            spilled.insert(u);
    }

    for (int u : spilled)
        res.colors[u] = SPILL_COLOR;

    bool success = spilled.empty();
    int maxColor = -1;

    for (int c : res.colors) {
        if (c != SPILL_COLOR && c > maxColor)
            maxColor = c;
    }

    res.success = success;
    res.registersUsed = (maxColor >= 0) ? maxColor + 1 : 0;

    return res;
}

// ---------------------------------------------------------------------------
// pickSpill
// ---------------------------------------------------------------------------

/**
 * @brief Select the best web to spill.
 *
 * The heuristic maximizes degree * live_range_length. This tends to remove webs
 * that are both highly connected and long-lived, reducing register pressure.
 *
 * @param active Set of currently active graph nodes.
 * @param graph Interference graph.
 * @param webs Vector of webs.
 * @return Index of the selected web.
 *
 * @complexity O(W), where W is the number of active webs.
 */
int Allocator::pickSpill(const std::set<int>& active, const Graph& graph, const std::vector<Web>& webs) const {
    int best = *active.begin();
    long bestScore = -1;

    for (int u : active) {
        int deg = graph.degree(u, active);
        long rangeLen = static_cast<long>(webs[u].lines.size());
        long score = static_cast<long>(deg) * rangeLen;

        if (score > bestScore) {
            bestScore = score;
            best = u;
        }
    }

    return best;
}

// ---------------------------------------------------------------------------
// pickSplitTarget
// ---------------------------------------------------------------------------

/**
 * @brief Select the web that should be split.
 *
 * The selected web is the one with the highest degree * live_range_length score.
 * Webs with fewer than two live program points are ignored.
 *
 * @param webs Vector of webs.
 * @param graph Interference graph.
 * @return Index of the selected web.
 *
 * @complexity O(W), where W is the number of webs.
 */
int Allocator::pickSplitTarget(const std::vector<Web>& webs, const Graph& graph) const {
    int W = graph.size();

    std::set<int> all;
    for (int i = 0; i < W; ++i)
        all.insert(i);

    int best = 0;
    long bestScore = -1;

    for (int u = 0; u < W; ++u) {
        int deg = graph.degree(u, all);
        long rangeLen = static_cast<long>(webs[u].lines.size());
        long score = static_cast<long>(deg) * rangeLen;

        if (score > bestScore && rangeLen >= 2) {
            bestScore = score;
            best = u;
        }
    }

    return best;
}

// ---------------------------------------------------------------------------
// splitWebInPlace
// ---------------------------------------------------------------------------

/**
 * @brief Split a web into two smaller webs.
 *
 * The original web is removed from the vector and replaced by two new webs whose
 * live program points are divided around the median. The returned SplitRecord
 * identifies the original web and the two derived webs.
 *
 * @param webs Vector of webs to mutate.
 * @param graph Interference graph. Currently unused here because the caller
 *              rebuilds it after the split.
 * @param target Iterator pointing to the web to split.
 * @return Metadata describing the split.
 *
 * @complexity O(L log L), where L is the number of live program points in the web.
 */
SplitRecord Allocator::splitWebInPlace(std::vector<Web>& webs, Graph& /*graph*/, const std::vector<Web>::iterator& target) {
    Web original = *target;

    SplitRecord record;
    record.originalWebId = original.id;

    webs.erase(target);

    std::vector<int> sortedLines(original.lines.begin(), original.lines.end());
    std::sort(sortedLines.begin(), sortedLines.end());

    if (sortedLines.size() < 2) {
        webs.push_back(original);
        return record;
    }

    size_t mid = sortedLines.size() / 2;

    auto makeRange = [&](std::vector<int>::iterator beg, std::vector<int>::iterator en) -> LiveRange {
        LiveRange lr;
        lr.start = *beg;
        lr.end = *(en - 1);

        if (en - beg > 2) {
            for (auto it = beg + 1; it != en - 1; ++it)
                lr.intermediate.push_back(*it);
        }

        return lr;
    };

    LiveRange r1 = makeRange(sortedLines.begin(), sortedLines.begin() + static_cast<int>(mid));
    LiveRange r2 = makeRange(sortedLines.begin() + static_cast<int>(mid), sortedLines.end());

    int nextId = static_cast<int>(webs.size());

    Web w1;
    w1.id = nextId++;
    w1.varName = original.varName;
    w1.addRange(r1);

    Web w2;
    w2.id = nextId;
    w2.varName = original.varName;
    w2.addRange(r2);

    record.firstNewWebId = w1.id;
    record.secondNewWebId = w2.id;

    webs.push_back(w1);
    webs.push_back(w2);

    return record;
}

// ---------------------------------------------------------------------------
// Algorithm variants
// ---------------------------------------------------------------------------

/**
 * @brief Run the BASIC allocation algorithm.
 *
 * BASIC does not allow spilling. If the graph cannot be fully colored with the
 * available registers, the allocation fails completely and all webs are assigned
 * to memory in the output.
 *
 * @param webs Vector of webs.
 * @param graph Interference graph.
 * @param N Number of available registers.
 * @return Allocation result.
 *
 * @complexity O(W^2), where W is the number of webs.
 */
AllocationResult Allocator::runBasic(std::vector<Web>& webs, const Graph& graph, int N) {
    AllocationResult res = simplifyAndColor(graph, N, webs, 0);

    res.algorithm = AlgorithmType::BASIC;
    res.K = 0;

    if (!res.success) {
        std::cerr << "[ERROR] Basic allocation failed: "
                  << std::count(res.colors.begin(), res.colors.end(), SPILL_COLOR)
                  << " web(s) could not be colored with " << N
                  << " register(s).\n";

        res.registersUsed = 0;
        std::fill(res.colors.begin(), res.colors.end(), SPILL_COLOR);
    }

    return res;
}

/**
 * @brief Run allocation with web spilling.
 *
 * The algorithm allows up to K webs to be assigned to memory. If more than K
 * webs would need to be spilled, the allocation is treated as a complete failure.
 *
 * @param webs Vector of webs.
 * @param graph Interference graph.
 * @param N Number of available registers.
 * @param K Maximum number of allowed spills.
 * @return Allocation result.
 *
 * @complexity O(W^2), where W is the number of webs.
 */
AllocationResult Allocator::runSpilling(std::vector<Web>& webs, const Graph& graph, int N, int K) {
    AllocationResult res = simplifyAndColor(graph, N, webs, K);

    res.algorithm = AlgorithmType::SPILLING;
    res.K = K;

    int spillCount = static_cast<int>(
        std::count(res.colors.begin(), res.colors.end(), SPILL_COLOR)
    );

    res.spilledWebs.clear();

    for (int i = 0; i < static_cast<int>(res.colors.size()); ++i) {
        if (res.colors[i] == SPILL_COLOR)
            res.spilledWebs.push_back(i);
    }

    if (spillCount > K) {
        std::cerr << "[ERROR] Spilling allocation failed: needed " << spillCount
                  << " spill(s) but K=" << K << ".\n";

        res.success = false;
        res.registersUsed = 0;
        std::fill(res.colors.begin(), res.colors.end(), SPILL_COLOR);

        res.spilledWebs.clear();
        for (int i = 0; i < static_cast<int>(res.colors.size()); ++i) res.spilledWebs.push_back(i);

        return res;
    }

    if (spillCount > 0) {
        std::cerr << "[INFO] Spilling: " << spillCount
                  << " web(s) assigned to memory.\n";
        res.success = true;
    }

    int maxColor = -1;

    for (int c : res.colors) {
        if (c != SPILL_COLOR && c > maxColor)
            maxColor = c;
    }

    res.registersUsed = (maxColor >= 0) ? maxColor + 1 : 0;

    return res;
}

/**
 * @brief Run allocation with web splitting.
 *
 * The algorithm first tries regular coloring. If it fails, it splits one web and
 * rebuilds the interference graph. This process is repeated up to K times.
 *
 * @param webs Vector of webs. May be mutated if splitting occurs.
 * @param graph Interference graph. May be rebuilt after splits.
 * @param N Number of available registers.
 * @param K Maximum number of allowed splits.
 * @return Allocation result.
 *
 * @complexity O(K * W^2 * L), where W is the number of webs and L is the average
 *             number of live points per web.
 */
AllocationResult Allocator::runSplitting(std::vector<Web>& webs, Graph& graph, int N, int K) {
    std::vector<SplitRecord> performedSplits;

    for (int iter = 0; iter <= K; ++iter) {
        AllocationResult res = simplifyAndColor(graph, N, webs, 0);

        res.algorithm = AlgorithmType::SPLITTING;
        res.K = K;
        res.splitRecords = performedSplits;

        if (res.success)
            return res;

        if (iter == K) {
            std::cerr << "[ERROR] Splitting allocation still failed after "
                      << K << " split(s).\n";

            res.registersUsed = 0;
            std::fill(res.colors.begin(), res.colors.end(), SPILL_COLOR);
            return res;
        }

        int targetIdx = pickSplitTarget(webs, graph);
        SplitRecord split = splitWebInPlace(webs, graph, webs.begin() + targetIdx);
        performedSplits.push_back(split);

        for (int i = 0; i < static_cast<int>(webs.size()); ++i)
            webs[i].id = i;

        graph = buildInterferenceGraph(webs);
    }

    AllocationResult dummy;
    dummy.success = false;
    dummy.registersUsed = 0;
    dummy.colors.assign(webs.size(), SPILL_COLOR);
    dummy.algorithm = AlgorithmType::SPLITTING;
    dummy.K = K;
    dummy.splitRecords = performedSplits;

    return dummy;
}

// ---------------------------------------------------------------------------
// FREE: DSATUR algorithm
// ---------------------------------------------------------------------------

/**
 * @brief Run the FREE allocation algorithm using a DSATUR-based heuristic.
 *
 * DSATUR repeatedly selects the uncolored node with the highest saturation,
 * meaning the highest number of distinct colors already used by its neighbors.
 * Ties are broken by graph degree.
 *
 * @param webs Vector of webs. Currently unused by this implementation.
 * @param graph Interference graph.
 * @param N Number of available registers.
 * @return Allocation result.
 *
 * @complexity O(W^2), where W is the number of webs.
 */
AllocationResult Allocator::runFree(std::vector<Web>& /*webs*/, const Graph& graph, int N) {
    int W = graph.size();

    AllocationResult res;
    res.success = false;
    res.registersUsed = 0;
    res.colors.assign(W, SPILL_COLOR);
    res.algorithm = AlgorithmType::FREE;
    res.K = 0;

    std::vector<std::set<int>> neighborColors(W);
    std::vector<bool> colored(W, false);

    std::set<int> uncolored;
    for (int i = 0; i < W; ++i)
        uncolored.insert(i);

    while (!uncolored.empty()) {
        int best = -1;
        int bestSat = -1;
        int bestDeg = -1;

        std::set<int> all;
        for (int i = 0; i < W; ++i)
            all.insert(i);

        for (int u : uncolored) {
            int sat = static_cast<int>(neighborColors[u].size());
            int deg = graph.degree(u, all);

            if (sat > bestSat || (sat == bestSat && deg > bestDeg)) {
                bestSat = sat;
                bestDeg = deg;
                best = u;
            }
        }

        int chosen = SPILL_COLOR;

        for (int c = 0; c < N; ++c) {
            if (!neighborColors[best].count(c)) {
                chosen = c;
                break;
            }
        }

        res.colors[best] = chosen;
        colored[best] = true;
        uncolored.erase(best);

        if (chosen == SPILL_COLOR)
            continue;

        for (int nb : graph.neighbors(best))
            neighborColors[nb].insert(chosen);
    }

    int maxColor = -1;
    int spillCount = 0;

    for (int c : res.colors) {
        if (c == SPILL_COLOR)
            ++spillCount;
        else if (c > maxColor)
            maxColor = c;
    }

    res.success = (spillCount == 0);
    res.registersUsed = (maxColor >= 0) ? maxColor + 1 : 0;

    res.spilledWebs.clear();
    for (int i = 0; i < static_cast<int>(res.colors.size()); ++i) {
        if (res.colors[i] == SPILL_COLOR)
            res.spilledWebs.push_back(i);
    }

    return res;
}

// ---------------------------------------------------------------------------
// allocate  (dispatcher)
// ---------------------------------------------------------------------------

/**
 * @brief Dispatch allocation to the algorithm selected in the configuration.
 *
 * @param webs Vector of webs.
 * @param graph Interference graph.
 * @param config Allocation configuration.
 * @return Allocation result.
 *
 * @complexity Depends on the selected allocation algorithm.
 */
AllocationResult Allocator::allocate(std::vector<Web>& webs, Graph& graph, const AllocConfig& config) {
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

    AllocationResult dummy;
    dummy.success = false;
    dummy.registersUsed = 0;
    dummy.colors.assign(webs.size(), SPILL_COLOR);
    return dummy;
}

// ---------------------------------------------------------------------------
// writeOutput
// ---------------------------------------------------------------------------

/**
 * @brief Write the allocation result to an output file.
 *
 * The output format follows the project statement. For spilling and splitting,
 * extra metadata is appended in a simple line-based format.
 *
 * @param filename Output file path.
 * @param webs Vector of webs.
 * @param result Allocation result.
 * @param numRegisters Number of registers available in the input configuration.
 *                     This parameter is kept for compatibility, but the output
 *                     prints result.registersUsed.
 *
 * @complexity O(W + A + M), where W is the number of webs, A is the number of
 *             register assignments, and M is the amount of metadata written.
 */
void writeOutput(const std::string& filename,
                 const std::vector<Web>& webs,
                 const AllocationResult& result,
                 int numRegisters) {
    (void)numRegisters;

    std::ofstream f(filename);

    if (!f.is_open())
        throw std::runtime_error("Cannot open output file: " + filename);

    int W = static_cast<int>(webs.size());

    f << "webs: " << W << '\n';

    for (int i = 0; i < W; ++i)
        f << "web" << i << ": " << webs[i].toString() << '\n';

    if (!result.success &&
        (result.registersUsed == 0 ||
         std::all_of(result.colors.begin(), result.colors.end(),
                     [](int c) { return c == SPILL_COLOR; }))) {
        f << "registers: 0\n";

        for (int i = 0; i < W; ++i)
            f << "M: web" << i << '\n';

        writeAlgorithmMetadata(f, result);
        return;
    }

    f << "registers: " << result.registersUsed << '\n';

    std::map<int, std::vector<int>> byColor;

    for (int i = 0; i < W; ++i)
        byColor[result.colors[i]].push_back(i);

    for (int c = 0; c < result.registersUsed; ++c) {
        auto it = byColor.find(c);

        if (it == byColor.end())
            continue;

        for (int wi : it->second)
            f << 'r' << c << ": web" << wi << '\n';
    }

    auto it = byColor.find(SPILL_COLOR);

    if (it != byColor.end()) {
        for (int wi : it->second)
            f << "M: web" << wi << '\n';
    }

    writeAlgorithmMetadata(f, result);
}