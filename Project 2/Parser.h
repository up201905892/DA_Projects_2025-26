/**
 * @file Parser.h
 * @brief Parses range and register/algorithm input files.
 *
 * Handles two input formats:
 *  - Ranges file: "varname: start+,mid,...,end-"
 *  - Config  file: "registers: N" / "algorithm: type[, K]"
 */

#pragma once
#include <string>
#include <vector>
#include <stdexcept>

// ---------------------------------------------------------------------------
// Data types
// ---------------------------------------------------------------------------

/**
 * @brief One contiguous live range for a variable.
 *
 * Lines are stored in ascending order. start is the definition point (+),
 * end is the last-use point (-).
 */
struct LiveRange {
    int              start;        ///< definition line (marked with + in input)
    int              end;          ///< last-use   line (marked with - in input)
    std::vector<int> intermediate; ///< lines strictly between start and end
};

/**
 * @brief A single parsed entry from the ranges file.
 */
struct RangeEntry {
    std::string varName; ///< variable name
    LiveRange   range;
};

/**
 * @brief Algorithm type requested by the config file.
 */
enum class AlgorithmType {
    BASIC,    ///< plain graph-coloring, no recovery
    SPILLING, ///< spill up to K webs to memory
    SPLITTING,///< split up to K webs
    FREE      ///< user-defined optimized strategy
};

/**
 * @brief Parsed configuration from the registers/algorithm file.
 */
struct AllocConfig {
    int           numRegisters = 0;
    AlgorithmType type         = AlgorithmType::BASIC;
    int           K            = 0; ///< parameter for spilling / splitting
};

// ---------------------------------------------------------------------------
// Parser class
// ---------------------------------------------------------------------------

/**
 * @brief Parses the two input files required by the register allocator.
 *
 * @complexity Parsing ranges: O(L * R) where L = number of lines, R = avg
 *             tokens per line. Parsing config: O(1).
 */
class Parser {
public:
    /**
     * @brief Parse the live-ranges file.
     *
     * Lines beginning with '#' or empty lines are ignored.
     * Format per line: "varname: token1,token2,...tokenN"
     * where exactly one token has a '+' suffix (definition) and one has '-'
     * suffix (last use).
     *
     * @param filename Path to the ranges file.
     * @return Vector of RangeEntry, one per non-comment line.
     * @throws std::runtime_error on malformed input.
     * @complexity O(L * T) where L = lines, T = tokens per line.
     */
    static std::vector<RangeEntry> parseRanges(const std::string& filename);

    /**
     * @brief Parse the registers/algorithm config file.
     *
     * @param filename Path to the config file.
     * @return Populated AllocConfig struct.
     * @throws std::runtime_error on malformed input or missing fields.
     * @complexity O(1) — fixed number of key-value pairs.
     */
    static AllocConfig parseConfig(const std::string& filename);

private:
    /**
     * @brief Split a comma-separated token list into individual tokens.
     * @complexity O(n) where n = string length.
     */
    static std::vector<std::string> splitComma(const std::string& s);

    /**
     * @brief Strip leading/trailing whitespace from a string.
     * @complexity O(n).
     */
    static std::string trim(const std::string& s);
};
