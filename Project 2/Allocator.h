/**
 * @file Allocator.h
 * @brief Register allocation algorithms operating on the interference graph.
 *
 * Supports four allocation strategies:
 *  - BASIC:     Greedy graph coloring; fails hard on spill.
 *  - SPILLING:  Tolerates up to K spills to memory.
 *  - SPLITTING: Splits up to K webs before attempting coloring.
 *  - FREE:      Optimized DSATUR-based coloring with integrated spilling.
 */

#pragma once

#include "Graph.h"
#include "Parser.h"
#include <vector>
#include <set>
#include <string>

// ---------------------------------------------------------------------------
// Result types
// ---------------------------------------------------------------------------

/** @brief Sentinel value meaning "spill to memory". */
static constexpr int SPILL_COLOR = -1;

/**
 * @brief Metadata describing one web split operation.
 *
 * This structure is used only by the SPLITTING algorithm. It records which web
 * was selected for splitting and which two derived webs were created.
 */
struct SplitRecord {
    int originalWebId = -1;  ///< id of the web selected for splitting
    int firstNewWebId = -1;  ///< id of the first derived web
    int secondNewWebId = -1; ///< id of the second derived web
};

/**
 * @brief Result of a register allocation attempt.
 *
 * colors[i] holds the register index (0..N-1) for web i, or SPILL_COLOR (-1)
 * if web i was assigned to memory.
 *
 * spilledWebs and splitRecords are complementary metadata used by the
 * alternative algorithms. They make the output more processing-friendly and
 * easier to justify in the project presentation.
 */
struct AllocationResult {
    bool success = false;                 ///< true if allocation is acceptable
    int registersUsed = 0;                ///< highest register index used + 1
    std::vector<int> colors;              ///< color/register per web

    AlgorithmType algorithm = AlgorithmType::BASIC; ///< algorithm used
    int K = 0;                                      ///< spilling/splitting budget

    std::vector<int> spilledWebs;          ///< ids of webs assigned to memory
    std::vector<SplitRecord> splitRecords; ///< split operations performed
};

// ---------------------------------------------------------------------------
// Allocator class
// ---------------------------------------------------------------------------

/**
 * @brief Orchestrates register allocation.
 *
 * Typical usage:
 * @code
 *   Allocator alloc;
 *   AllocationResult r = alloc.allocate(webs, graph, config);
 * @endcode
 */
class Allocator {
public:
    /**
     * @brief Top-level entry point: dispatches to the selected algorithm.
     *
     * @param webs Webs to allocate. May be mutated for SPLITTING.
     * @param graph Interference graph. May be mutated for SPLITTING.
     * @param config Algorithm configuration.
     * @return Allocation result.
     *
     * @complexity Depends on the selected algorithm.
     */
    AllocationResult allocate(std::vector<Web>& webs, Graph& graph,
                              const AllocConfig& config);

private:
    // ------------------------------------------------------------------
    // Core simplification / coloring
    // ------------------------------------------------------------------

    /**
     * @brief Chaitin-Briggs simplification and coloring.
     *
     * Phase 1: while any node in the active graph has degree < N, push it onto
     * a stack and remove it from the active graph. If no such node exists, spill
     * a selected node if the spill budget allows it.
     *
     * Phase 2: pop nodes from the stack and assign the lowest available color
     * not used by already-colored neighbors.
     *
     * @param graph Original interference graph.
     * @param N Number of available registers.
     * @param webs Web list, used by spill heuristics.
     * @param spillsLeft Maximum number of allowed spills.
     * @return Allocation result.
     *
     * @complexity O(W^2), where W is the number of webs.
     */
    AllocationResult simplifyAndColor(const Graph& graph, int N,
                                      const std::vector<Web>& webs,
                                      int spillsLeft);

    // ------------------------------------------------------------------
    // Spill heuristic
    // ------------------------------------------------------------------

    /**
     * @brief Choose the best web to spill from the active set.
     *
     * Heuristic: maximize degree * live_range_length. This tends to remove webs
     * that are both highly connected and long-lived, reducing register pressure.
     *
     * @param active Set of active nodes.
     * @param graph Interference graph.
     * @param webs Web vector.
     * @return Index of the selected web.
     *
     * @complexity O(W), where W is the number of active webs.
     */
    int pickSpill(const std::set<int>& active, const Graph& graph,
                  const std::vector<Web>& webs) const;

    // ------------------------------------------------------------------
    // Splitting
    // ------------------------------------------------------------------

    /**
     * @brief Split a web at its median live line, producing two derived webs.
     *
     * The original web is removed; two new webs are appended to the web vector.
     * The caller must rebuild the interference graph after this operation.
     *
     * @param webs Web list to mutate.
     * @param graph Interference graph. Currently unused here, rebuilt by caller.
     * @param target Iterator pointing to the web to split.
     * @return Metadata describing the split operation.
     *
     * @complexity O(L log L), where L is the number of live points in the web.
     */
    SplitRecord splitWebInPlace(std::vector<Web>& webs, Graph& graph,
                                const std::vector<Web>::iterator& target);

    /**
     * @brief Choose the web most worth splitting.
     *
     * Heuristic: maximize degree * live_range_length.
     *
     * @param webs Web vector.
     * @param graph Interference graph.
     * @return Index of the selected web.
     *
     * @complexity O(W), where W is the number of webs.
     */
    int pickSplitTarget(const std::vector<Web>& webs, const Graph& graph) const;

    // ------------------------------------------------------------------
    // Algorithm variants
    // ------------------------------------------------------------------

    /** @brief BASIC: fail on first required spill. @complexity O(W^2). */
    AllocationResult runBasic(std::vector<Web>& webs, const Graph& graph,
                              int N);

    /** @brief SPILLING: allow up to K spills. @complexity O(W^2). */
    AllocationResult runSpilling(std::vector<Web>& webs, const Graph& graph,
                                 int N, int K);

    /**
     * @brief SPLITTING: split up to K webs before coloring.
     * @complexity O(K * W^2 * L).
     */
    AllocationResult runSplitting(std::vector<Web>& webs, Graph& graph,
                                  int N, int K);

    /**
     * @brief FREE: DSATUR algorithm with integrated spilling fallback.
     * @complexity O(W^2).
     */
    AllocationResult runFree(std::vector<Web>& webs, const Graph& graph,
                             int N);
};

// ---------------------------------------------------------------------------
// Output helpers
// ---------------------------------------------------------------------------

/**
 * @brief Write allocation result to a file.
 *
 * Format:
 * @code
 * webs: W
 * web0: <ranges>
 * ...
 * registers: R
 * r0: webX
 * ...
 * M: webY
 * @endcode
 *
 * For alternative algorithms, additional processing-friendly metadata may be
 * appended, for example:
 *
 * @code
 * # algorithm metadata
 * algorithm: spilling
 * spill_budget: 2
 * spilled_webs: 1
 * spilled: web3
 * @endcode
 *
 * @param filename Output file path.
 * @param webs Web vector.
 * @param result Allocation result.
 * @param numRegisters Number of registers from configuration. Kept for
 *                     compatibility; result.registersUsed is printed.
 *
 * @complexity O(W + A), where W is the number of webs and A is the number of
 *             assignments written.
 */
void writeOutput(const std::string& filename,
                 const std::vector<Web>& webs,
                 const AllocationResult& result,
                 int numRegisters);