/**
 * @file parser.h
 * @brief Robust parser for the conference assignment input file format.
 *
 * Implements a finite state machine that reads the custom CSV format
 * described in the project specification (Figure 1). The file has four
 * sections delimited by header lines starting with '#':
 *
 *   #Submissions   →  id, primaryDomain [, secondaryDomain]
 *   #Reviewers     →  id, primaryExpertise [, secondaryExpertise]
 *   #Parameters    →  key, value
 *   #Control       →  key, value
 *
 * @par Design: State Machine
 * The parser transitions between states as it encounters section headers.
 * This makes it resilient to:
 * - Sections appearing in any order
 * - Empty lines between entries
 * - Trailing whitespace
 * - Missing optional fields
 *
 * @par Complexity
 * - Time:  \f$O(L)\f$ where \f$L\f$ = total characters in the input file.
 * - Space: \f$O(n + m)\f$ where \f$n\f$ = submissions, \f$m\f$ = reviewers.
 *
 * @author DA 2026 - Programming Project I
 * @date Spring 2026
 * @version 1.0
 */

#ifndef PARSER_H
#define PARSER_H

#include "domain.h"

#include <string>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <iostream>
#include <cctype>

/**
 * @brief Parses the conference input file into a ProblemData structure.
 *
 * Uses a finite state machine with 5 states (IDLE, SUBMISSIONS, REVIEWERS,
 * PARAMETERS, CONTROL) to determine how each line should be interpreted.
 *
 * @par Error Handling Strategy:
 * - Structural errors (duplicate IDs, missing mandatory sections) throw ParseError.
 * - Warnings (extra whitespace, empty lines) are logged to stderr but don't abort.
 * - The parser collects ALL errors before throwing, giving the user a complete report.
 *
 * @par Usage:
 * @code
 *   Parser parser;
 *   ProblemData data = parser.parse("input.csv");
 * @endcode
 */
class Parser {
public:
    /**
     * @brief Parses the input file and returns a fully populated ProblemData.
     *
     * @param filename Path to the input CSV file.
     * @return ProblemData containing submissions, reviewers, and config.
     * @throws ParseError on malformed input (with line number and section).
     * @throws std::runtime_error if the file cannot be opened.
     *
     * @par Complexity
     * \f$O(L)\f$ where \f$L\f$ = total characters in the file.
     */
    ProblemData parse(const std::string& filename) const {
        std::ifstream file(filename);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open input file: " + filename);
        }

        ProblemData data;
        State state = State::IDLE;
        std::string line;
        int lineNumber = 0;
        std::vector<std::string> errors;

        // Track which mandatory sections we've seen
        bool seenSubmissions = false;
        bool seenReviewers   = false;
        bool seenParameters  = false;
        bool seenControl     = false;

        while (std::getline(file, line)) {
            lineNumber++;
            line = trim(line);

            // Skip empty lines
            if (line.empty()) continue;

            // --- State transition: detect section headers ---
            if (line[0] == '#') {
                std::string header = toLower(line);

                if (header == "#submissions") {
                    if (seenSubmissions) {
                        errors.push_back("Line " + std::to_string(lineNumber) +
                                         ": Duplicate #Submissions section.");
                    }
                    state = State::SUBMISSIONS;
                    seenSubmissions = true;
                } else if (header == "#reviewers") {
                    if (seenReviewers) {
                        errors.push_back("Line " + std::to_string(lineNumber) +
                                         ": Duplicate #Reviewers section.");
                    }
                    state = State::REVIEWERS;
                    seenReviewers = true;
                } else if (header == "#parameters") {
                    if (seenParameters) {
                        errors.push_back("Line " + std::to_string(lineNumber) +
                                         ": Duplicate #Parameters section.");
                    }
                    state = State::PARAMETERS;
                    seenParameters = true;
                } else if (header == "#control") {
                    if (seenControl) {
                        errors.push_back("Line " + std::to_string(lineNumber) +
                                         ": Duplicate #Control section.");
                    }
                    state = State::CONTROL;
                    seenControl = true;
                } else {
                    // Unknown section header or column-header comment — skip silently
                    // Do NOT change state: e.g. "#Id, Title, ..." is a column header
                    // inside a section and must not reset the current parsing state.
                }
                continue; // Header line consumed, move to next line
            }

            // --- Data lines: dispatch to section-specific parser ---
            try {
                switch (state) {
                    case State::SUBMISSIONS:
                        parseSubmissionLine(line, lineNumber, data);
                        break;
                    case State::REVIEWERS:
                        parseReviewerLine(line, lineNumber, data);
                        break;
                    case State::PARAMETERS:
                        parseParameterLine(line, lineNumber, data.config);
                        break;
                    case State::CONTROL:
                        parseControlLine(line, lineNumber, data.config);
                        break;
                    case State::IDLE:
                        // Data before any section header — ignore with warning
                        std::cerr << "Warning: line " << lineNumber
                                  << " is outside any section, skipping." << std::endl;
                        break;
                }
            } catch (const ParseError& e) {
                errors.push_back(e.what());
            }
        }

