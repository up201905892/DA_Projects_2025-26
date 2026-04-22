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

std::string Parser::trim(const std::string& s) {
    size_t b = s.find_first_not_of(" \t\r\n");
    if (b == std::string::npos) return "";
    size_t e = s.find_last_not_of(" \t\r\n");
    return s.substr(b, e - b + 1);
}

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

        // Split on first ':'
        size_t colon = tl.find(':');
        if (colon == std::string::npos)
            throw std::runtime_error("Line " + std::to_string(lineNum) +
                                     ": missing ':' in '" + tl + "'");

        std::string varName = trim(tl.substr(0, colon));
        std::string rest    = trim(tl.substr(colon + 1));

        if (varName.empty())
            throw std::runtime_error("Line " + std::to_string(lineNum) +
                                     ": empty variable name");

        std::vector<std::string> tokens = splitComma(rest);
        if (tokens.empty())
            throw std::runtime_error("Line " + std::to_string(lineNum) +
                                     ": no tokens after ':'");

        LiveRange lr;
        lr.start = -1;
        lr.end   = -1;
        std::vector<int> allLines;

        for (const std::string& tok : tokens) {
            if (tok.empty())
                throw std::runtime_error("Line " + std::to_string(lineNum) +
                                         ": empty token");

            bool isDef      = (tok.back() == '+');
            bool isLastUse  = (tok.back() == '-');
            std::string num = (isDef || isLastUse) ? tok.substr(0, tok.size() - 1)
                                                   : tok;
            int lineNo;
            try {
                lineNo = std::stoi(num);
            } catch (...) {
                throw std::runtime_error("Line " + std::to_string(lineNum) +
                                         ": invalid line number '" + tok + "'");
            }
            if (lineNo < 0)
                throw std::runtime_error("Line " + std::to_string(lineNum) +
                                         ": negative line number");

            if (isDef) {
                if (lr.start != -1)
                    throw std::runtime_error("Line " + std::to_string(lineNum) +
                                             ": multiple '+' markers");
                lr.start = lineNo;
            } else if (isLastUse) {
                if (lr.end != -1)
                    throw std::runtime_error("Line " + std::to_string(lineNum) +
                                             ": multiple '-' markers");
                lr.end = lineNo;
            }
            allLines.push_back(lineNo);
        }

        if (lr.start == -1)
            throw std::runtime_error("Line " + std::to_string(lineNum) +
                                     ": missing '+' (definition) marker for '" +
                                     varName + "'");
        if (lr.end == -1)
            throw std::runtime_error("Line " + std::to_string(lineNum) +
                                     ": missing '-' (last-use) marker for '" +
                                     varName + "'");

        // Build intermediate list (everything except start and end)
        for (int ln : allLines)
            if (ln != lr.start && ln != lr.end)
                lr.intermediate.push_back(ln);

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

AllocConfig Parser::parseConfig(const std::string& filename) {
    std::ifstream f(filename);
    if (!f.is_open())
        throw std::runtime_error("Cannot open config file: " + filename);

    AllocConfig cfg;
    bool gotRegisters = false, gotAlgorithm = false;
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

        std::string key   = trim(tl.substr(0, colon));
        std::string value = trim(tl.substr(colon + 1));
        // Lowercase key for comparison
        std::string keyLow = key;
        std::transform(keyLow.begin(), keyLow.end(), keyLow.begin(), ::tolower);

        if (keyLow == "registers") {
            try {
                cfg.numRegisters = std::stoi(value);
            } catch (...) {
                throw std::runtime_error("Config line " + std::to_string(lineNum) +
                                         ": invalid register count '" + value + "'");
            }
            if (cfg.numRegisters <= 0)
                throw std::runtime_error("Config line " + std::to_string(lineNum) +
                                         ": register count must be positive");
            gotRegisters = true;

        } else if (keyLow == "algorithm") {
            // value can be: "basic", "spilling, K", "splitting, K", "free"
            std::vector<std::string> parts = Parser::splitComma(value);
            if (parts.empty())
                throw std::runtime_error("Config line " + std::to_string(lineNum) +
                                         ": empty algorithm specification");

            std::string algName = parts[0];
            std::transform(algName.begin(), algName.end(), algName.begin(), ::tolower);

            if (algName == "basic") {
                cfg.type = AlgorithmType::BASIC;
            } else if (algName == "spilling") {
                cfg.type = AlgorithmType::SPILLING;
                if (parts.size() < 2)
                    throw std::runtime_error("Config line " + std::to_string(lineNum) +
                                             ": 'spilling' requires K parameter");
                try { cfg.K = std::stoi(parts[1]); }
                catch (...) {
                    throw std::runtime_error("Config line " + std::to_string(lineNum) +
                                             ": invalid K for spilling");
                }
            } else if (algName == "splitting") {
                cfg.type = AlgorithmType::SPLITTING;
                if (parts.size() < 2)
                    throw std::runtime_error("Config line " + std::to_string(lineNum) +
                                             ": 'splitting' requires K parameter");
                try { cfg.K = std::stoi(parts[1]); }
                catch (...) {
                    throw std::runtime_error("Config line " + std::to_string(lineNum) +
                                             ": invalid K for splitting");
                }
            } else if (algName == "free") {
                cfg.type = AlgorithmType::FREE;
            } else {
                throw std::runtime_error("Config line " + std::to_string(lineNum) +
                                         ": unknown algorithm '" + algName + "'");
            }
            gotAlgorithm = true;

        } else {
            // Unknown key — warn but continue
        }
    }

    if (!gotRegisters)
        throw std::runtime_error("Config file missing 'registers' field: " + filename);
    if (!gotAlgorithm)
        throw std::runtime_error("Config file missing 'algorithm' field: " + filename);

    return cfg;
}
