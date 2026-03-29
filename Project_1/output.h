/**
 * @file output.h
 * @brief Output formatter for the conference assignment tool.
 *
 * Generates CSV files and console output in the exact format specified
 * by the project description (Figures 2, 3, and 4).
 *
 * @par Output sections (in order):
 * 1. Assignment by submission: #SubmissionId,ReviewerId,Match
 * 2. Assignment by reviewer:   #ReviewerId,SubmissionId,Match
 * 3. Total count:              #Total: N
 * 4. Unfulfilled (if any):     #SubmissionId,Domain,MissingReviews
 * 5. Risk analysis (if any):   #Risk Analysis: M
 *
 * @author DA 2026 - Programming Project I
 * @date Spring 2026
 * @version 1.0
 */

#ifndef OUTPUT_H
#define OUTPUT_H

#include "domain.h"

#include <string>
#include <fstream>
#include <iostream>
#include <sstream>
#include <algorithm>
#include <map>

/**
 * @brief Formats and writes solver results to file and/or console.
 *
 * All output methods are static — this is a utility class with no state.
 */
class OutputWriter {
public:
    /**
     * @brief Writes the complete assignment output to a file.
     *
     * Generates the output file in the format specified by the project PDF.
     * Includes assignment tables (by submission and by reviewer), totals,
     * unfulfilled submissions, and — when @p includeRisk is true and risk
     * analysis was configured — the risk analysis section as well.
     *
     * @param filename    Path to the output file.
     * @param result      The solver result to format.
     * @param data        The problem data (for config access).
     * @param includeRisk If true (default) and risk is configured, appends the
     *                    risk section to the same file.  Pass false when the
     *                    caller intends to write risk to a separate file
     *                    (batch mode with an explicit risk output path).
     *
     * @par Complexity
     * \f$O(A \log A + U \log U + R \log R)\f$ where A = assignments,
     * U = unfulfilled, R = risky reviewers (due to sorting).
     */
    static void writeToFile(const std::string& filename,
                            const SolverResult& result,
                            const ProblemData& data,
                            bool includeRisk = true) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open output file: " << filename << std::endl;
            return;
        }

        writeAssignmentSection(file, result, data);

        if (includeRisk && data.config.shouldAnalyzeRisk()) {
            file << "\n";
            writeRiskSection(file, result, data.config.riskAnalysis);
        }

        file.close();

        std::cout << "Output written to: " << filename << std::endl;
    }

    /**
     * @brief Writes the risk analysis results to a separate file.
     *
     * Format (Figure 4 of the PDF):
     * @code
     *   #Risk Analysis: M
     *   id1, id2, ...
     * @endcode
     *
     * @param filename   Path to the risk output file.
     * @param result     The solver result with riskyReviewers populated.
     * @param riskParam  The risk analysis parameter value (M).
     */
    static void writeRiskToFile(const std::string& filename,
                                const SolverResult& result,
                                int riskParam) {
        std::ofstream file(filename);
        if (!file.is_open()) {
            std::cerr << "Error: Cannot open risk output file: " << filename << std::endl;
            return;
        }

        writeRiskSection(file, result, riskParam);
        file.close();

        std::cout << "Risk analysis written to: " << filename << std::endl;
    }

    /**
     * @brief Prints the assignment results to the console.
     *
     * Same format as file output, directed to stdout.
     *
     * @param result The solver result.
     * @param data   The problem data.
     */
    static void printToConsole(const SolverResult& result,
                               const ProblemData& data) {
        writeAssignmentSection(std::cout, result, data);
        if (data.config.shouldAnalyzeRisk()) {
            std::cout << std::endl;
            writeRiskSection(std::cout, result, data.config.riskAnalysis);
        }
    }

    /**
     * @brief Prints a summary of the problem data (for menu option).
     *
     * Shows submission count, reviewer count, parameter values,
     * and a capacity feasibility check.
     *
     * @param data The parsed problem data.
     */
    static void printSummary(const ProblemData& data) {
        std::cout << "\n===== Problem Summary =====\n";
        std::cout << "Submissions: " << data.numSubmissions() << "\n";
        std::cout << "Reviewers:   " << data.numReviewers() << "\n";
        std::cout << "\n--- Parameters ---\n";
        std::cout << "MinReviewsPerSubmission: "
                  << data.config.minReviewsPerSubmission << "\n";
        std::cout << "MaxReviewsPerReviewer:   "
                  << data.config.maxReviewsPerReviewer << "\n";
        std::cout << "MatchMode:               "
                  << matchModeToString(data.config.matchMode) << "\n";
        std::cout << "RiskAnalysis:            "
                  << data.config.riskAnalysis << "\n";
        std::cout << "OutputFile:              "
                  << data.config.outputFileName << "\n";

        std::cout << "\n--- Capacity Check ---\n";
        long long totalCap = static_cast<long long>(data.numReviewers()) *
                             data.config.maxReviewsPerReviewer;
        long long totalDem = static_cast<long long>(data.numSubmissions()) *
                             data.config.minReviewsPerSubmission;
        std::cout << "Total reviewer capacity: " << totalCap << "\n";
        std::cout << "Total review demand:     " << totalDem << "\n";
        std::cout << "Feasibility (necessary): "
                  << (data.hasEnoughCapacity() ? "PASS" : "FAIL") << "\n";
        std::cout << "===========================\n" << std::endl;
    }

    /**
     * @brief Lists all submissions with their domains (and title/authors/email if available).
     * @param data The parsed problem data.
     */
    static void listSubmissions(const ProblemData& data) {
        std::cout << "\n--- Submissions (" << data.numSubmissions() << ") ---\n";
        for (const auto& s : data.submissions) {
            std::cout << "  ID=" << s.id;
            if (!s.title.empty())   std::cout << "  Title=\""   << s.title   << "\"";
            if (!s.authors.empty()) std::cout << "  Authors=\"" << s.authors << "\"";
            if (!s.email.empty())   std::cout << "  Email="     << s.email;
            std::cout << "  Primary=" << s.primaryDomain;
            if (s.secondaryDomain.has_value()) {
                std::cout << "  Secondary=" << s.secondaryDomain.value();
            }
            std::cout << "\n";
        }
        std::cout << std::endl;
    }

    /**
     * @brief Lists all reviewers with their expertise (and name/email if available).
     * @param data The parsed problem data.
     */
    static void listReviewers(const ProblemData& data) {
        std::cout << "\n--- Reviewers (" << data.numReviewers() << ") ---\n";
        for (const auto& r : data.reviewers) {
            std::cout << "  ID=" << r.id;
            if (!r.name.empty())  std::cout << "  Name=\"" << r.name  << "\"";
            if (!r.email.empty()) std::cout << "  Email="  << r.email;
            std::cout << "  Primary=" << r.primaryExpertise;
            if (r.secondaryExpertise.has_value()) {
                std::cout << "  Secondary=" << r.secondaryExpertise.value();
            }
            std::cout << "\n";
        }
        std::cout << std::endl;
    }