        // --- Post-parse validation ---
        if (!seenSubmissions) errors.push_back("Missing mandatory section: #Submissions");
        if (!seenReviewers)   errors.push_back("Missing mandatory section: #Reviewers");
        if (!seenParameters)  errors.push_back("Missing mandatory section: #Parameters");
        if (!seenControl)     errors.push_back("Missing mandatory section: #Control");

        if (data.submissions.empty() && seenSubmissions) {
            errors.push_back("#Submissions section is empty — no submissions to assign.");
        }
        if (data.reviewers.empty() && seenReviewers) {
            errors.push_back("#Reviewers section is empty — no reviewers available.");
        }

        // Validate parameter consistency
        if (data.config.minReviewsPerSubmission < 0) {
            errors.push_back("MinReviewsPerSubmission must be >= 0, got: " +
                             std::to_string(data.config.minReviewsPerSubmission));
        }
        if (data.config.maxReviewsPerReviewer < 0) {
            errors.push_back("MaxReviewsPerReviewer must be >= 0, got: " +
                             std::to_string(data.config.maxReviewsPerReviewer));
        }
        if (data.config.riskAnalysis < 0) {
            errors.push_back("RiskAnalysis must be >= 0, got: " +
                             std::to_string(data.config.riskAnalysis));
        }

        // Report all collected errors
        if (!errors.empty()) {
            std::ostringstream oss;
            oss << "Input file has " << errors.size() << " error(s):\n";
            for (size_t i = 0; i < errors.size(); i++) {
                oss << "  [" << (i + 1) << "] " << errors[i] << "\n";
            }
            throw ParseError(oss.str(), 0, "validation");
        }

        return data;
    }

