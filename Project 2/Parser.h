/**
 * @file Parser.h
 * @brief Parses range and register/algorithm input files.
 *
 * Handles two input formats:
 *  - Ranges file: "varname: p1[,p2,...,pn]" where any point may optionally
 *    be marked with '+' for definition or '-' for last use.
 *  - Config file: "registers: N" / "algorithm: type[, K]"
 */

#pragma once
#include <string>
#include <vector>
#include <stdexcept>

// ---------------------------------------------------------------------------
// Data types
// ---------------------------------------------------------------------------

/**
 * @brief One live range for a variable.
 *
 * A live range is represented as a set/list of program points. According to the
 * project statement, not every live range line must contain a '+' or '-' marker:
 * ranges that exist only because of intersections may contain plain program
 * points only.
 *
 * The fields start and end store the first and last program points in the input
 * line, respectively. The boolean flags indicate whether those points were
 * explicitly marked as a definition '+' or last use '-'.
 */
struct LiveRange {
    int start = -1;                  ///< first program point in this range
    int end = -1;                    ///< last program point in this range

    bool hasStartMarker = false;     ///< true if a '+' marker exists in this range
    bool hasEndMarker = false;       ///< true if a '-' marker exists in this range

    std::vector<int> intermediate;   ///< remaining program points between start and end
};

/**
 * @brief A single parsed entry from the ranges file.
 */
struct RangeEntry {
    std::string varName; ///< variable name
    LiveRange range;     ///< parsed live range
};

/**
 * @brief Algorithm type requested by the config file.
 */
enum class AlgorithmType {
    BASIC,     ///< plain graph-coloring, no recovery
    SPILLING,  ///< spill up to K webs to memory
    SPLITTING, ///< split up to K webs
    FREE       ///< user-defined optimized strategy
};

/**
 * @brief Parsed configuration from the registers/algorithm file.
 */
struct AllocConfig {
    int numRegisters = 0;                       ///< maximum number of available registers
    AlgorithmType type = AlgorithmType::BASIC;  ///< selected allocation algorithm
    int K = 0;                                  ///< parameter for spilling / splitting
};

// ---------------------------------------------------------------------------
// Parser class
// ---------------------------------------------------------------------------

/**
 * @brief Parses the two input files required by the register allocator.
 *
 * @complexity Parsing ranges: O(L * R), where L is the number of lines and R is
 *             the average number of tokens per line. Parsing config: O(1).
 */
class Parser {
public:
    /**
     * @brief Parse the live-ranges file.
     *
     * Lines beginning with '#' or empty lines are ignored.
     *
     * Format per line:
     * @code
     * varname: token1,token2,...,tokenN
     * @endcode
     *
     * Each token is a program line number, optionally followed by:
     *  - '+' meaning definition/start of a value;
     *  - '-' meaning last use/end of a value.
     *
     * A line may have no markers, one marker, or both markers. Multiple '+'
     * markers or multiple '-' markers in the same range are rejected.
     *
     * @param filename Path to the ranges file.
     * @return Vector of RangeEntry, one per non-comment line.
     * @throws std::runtime_error on malformed input.
     * @complexity O(L * T), where L is the number of lines and T is the average
     *             number of tokens per line.
     */
    static std::vector<RangeEntry> parseRanges(const std::string& filename);

    /**
     * @brief Parse the registers/algorithm config file.
     *
     * @param filename Path to the config file.
     * @return Populated AllocConfig struct.
     * @throws std::runtime_error on malformed input or missing fields.
     * @complexity O(1), since the file has a fixed number of expected fields.
     */
    static AllocConfig parseConfig(const std::string& filename);

private:
    /**
     * @brief Split a comma-separated token list into individual tokens.
     * @complexity O(n), where n is the input string length.
     */
    static std::vector<std::string> splitComma(const std::string& s);

    /**
     * @brief Strip leading and trailing whitespace from a string.
     * @complexity O(n), where n is the input string length.
     */
    static std::string trim(const std::string& s);
};