private:
    /**
     * @brief Writes the assignment sections to any output stream.
     *
     * Handles both successful (Figure 2) and unsuccessful (Figure 3) cases.
     *
     * @param os     Output stream (file or cout).
     * @param result Solver results.
     * @param data   Problem data.
     */
    static void writeAssignmentSection(std::ostream& os,
                                       const SolverResult& result,
                                       const ProblemData& data) {
        int totalDemand = static_cast<int>(data.numSubmissions()) *
                          data.config.minReviewsPerSubmission;

        if (result.isComplete(totalDemand) || !result.assignments.empty()) {
            // --- Section 1: Assignments by submission (Figure 2, part 1) ---
            os << "#SubmissionId,ReviewerId,Match\n";
            for (const auto& a : result.assignments) {
                os << a.submissionId << ", "
                   << a.reviewerId << ", "
                   << a.matchedDomain << "\n";
            }

            // --- Section 2: Assignments by reviewer (Figure 2, part 2) ---
            // Build reviewer → submissions map
            std::map<EntityId, std::vector<std::pair<EntityId, DomainCode>>> byReviewer;
            for (const auto& a : result.assignments) {
                byReviewer[a.reviewerId].push_back({a.submissionId, a.matchedDomain});
            }

            os << "#ReviewerId,SubmissionId,Match\n";
            for (auto& [revId, subs] : byReviewer) {
                // Sort submissions within each reviewer
                std::sort(subs.begin(), subs.end());
                for (const auto& [subId, domain] : subs) {
                    os << revId << ", " << subId << ", " << domain << "\n";
                }
            }

            // --- Section 3: Total ---
            os << "#Total: " << result.assignments.size() << "\n";
        }

        // --- Section 4: Unfulfilled submissions (Figure 3) ---
        if (!result.unfulfilled.empty()) {
            os << "#SubmissionId,Domain,MissingReviews\n";
            for (const auto& u : result.unfulfilled) {
                os << u.submissionId << ", "
                   << u.domain << ", "
                   << u.missingReviews << "\n";
            }
        }
    }

    /**
     * @brief Writes the risk analysis section to any output stream.
     *
     * Format matches Figure 4 of the PDF.
     *
     * @param os        Output stream.
     * @param result    Solver results with riskyReviewers.
     * @param riskParam The M parameter value.
     */
    static void writeRiskSection(std::ostream& os,
                                 const SolverResult& result,
                                 int riskParam) {
        os << "#Risk Analysis: " << riskParam << "\n";
        if (result.riskyReviewers.empty()) {
            os << "No risky reviewers found.\n";
        } else {
            for (size_t i = 0; i < result.riskyReviewers.size(); i++) {
                if (i > 0) os << ", ";
                os << result.riskyReviewers[i];
            }
            os << "\n";
        }
    }

    /**
     * @brief Converts a MatchMode to a human-readable string.
     * @param mode The MatchMode enum.
     * @return Descriptive string.
     */
    static std::string matchModeToString(MatchMode mode) {
        switch (mode) {
            case MatchMode::SILENT:            return "0 (Silent)";
            case MatchMode::PRIMARY_ONLY:      return "1 (Primary only)";
            case MatchMode::SUB_BOTH_REV_PRI:  return "2 (Sub P+S, Rev P)";
            case MatchMode::ALL:               return "3 (All domains)";
            default:                           return "Unknown";
        }
    }
};

#endif // OUTPUT_H