private:
    /**
     * @brief The finite set of states for the parser state machine.
     *
     * Transitions happen when a line starting with '#' is encountered.
     * Data lines are dispatched to the handler matching the current state.
     *
     * @verbatim
     *  ┌──────┐  #Submissions  ┌─────────────┐
     *  │ IDLE │───────────────>│ SUBMISSIONS  │
     *  └──┬───┘                └─────────────┘
     *     │  #Reviewers        ┌─────────────┐
     *     ├───────────────────>│  REVIEWERS   │
     *     │                    └─────────────┘
     *     │  #Parameters       ┌─────────────┐
     *     ├───────────────────>│ PARAMETERS   │
     *     │                    └─────────────┘
     *     │  #Control          ┌─────────────┐
     *     └───────────────────>│   CONTROL    │
     *                          └─────────────┘
     * @endverbatim
     */
    enum class State {
        IDLE,           ///< Before any section or after an unknown header.
        SUBMISSIONS,    ///< Reading submission entries.
        REVIEWERS,      ///< Reading reviewer entries.
        PARAMETERS,     ///< Reading key-value parameter pairs.
        CONTROL         ///< Reading key-value control pairs.
    };

    // ========================================================================
    // Section parsers
    // ========================================================================

    /**
     * @brief Parses a single submission line.
     *
     * Expected CSV format: `Id, Title, Authors, E-mail, Primary [, Secondary]`
     * - 5 fields: no secondary domain.
     * - 6 fields: includes optional secondary domain.
     * Title may contain commas if the field is quoted (handled by splitCSV).
     *
     * Legacy format without text fields (`Id, Primary [, Secondary]`) is also
     * accepted for backwards compatibility with unit-test mock data.
     *
     * @param line      Trimmed line content.
     * @param lineNum   1-based line number for error reporting.
     * @param data      ProblemData to populate.
     * @throws ParseError on malformed line or duplicate ID.
     *
     * @par Complexity
     * \f$O(k)\f$ where \f$k\f$ = length of the line.
     */
    static void parseSubmissionLine(const std::string& line, int lineNum,
                                    ProblemData& data) {
        auto tokens = splitCSV(line);

        Submission sub;

        // Full format: Id, Title, Authors, E-mail, Primary [, Secondary]  → 5 or 6 tokens
        // Legacy format: Id, Primary [, Secondary]                         → 2 or 3 tokens
        if (tokens.size() == 5 || tokens.size() == 6) {
            sub.id      = parseIntField(tokens[0], "submission ID",   lineNum, "#Submissions");
            sub.title   = tokens[1];
            sub.authors = tokens[2];
            sub.email   = tokens[3];
            sub.primaryDomain = parseIntField(tokens[4], "primary domain", lineNum, "#Submissions");
            if (tokens.size() == 6) {
                sub.secondaryDomain = parseIntField(tokens[5], "secondary domain",
                                                    lineNum, "#Submissions");
            }
        } else if (tokens.size() == 2 || tokens.size() == 3) {
            // Legacy / test format: Id, Primary [, Secondary]
            sub.id            = parseIntField(tokens[0], "submission ID",   lineNum, "#Submissions");
            sub.primaryDomain = parseIntField(tokens[1], "primary domain",  lineNum, "#Submissions");
            if (tokens.size() == 3) {
                sub.secondaryDomain = parseIntField(tokens[2], "secondary domain",
                                                    lineNum, "#Submissions");
            }
        } else {
            throw ParseError(
                "Submission line expects 5–6 fields (Id, Title, Authors, E-mail, Primary"
                " [, Secondary]) or 2–3 legacy fields, got " +
                std::to_string(tokens.size()) + ": \"" + line + "\"",
                lineNum, "#Submissions");
        }

        // Validate: ID must be positive
        if (sub.id <= 0) {
            throw ParseError("Submission ID must be positive, got: " +
                             std::to_string(sub.id), lineNum, "#Submissions");
        }
        // Validate: domain codes must be positive
        if (sub.primaryDomain <= 0) {
            throw ParseError("Primary domain must be positive, got: " +
                             std::to_string(sub.primaryDomain), lineNum, "#Submissions");
        }
        if (sub.secondaryDomain.has_value() && sub.secondaryDomain.value() <= 0) {
            throw ParseError("Secondary domain must be positive, got: " +
                             std::to_string(sub.secondaryDomain.value()),
                             lineNum, "#Submissions");
        }

        try {
            data.addSubmission(sub);
        } catch (const std::runtime_error& e) {
            throw ParseError(e.what(), lineNum, "#Submissions");
        }
    }

    /**
     * @brief Parses a single reviewer line.
     *
     * Expected CSV format: `Id, Name, E-mail, Primary [, Secondary]`
     * - 4 fields: no secondary expertise.
     * - 5 fields: includes optional secondary expertise.
     *
     * Legacy format without text fields (`Id, Primary [, Secondary]`) is also
     * accepted for backwards compatibility with unit-test mock data.
     *
     * @param line      Trimmed line content.
     * @param lineNum   1-based line number for error reporting.
     * @param data      ProblemData to populate.
     * @throws ParseError on malformed line or duplicate ID.
     *
     * @par Complexity
     * \f$O(k)\f$ where \f$k\f$ = length of the line.
     */
    static void parseReviewerLine(const std::string& line, int lineNum,
                                  ProblemData& data) {
        auto tokens = splitCSV(line);

        Reviewer rev;

        // Full format: Id, Name, E-mail, Primary [, Secondary]  → 4 or 5 tokens
        // Legacy format: Id, Primary [, Secondary]              → 2 or 3 tokens
        if (tokens.size() == 4 || tokens.size() == 5) {
            rev.id    = parseIntField(tokens[0], "reviewer ID",       lineNum, "#Reviewers");
            rev.name  = tokens[1];
            rev.email = tokens[2];
            rev.primaryExpertise = parseIntField(tokens[3], "primary expertise",
                                                 lineNum, "#Reviewers");
            if (tokens.size() == 5) {
                rev.secondaryExpertise = parseIntField(tokens[4], "secondary expertise",
                                                       lineNum, "#Reviewers");
            }
        } else if (tokens.size() == 2 || tokens.size() == 3) {
            // Legacy / test format: Id, Primary [, Secondary]
            rev.id               = parseIntField(tokens[0], "reviewer ID",       lineNum, "#Reviewers");
            rev.primaryExpertise = parseIntField(tokens[1], "primary expertise",  lineNum, "#Reviewers");
            if (tokens.size() == 3) {
                rev.secondaryExpertise = parseIntField(tokens[2], "secondary expertise",
                                                       lineNum, "#Reviewers");
            }
        } else {
            throw ParseError(
                "Reviewer line expects 4–5 fields (Id, Name, E-mail, Primary"
                " [, Secondary]) or 2–3 legacy fields, got " +
                std::to_string(tokens.size()) + ": \"" + line + "\"",
                lineNum, "#Reviewers");
        }

        // Validate: ID must be positive
        if (rev.id <= 0) {
            throw ParseError("Reviewer ID must be positive, got: " +
                             std::to_string(rev.id), lineNum, "#Reviewers");
        }
        if (rev.primaryExpertise <= 0) {
            throw ParseError("Primary expertise must be positive, got: " +
                             std::to_string(rev.primaryExpertise), lineNum, "#Reviewers");
        }
        if (rev.secondaryExpertise.has_value() && rev.secondaryExpertise.value() <= 0) {
            throw ParseError("Secondary expertise must be positive, got: " +
                             std::to_string(rev.secondaryExpertise.value()),
                             lineNum, "#Reviewers");
        }

        try {
            data.addReviewer(rev);
        } catch (const std::runtime_error& e) {
            throw ParseError(e.what(), lineNum, "#Reviewers");
        }
    }

    /**
     * @brief Parses a parameter line: "KeyName, value"
     *
     * Recognized keys (case-insensitive matching):
     *   - MinReviewsPerSubmission → int
     *   - MaxReviewsPerReviewer → int
     *   - SecondaryReviewerExpertise → bool (0/1)
     *   - PrimarySubmissionDomain → bool (0/1)
     *   - SecondarySubmissionDomain → bool (0/1)
     *
     * @param line    Trimmed line content.
     * @param lineNum 1-based line number.
     * @param config  Config to populate.
     * @throws ParseError on unrecognized key or bad value.
     *
     * @par Complexity
     * \f$O(k)\f$ where \f$k\f$ = length of the line.
     */
    static void parseParameterLine(const std::string& line, int lineNum,
                                   Config& config) {
        auto tokens = splitCSV(line);
        if (tokens.size() != 2) {
            throw ParseError(
                "Parameter line expects 2 fields (key, value), got " +
                std::to_string(tokens.size()) + ": \"" + line + "\"",
                lineNum, "#Parameters");
        }

        std::string key = toLower(tokens[0]);

        if (key == "minreviewspersubmission") {
            config.minReviewsPerSubmission =
                parseIntField(tokens[1], "MinReviewsPerSubmission", lineNum, "#Parameters");
        } else if (key == "maxreviewsperreviewer") {
            config.maxReviewsPerReviewer =
                parseIntField(tokens[1], "MaxReviewsPerReviewer", lineNum, "#Parameters");
        } else if (key == "primaryreviewerexpertise") {
            config.primaryReviewerExpertise =
                parseBoolField(tokens[1], "PrimaryReviewerExpertise", lineNum, "#Parameters");
        } else if (key == "secondaryreviewerexpertise") {
            config.secondaryReviewerExpertise =
                parseBoolField(tokens[1], "SecondaryReviewerExpertise", lineNum, "#Parameters");
        } else if (key == "primarysubmissiondomain") {
            config.primarySubmissionDomain =
                parseBoolField(tokens[1], "PrimarySubmissionDomain", lineNum, "#Parameters");
        } else if (key == "secondarysubmissiondomain") {
            config.secondarySubmissionDomain =
                parseBoolField(tokens[1], "SecondarySubmissionDomain", lineNum, "#Parameters");
        } else {
            throw ParseError("Unrecognized parameter key: \"" + tokens[0] + "\"",
                             lineNum, "#Parameters");
        }
    }

    /**
     * @brief Parses a control line: "KeyName, value"
     *
     * Recognized keys (case-insensitive matching):
     *   - GenerateAssignments → int (0–3), mapped to MatchMode
     *   - RiskAnalysis → int (>= 0)
     *   - OutputFileName → string (possibly quoted)
     *
     * @param line    Trimmed line content.
     * @param lineNum 1-based line number.
     * @param config  Config to populate.
     * @throws ParseError on unrecognized key or bad value.
     *
     * @par Complexity
     * \f$O(k)\f$ where \f$k\f$ = length of the line.
     */
    static void parseControlLine(const std::string& line, int lineNum,
                                 Config& config) {
        auto tokens = splitCSV(line);
        if (tokens.size() != 2) {
            throw ParseError(
                "Control line expects 2 fields (key, value), got " +
                std::to_string(tokens.size()) + ": \"" + line + "\"",
                lineNum, "#Control");
        }

        std::string key = toLower(tokens[0]);

        if (key == "generateassignments") {
            int val = parseIntField(tokens[1], "GenerateAssignments", lineNum, "#Control");
            try {
                config.matchMode = toMatchMode(val);
            } catch (const std::invalid_argument& e) {
                throw ParseError(e.what(), lineNum, "#Control");
            }
        } else if (key == "riskanalysis") {
            config.riskAnalysis =
                parseIntField(tokens[1], "RiskAnalysis", lineNum, "#Control");
        } else if (key == "outputfilename") {
            config.outputFileName = stripQuotes(tokens[1]);
            if (config.outputFileName.empty()) {
                throw ParseError("OutputFileName cannot be empty.", lineNum, "#Control");
            }
        } else {
            throw ParseError("Unrecognized control key: \"" + tokens[0] + "\"",
                             lineNum, "#Control");
        }
    }

    // ========================================================================
    // Utility functions
    // ========================================================================

    /**
     * @brief Splits a CSV line by commas, respecting double-quoted fields.
     *
     * Commas inside double quotes are treated as part of the field value.
     * The surrounding quotes are stripped from the result. Each token is
     * trimmed of leading/trailing whitespace. Empty tokens (e.g., from a
     * trailing comma used to mark an absent optional field) are discarded.
     *
     * @param line The input line.
     * @return Vector of trimmed, unquoted tokens (empty tokens excluded).
     *
     * @par Example:
     * @code
     *   splitCSV("31, \"Silva, João\", foo@bar.com, 4")
     *     → {"31", "Silva, João", "foo@bar.com", "4"}
     *   splitCSV("  31 , 4 , 2  ") → {"31", "4", "2"}
     *   splitCSV("87, \"Title\", Author, email, 1,") → {"87","Title","Author","email","1"}
     * @endcode
     *
     * @par Complexity
     * \f$O(k)\f$ where \f$k\f$ = length of the line.
     */
    static std::vector<std::string> splitCSV(const std::string& line) {
        std::vector<std::string> tokens;
        std::string current;
        bool inQuotes = false;

        for (size_t i = 0; i < line.size(); ++i) {
            char c = line[i];
            if (c == '"') {
                inQuotes = !inQuotes;   // toggle quote mode; quote char itself not stored
            } else if (c == ',' && !inQuotes) {
                std::string t = trim(current);
                if (!t.empty()) {
                    tokens.push_back(t);
                }
                current.clear();
            } else {
                current += c;
            }
        }

        // Flush the last field
        std::string t = trim(current);
        if (!t.empty()) {
            tokens.push_back(t);
        }

        return tokens;
    }

    /**
     * @brief Trims leading and trailing whitespace from a string.
     *
     * @param s Input string.
     * @return Trimmed copy.
     *
     * @par Complexity
     * \f$O(k)\f$
     */
    static std::string trim(const std::string& s) {
        auto start = s.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        auto end = s.find_last_not_of(" \t\r\n");
        return s.substr(start, end - start + 1);
    }

    /**
     * @brief Converts a string to lowercase (for case-insensitive comparison).
     *
     * @param s Input string.
     * @return Lowercase copy.
     *
     * @par Complexity
     * \f$O(k)\f$
     */
    static std::string toLower(const std::string& s) {
        std::string result = s;
        std::transform(result.begin(), result.end(), result.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        return result;
    }

    /**
     * @brief Removes surrounding double quotes from a string value.
     *
     * @param s Input string (e.g., "\"assign.csv\"").
     * @return Unquoted string (e.g., "assign.csv").
     *
     * @par Complexity
     * \f$O(k)\f$
     */
    static std::string stripQuotes(const std::string& s) {
        std::string result = trim(s);
        if (result.size() >= 2 && result.front() == '"' && result.back() == '"') {
            result = result.substr(1, result.size() - 2);
        }
        return result;
    }

    /**
     * @brief Parses a trimmed token as an integer with error context.
     *
     * @param token    The string token to parse.
     * @param fieldName Human-readable name of the field (for error messages).
     * @param lineNum  1-based line number.
     * @param section  Section name.
     * @return Parsed integer value.
     * @throws ParseError if the token is not a valid integer.
     *
     * @par Complexity
     * \f$O(k)\f$ where \f$k\f$ = length of the token.
     */
    static int parseIntField(const std::string& token, const std::string& fieldName,
                             int lineNum, const std::string& section) {
        try {
            size_t pos = 0;
            int value = std::stoi(token, &pos);
            // Ensure the entire token was consumed (no trailing garbage)
            if (pos != token.size()) {
                throw ParseError(
                    fieldName + " has trailing characters: \"" + token + "\"",
                    lineNum, section);
            }
            return value;
        } catch (const std::invalid_argument&) {
            throw ParseError(
                fieldName + " is not a valid integer: \"" + token + "\"",
                lineNum, section);
        } catch (const std::out_of_range&) {
            throw ParseError(
                fieldName + " is out of range: \"" + token + "\"",
                lineNum, section);
        }
    }

    /**
     * @brief Parses a trimmed token as a boolean (0 or 1).
     *
     * @param token     The string token ("0" or "1").
     * @param fieldName Human-readable field name.
     * @param lineNum   1-based line number.
     * @param section   Section name.
     * @return true if token is "1", false if "0".
     * @throws ParseError if the token is neither "0" nor "1".
     *
     * @par Complexity
     * \f$O(1)\f$
     */
    static bool parseBoolField(const std::string& token, const std::string& fieldName,
                               int lineNum, const std::string& section) {
        int value = parseIntField(token, fieldName, lineNum, section);
        if (value != 0 && value != 1) {
            throw ParseError(
                fieldName + " must be 0 or 1, got: " + std::to_string(value),
                lineNum, section);
        }
        return value == 1;
    }
};

#endif // PARSER_H
