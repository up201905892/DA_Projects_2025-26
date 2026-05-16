/**
 * @file Parser.cpp
 * @brief Implementation of Parser for ranges and config files.
 */

#include "Parser.h"
#include <fstream>
#include <sstream>
#include <algorithm>
#include <stdexcept>
#include <cctype>

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

/**
 * @brief Strip leading and trailing whitespace from a string.
 *
 * @param s Input string.
 * @return Trimmed string.
 * @complexity O(n), where n is the string length.
 */
std::string Parser::trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";

    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

/**
 * @brief Split a comma-separated string and trim each token.
 *
 * @param s Input comma-separated string.
 * @return Vector of trimmed tokens.
 * @complexity O(n), where n is the string length.
 */
std::vector<std::string> Parser::splitComma(const std::string& s) {
    std::vector<std::string> result;
    std::stringstream ss(s);
    std::string token;

    while (std::getline(ss, token, ','))
        result.push_back(trim(token));

    return result;
}

// ---------------------------------------------------------------------------
// parseRanges
// ---------------------------------------------------------------------------

/**
 * @brief Parse the live-ranges input file.
 *
 * This parser accepts the full format described in the project statement. In
 * particular, a live range is not required to contain both a '+' and a '-'
 * marker. Some ranges may contain only plain line numbers because they describe
 * intersection points between live ranges.
 *
 * @param filename Path to the ranges file.
 * @return Vector of parsed range entries.
 * @throws std::runtime_error on malformed input.
 * @complexity O(L * T), where L is the number of input lines and T is the
 *             average number of tokens per live range.
 */
std::vector<RangeEntry> Parser::parseRanges(const std::string& filename) {
    std::ifstream f(filename);
    if (!f.is_open())
        throw std::runtime_error("Cannot open ranges file: " + filename);

    std::vector<RangeEntry> entries;
    std::string line;
    int lineNum = 0;

    while (std::getline(f, line)) {
        ++lineNum;

        std::string tl = trim(line);
        if (tl.empty() || tl[0] == '#') continue;

        // Split on first ':'.
        size_t colon = tl.find(':');
        if (colon == std::string::npos)
            throw std::runtime_error("Line " + std::to_string(lineNum) +
                                     ": missing ':' in '" + tl + "'");

        std::string varName = trim(tl.substr(0, colon));
        std::string rest = trim(tl.substr(colon + 1));

        if (varName.empty())
            throw std::runtime_error("Line " + std::to_string(lineNum) +
                                     ": empty variable name");

        std::vector<std::string> tokens = splitComma(rest);
        if (tokens.empty())
            throw std::runtime_error("Line " + std::to_string(lineNum) +
                                     ": no tokens after ':'");

        std::vector<int> allLines;
        int defLine = -1;
        int endLine = -1;

        bool hasDef = false;
        bool hasEnd = false;

        for (const std::string& tok : tokens) {
            if (tok.empty())
                throw std::runtime_error("Line " + std::to_string(lineNum) +
                                         ": empty token");

            bool isDef = (tok.back() == '+');
            bool isLastUse = (tok.back() == '-');

            if (isDef && isLastUse)
                throw std::runtime_error("Line " + std::to_string(lineNum) +
                                         ": invalid token '" + tok + "'");

            std::string num = (isDef || isLastUse)
                                ? tok.substr(0, tok.size() - 1)
                                : tok;

            if (num.empty())
                throw std::runtime_error("Line " + std::to_string(lineNum) +
                                         ": missing line number in token '" +
                                         tok + "'");

            int lineNo;
            try {
                size_t parsed = 0;
                lineNo = std::stoi(num, &parsed);

                if (parsed != num.size())
                    throw std::invalid_argument("trailing characters");
            } catch (...) {
                throw std::runtime_error("Line " + std::to_string(lineNum) +
                                         ": invalid line number '" + tok + "'");
            }

            if (lineNo < 0)
                throw std::runtime_error("Line " + std::to_string(lineNum) +
                                         ": negative line number");

            if (isDef) {
                if (hasDef)
                    throw std::runtime_error("Line " + std::to_string(lineNum) +
                                             ": multiple '+' markers");
                hasDef = true;
                defLine = lineNo;
            }

            if (isLastUse) {
                if (hasEnd)
                    throw std::runtime_error("Line " + std::to_string(lineNum) +
                                             ": multiple '-' markers");
                hasEnd = true;
                endLine = lineNo;
            }

            allLines.push_back(lineNo);
        }

        if (allLines.empty())
            throw std::runtime_error("Line " + std::to_string(lineNum) +
                                     ": empty live range for '" + varName + "'");

        LiveRange lr;

        // If there is a '+' marker, that line is the start point. Otherwise,
        // use the first listed program point as the range start.
        lr.start = hasDef ? defLine : allLines.front();

        // If there is a '-' marker, that line is the end point. Otherwise,
        // use the last listed program point as the range end.
        lr.end = hasEnd ? endLine : allLines.back();

        lr.hasStartMarker = hasDef;
        lr.hasEndMarker = hasEnd;

        // Store all other points as intermediate points. Duplicates are removed
        // to keep later web construction deterministic.
        for (int ln : allLines) {
            if (ln != lr.start && ln != lr.end)
                lr.intermediate.push_back(ln);
        }

        std::sort(lr.intermediate.begin(), lr.intermediate.end());
        lr.intermediate.erase(
            std::unique(lr.intermediate.begin(), lr.intermediate.end()),
            lr.intermediate.end()
        );

        entries.push_back({varName, lr});
    }

    if (entries.empty())
        throw std::runtime_error("Ranges file contains no valid entries: " +
                                 filename);

    return entries;
}

// ---------------------------------------------------------------------------
// parseConfig
// ---------------------------------------------------------------------------

/**
 * @brief Parse the register and algorithm configuration file.
 *
 * Expected fields:
 * @code
 * registers: N
 * algorithm: basic
 * algorithm: spilling, K
 * algorithm: splitting, K
 * algorithm: free
 * @endcode
 *
 * @param filename Path to the config file.
 * @return Parsed allocation configuration.
 * @throws std::runtime_error on malformed input or missing fields.
 * @complexity O(1), since the file contains a fixed number of relevant fields.
 */
AllocConfig Parser::parseConfig(const std::string& filename) {
    std::ifstream f(filename);
    if (!f.is_open())
        throw std::runtime_error("Cannot open config file: " + filename);

    AllocConfig cfg;
    bool gotRegisters = false;
    bool gotAlgorithm = false;

    std::string line;
    int lineNum = 0;

    while (std::getline(f, line)) {
        ++lineNum;

        std::string tl = trim(line);
        if (tl.empty() || tl[0] == '#') continue;

        size_t colon = tl.find(':');
        if (colon == std::string::npos)
            throw std::runtime_error("Config line " + std::to_string(lineNum) +
                                     ": missing ':'");

        std::string key = trim(tl.substr(0, colon));
        std::string value = trim(tl.substr(colon + 1));

        std::string keyLow = key;
        std::transform(keyLow.begin(), keyLow.end(), keyLow.begin(),
                       [](unsigned char c) { return std::tolower(c); });

        if (keyLow == "registers") {
            try {
                size_t parsed = 0;
                cfg.numRegisters = std::stoi(value, &parsed);

                if (parsed != value.size())
                    throw std::invalid_argument("trailing characters");
            } catch (...) {
                throw std::runtime_error("Config line " +
                                         std::to_string(lineNum) +
                                         ": invalid register count '" +
                                         value + "'");
            }

            if (cfg.numRegisters <= 0)
                throw std::runtime_error("Config line " +
                                         std::to_string(lineNum) +
                                         ": register count must be positive");

            gotRegisters = true;

        } else if (keyLow == "algorithm") {
            // Accepted values:
            //   basic
            //   spilling, K
            //   splitting, K
            //   free
            std::vector<std::string> parts = Parser::splitComma(value);
            if (parts.empty())
                throw std::runtime_error("Config line " +
                                         std::to_string(lineNum) +
                                         ": empty algorithm specification");

            std::string algName = parts[0];
            std::transform(algName.begin(), algName.end(), algName.begin(),
                           [](unsigned char c) { return std::tolower(c); });

            if (algName == "basic") {
                if (parts.size() != 1)
                    throw std::runtime_error("Config line " +
                                             std::to_string(lineNum) +
                                             ": 'basic' does not take parameters");

                cfg.type = AlgorithmType::BASIC;
                cfg.K = 0;

            } else if (algName == "spilling") {
                if (parts.size() != 2)
                    throw std::runtime_error("Config line " +
                                             std::to_string(lineNum) +
                                             ": 'spilling' requires exactly one K parameter");

                cfg.type = AlgorithmType::SPILLING;

                try {
                    size_t parsed = 0;
                    cfg.K = std::stoi(parts[1], &parsed);

                    if (parsed != parts[1].size())
                        throw std::invalid_argument("trailing characters");
                } catch (...) {
                    throw std::runtime_error("Config line " +
                                             std::to_string(lineNum) +
                                             ": invalid K for spilling");
                }

                if (cfg.K < 0)
                    throw std::runtime_error("Config line " +
                                             std::to_string(lineNum) +
                                             ": K for spilling cannot be negative");

            } else if (algName == "splitting") {
                if (parts.size() != 2)
                    throw std::runtime_error("Config line " +
                                             std::to_string(lineNum) +
                                             ": 'splitting' requires exactly one K parameter");

                cfg.type = AlgorithmType::SPLITTING;

                try {
                    size_t parsed = 0;
                    cfg.K = std::stoi(parts[1], &parsed);

                    if (parsed != parts[1].size())
                        throw std::invalid_argument("trailing characters");
                } catch (...) {
                    throw std::runtime_error("Config line " +
                                             std::to_string(lineNum) +
                                             ": invalid K for splitting");
                }

                if (cfg.K < 0)
                    throw std::runtime_error("Config line " +
                                             std::to_string(lineNum) +
                                             ": K for splitting cannot be negative");

            } else if (algName == "free") {
                if (parts.size() != 1)
                    throw std::runtime_error("Config line " +
                                             std::to_string(lineNum) +
                                             ": 'free' does not take parameters");

                cfg.type = AlgorithmType::FREE;
                cfg.K = 0;

            } else {
                throw std::runtime_error("Config line " +
                                         std::to_string(lineNum) +
                                         ": unknown algorithm '" + algName + "'");
            }

            gotAlgorithm = true;

        } else {
            // Unknown keys are ignored to keep the parser tolerant to comments
            // or extra metadata fields.
        }
    }

    if (!gotRegisters)
        throw std::runtime_error("Config file missing 'registers' field: " +
                                 filename);

    if (!gotAlgorithm)
        throw std::runtime_error("Config file missing 'algorithm' field: " +
                                 filename);

    return cfg;
}