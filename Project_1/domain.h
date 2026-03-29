/**
 * @file domain.h
 * @brief Domain entities for the Scientific Conference Organization Tool.
 *
 * Defines the core data structures that model the problem domain:
 * submissions, reviewers, configuration parameters, and assignment results.
 * These structures are populated by the Parser and consumed by the
 * network-flow solver (Graph + Edmonds-Karp).
 *
 * @author DA 2026 - Programming Project I
 * @date Spring 2026
 * @version 1.0
 */

#ifndef DOMAIN_H
#define DOMAIN_H

#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <optional>
#include <stdexcept>
#include <sstream>

// ============================================================================
// Forward declarations
// ============================================================================
struct Submission;
struct Reviewer;
struct Config;
struct Assignment;
struct UnfulfilledSubmission;

// ============================================================================
// Domain type aliases
// ============================================================================

/**
 * @brief Numeric identifier for submissions and reviewers.
 *
 * The PDF specifies integer identifiers (e.g., 31, 87).
 * Using int is sufficient for the expected range (IDs <= 10^6).
 */
using EntityId = int;

/**
 * @brief Numeric code representing a scientific domain / topic area.
 *
 * Domains are encoded as positive integers (e.g., 1 = "AI", 4 = "Networks").
 * The exact mapping is opaque to the solver — only equality matters for matching.
 */
using DomainCode = int;

// ============================================================================
// Submission
// ============================================================================

/**
 * @brief Represents a single paper submission to the conference.
 *
 * Each submission has a unique identifier, a mandatory primary domain,
 * and an optional secondary domain. The domains determine which reviewers
 * are eligible to review this submission.
 *
 * @note For T2.1, only the primary domain is used for matching.
 *       T2.4 (General Formulation) also considers the secondary domain.
 *
 * @par Example (from Figure 1 of the project description):
 * @code
 *   Submission s;
 *   s.id              = 87;
 *   s.primaryDomain   = 1;
 *   s.secondaryDomain = std::nullopt;  // not specified
 * @endcode
 */
struct Submission {
    EntityId                    id;                ///< Unique submission identifier (must be positive).
    std::string                 title;             ///< Paper title (may contain commas if quoted in CSV).
    std::string                 authors;           ///< Author list string.
    std::string                 email;             ///< Contact e-mail address.
    DomainCode                  primaryDomain;     ///< Mandatory primary scientific domain.
    std::optional<DomainCode>   secondaryDomain;   ///< Optional secondary scientific domain.

    /**
     * @brief Checks if this submission covers the given domain.
     *
     * Used by the network builder to determine eligibility edges.
     *
     * @param domain  The domain code to test against.
     * @param usePrimary   Consider the primary domain.
     * @param useSecondary Consider the secondary domain (if present).
     * @return true if any of the active domains matches.
     *
     * @par Complexity
     * \f$O(1)\f$
     */
    [[nodiscard]] bool hasDomain(DomainCode domain,
                                 bool usePrimary   = true,
                                 bool useSecondary = false) const {
        if (usePrimary && primaryDomain == domain) return true;
        if (useSecondary && secondaryDomain.has_value() && secondaryDomain.value() == domain) return true;
        return false;
    }
};

// ============================================================================
// Reviewer
// ============================================================================

/**
 * @brief Represents a reviewer (expert) available to review submissions.
 *
 * Each reviewer has a unique identifier, a mandatory primary expertise domain,
 * and an optional secondary expertise domain. These domains are matched against
 * submission domains to build the bipartite compatibility graph.
 *
 * @note The reviewer does NOT store its assigned submissions — those are
 *       derived from the max-flow solution at extraction time.
 *
 * @par Example (from Figure 1 of the project description):
 * @code
 *   Reviewer r;
 *   r.id                = 1;
 *   r.primaryExpertise  = 1;
 *   r.secondaryExpertise = std::nullopt;
 * @endcode
 */
struct Reviewer {
    EntityId                    id;                  ///< Unique reviewer identifier (must be positive).
    std::string                 name;                ///< Reviewer full name.
    std::string                 email;               ///< Reviewer e-mail address.
    DomainCode                  primaryExpertise;    ///< Mandatory primary domain of expertise.
    std::optional<DomainCode>   secondaryExpertise;  ///< Optional secondary domain of expertise.

    /**
     * @brief Checks if this reviewer has expertise in the given domain.
     *
     * @param domain  The domain code to test against.
     * @param usePrimary   Consider the primary expertise.
     * @param useSecondary Consider the secondary expertise (if present).
     * @return true if any of the active expertise areas matches.
     *
     * @par Complexity
     * \f$O(1)\f$
     */
    [[nodiscard]] bool hasExpertise(DomainCode domain,
                                    bool usePrimary   = true,
                                    bool useSecondary = false) const {
        if (usePrimary && primaryExpertise == domain) return true;
        if (useSecondary && secondaryExpertise.has_value() && secondaryExpertise.value() == domain) return true;
        return false;
    }
};

// ============================================================================
// GenerateAssignments mode
// ============================================================================

/**
 * @brief Enumerates the matching strategies for GenerateAssignments.
 *
 * Controls which combination of primary/secondary domains and expertise
 * are considered when building the bipartite compatibility edges.
 *
 * | Value | Submission domains | Reviewer expertise |
 * |-------|--------------------|--------------------|
 * | 0     | (run but don't report) | Primary only  |
 * | 1     | Primary only       | Primary only       |
 * | 2     | Primary + Secondary| Primary only       |
 * | 3     | Primary + Secondary| Primary + Secondary|
 */
enum class MatchMode : int {
    SILENT        = 0,  ///< Assignment runs but output is suppressed.
    PRIMARY_ONLY  = 1,  ///< T2.1: Only primary domains vs. primary expertise.
    SUB_BOTH_REV_PRI = 2, ///< Primary+Secondary submission domains vs. primary expertise.
    ALL           = 3   ///< T2.4: All domains and all expertise considered.
};

/**
 * @brief Converts an integer from the input file to a MatchMode.
 * @param value Integer value (0–3).
 * @return Corresponding MatchMode enum.
 * @throws std::invalid_argument if value is out of range.
 */
inline MatchMode toMatchMode(int value) {
    if (value < 0 || value > 3) {
        throw std::invalid_argument(
            "GenerateAssignments must be 0–3, got: " + std::to_string(value));
    }
    return static_cast<MatchMode>(value);
}

// ============================================================================
// Config — Global problem parameters
// ============================================================================

/**
 * @brief Aggregates all configuration parameters parsed from the input file.
 *
 * These parameters come from the `#Parameters` and `#Control` sections
 * of the CSV input and govern the network construction and solver behavior.
 *
 * @par Invariants (validated by the Parser):
 * - MinReviewsPerSubmission >= 1
 * - MaxReviewsPerReviewer >= 1
 * - riskAnalysis >= 0
 * - outputFileName is non-empty when matchMode != SILENT
 */
struct Config {
    // --- From #Parameters section ---

    int minReviewsPerSubmission = 1;   ///< Minimum reviews each submission must receive.
    int maxReviewsPerReviewer   = 1;   ///< Maximum reviews each reviewer can perform.

    /**
     * @brief Whether the input provides primary reviewer expertise (from #Parameters).
     *
     * Maps to "PrimaryReviewerExpertise" field: 0 = no, 1 = yes.
     * Should always be 1 (mandatory) in well-formed input.
     */
    bool primaryReviewerExpertise = true;

    /**
     * @brief Whether to consider secondary reviewer expertise (from #Parameters).
     *
     * Maps to "SecondaryReviewerExpertise" field: 0 = no, 1 = yes.
     * This is an input parameter distinct from matchMode — it indicates whether
     * the data file provides secondary expertise, but the actual use depends on matchMode.
     */
    bool secondaryReviewerExpertise = false;

    /**
     * @brief Whether the input provides primary submission domains.
     *
     * Maps to "PrimarySubmissionDomain" field. Should always be 1 (mandatory).
     */
    bool primarySubmissionDomain = true;

    /**
     * @brief Whether the input provides secondary submission domains.
     *
     * Maps to "SecondarySubmissionDomain" field: 0 = no, 1 = yes.
     */
    bool secondarySubmissionDomain = false;

    // --- From #Control section ---

    MatchMode matchMode   = MatchMode::PRIMARY_ONLY;  ///< Which domains to consider (GenerateAssignments).
    int       riskAnalysis = 0;                        ///< Risk analysis parameter (0 = none, 1 = single, K>1 = K reviewers).
    std::string outputFileName = "output.csv";         ///< Output file name (default: "output.csv").

    // --- Convenience query methods ---

    /**
     * @brief Determines if submission secondary domains should be used in matching.
     *
     * Depends on both the matchMode (which strategy) and whether the data
     * actually provides secondary submission domains.
     *
     * @return true if matchMode >= SUB_BOTH_REV_PRI and data has secondary domains.
     *
     * @par Complexity
     * \f$O(1)\f$
     */
    [[nodiscard]] bool useSubmissionSecondary() const {
        return (matchMode == MatchMode::SUB_BOTH_REV_PRI ||
                matchMode == MatchMode::ALL) &&
               secondarySubmissionDomain;
    }

    /**
     * @brief Determines if reviewer secondary expertise should be used in matching.
     *
     * @return true if matchMode == ALL and data has secondary expertise.
     *
     * @par Complexity
     * \f$O(1)\f$
     */
    [[nodiscard]] bool useReviewerSecondary() const {
        return matchMode == MatchMode::ALL &&
               secondaryReviewerExpertise;
    }

    /**
     * @brief Whether the assignment output should be written to a file.
     * @return true if matchMode is not SILENT.
     */
    [[nodiscard]] bool shouldReport() const {
        return matchMode != MatchMode::SILENT;
    }

    /**
     * @brief Whether risk analysis should be performed.
     * @return true if riskAnalysis > 0.
     */
    [[nodiscard]] bool shouldAnalyzeRisk() const {
        return riskAnalysis > 0;
    }
};

// ============================================================================
// Assignment result structures
// ============================================================================

/**
 * @brief A single assignment: one reviewer assigned to one submission.
 *
 * Corresponds to one unit of flow on a Reviewer→Submission edge
 * in the max-flow solution.
 *
 * @par Output format (Figure 2 of the PDF):
 * @code
 *   SubmissionId, ReviewerId, MatchDomain
 * @endcode
 */
struct Assignment {
    EntityId   submissionId;   ///< The submission being reviewed.
    EntityId   reviewerId;     ///< The reviewer assigned to it.
    DomainCode matchedDomain;  ///< The domain that justified the match.

    /**
     * @brief Ordering for sorted output: by submissionId, then reviewerId.
     */
    bool operator<(const Assignment& other) const {
        if (submissionId != other.submissionId) return submissionId < other.submissionId;
        return reviewerId < other.reviewerId;
    }
};

/**
 * @brief Describes a submission that could not receive enough reviews.
 *
 * When the max-flow is less than n * MinRev, some submissions are
 * under-reviewed. This struct captures which submission, which domain
 * is missing, and how many additional reviews are needed.
 *
 * @par Output format (Figure 3 of the PDF):
 * @code
 *   #SubmissionId, Domain, MissingReviews
 *   31, 3, 2
 * @endcode
 */
struct UnfulfilledSubmission {
    EntityId   submissionId;   ///< The under-reviewed submission.
    DomainCode domain;         ///< The domain for which reviews are missing.
    int        missingReviews; ///< Number of additional reviews still needed.

    /**
     * @brief Ordering for sorted output: by submissionId.
     */
    bool operator<(const UnfulfilledSubmission& other) const {
        return submissionId < other.submissionId;
    }
};

// ============================================================================
// ProblemData — Aggregated parsed input
// ============================================================================

/**
 * @brief Holds all parsed data ready to be consumed by the solver.
 *
 * This is the output of the Parser and the input to the Graph builder.
 * It owns all submissions, reviewers, and configuration.
 *
 * @par Design decision — vectors vs. maps:
 * We store entities in vectors for cache-friendly iteration during graph
 * construction, and additionally maintain unordered_maps for O(1) lookup
 * by external ID (needed during parsing for duplicate detection and during
 * output generation for reverse-mapping graph node indices → entity IDs).
 */
struct ProblemData {
    std::vector<Submission> submissions;   ///< All parsed submissions (order of appearance).
    std::vector<Reviewer>   reviewers;     ///< All parsed reviewers (order of appearance).
    Config                  config;        ///< Parsed configuration parameters.

    /**
     * @brief Maps external submission ID → index in the submissions vector.
     *
     * Populated during parsing. Used for:
     * 1. Duplicate detection (O(1) lookup).
     * 2. Graph builder: mapping submission ID → graph node index.
     *
     * @par Invariant:
     * submissionIndex[submissions[i].id] == i, for all valid i.
     */
    std::unordered_map<EntityId, size_t> submissionIndex;

    /**
     * @brief Maps external reviewer ID → index in the reviewers vector.
     *
     * @par Invariant:
     * reviewerIndex[reviewers[j].id] == j, for all valid j.
     */
    std::unordered_map<EntityId, size_t> reviewerIndex;

    // --- Convenience accessors ---

    /**
     * @brief Number of submissions.
     * @return Size of the submissions vector.
     */
    [[nodiscard]] size_t numSubmissions() const { return submissions.size(); }

    /**
     * @brief Number of reviewers.
     * @return Size of the reviewers vector.
     */
    [[nodiscard]] size_t numReviewers() const { return reviewers.size(); }

    /**
     * @brief Adds a submission with duplicate-ID detection.
     *
     * @param sub The submission to insert.
     * @throws std::runtime_error if a submission with the same ID already exists.
     *
     * @par Complexity
     * Amortized \f$O(1)\f$.
     */
    void addSubmission(const Submission& sub) {
        if (submissionIndex.count(sub.id)) {
            throw std::runtime_error(
                "Duplicate submission ID: " + std::to_string(sub.id));
        }
        submissionIndex[sub.id] = submissions.size();
        submissions.push_back(sub);
    }

    /**
     * @brief Adds a reviewer with duplicate-ID detection.
     *
     * @param rev The reviewer to insert.
     * @throws std::runtime_error if a reviewer with the same ID already exists.
     *
     * @par Complexity
     * Amortized \f$O(1)\f$.
     */
    void addReviewer(const Reviewer& rev) {
        if (reviewerIndex.count(rev.id)) {
            throw std::runtime_error(
                "Duplicate reviewer ID: " + std::to_string(rev.id));
        }
        reviewerIndex[rev.id] = reviewers.size();
        reviewers.push_back(rev);
    }

    /**
     * @brief Quick sanity check: is total reviewer capacity sufficient?
     *
     * Verifies the necessary (but not sufficient) condition:
     * \f$ m \times \text{MaxRev} \geq n \times \text{MinRev} \f$
     *
     * @return true if total capacity is at least total demand.
     *
     * @par Complexity
     * \f$O(1)\f$
     */
    [[nodiscard]] bool hasEnoughCapacity() const {
        long long totalCapacity = static_cast<long long>(reviewers.size()) *
                                  config.maxReviewsPerReviewer;
        long long totalDemand   = static_cast<long long>(submissions.size()) *
                                  config.minReviewsPerSubmission;
        return totalCapacity >= totalDemand;
    }

    /**
     * @brief Collects all distinct domain codes present in submissions.
     *
     * Useful for diagnostics and for the general formulation (T2.4).
     *
     * @return Set of all domain codes referenced by any submission.
     *
     * @par Complexity
     * \f$O(n)\f$ where \f$n\f$ = number of submissions.
     */
    [[nodiscard]] std::unordered_set<DomainCode> allSubmissionDomains() const {
        std::unordered_set<DomainCode> domains;
        for (const auto& s : submissions) {
            domains.insert(s.primaryDomain);
            if (s.secondaryDomain.has_value()) {
                domains.insert(s.secondaryDomain.value());
            }
        }
        return domains;
    }

    /**
     * @brief Collects all distinct expertise codes present in reviewers.
     *
     * @return Set of all expertise codes referenced by any reviewer.
     *
     * @par Complexity
     * \f$O(m)\f$ where \f$m\f$ = number of reviewers.
     */
    [[nodiscard]] std::unordered_set<DomainCode> allReviewerExpertise() const {
        std::unordered_set<DomainCode> domains;
        for (const auto& r : reviewers) {
            domains.insert(r.primaryExpertise);
            if (r.secondaryExpertise.has_value()) {
                domains.insert(r.secondaryExpertise.value());
            }
        }
        return domains;
    }
};

// ============================================================================
// SolverResult — Output of the max-flow solver
// ============================================================================

/**
 * @brief Encapsulates the complete result of solving the assignment problem.
 *
 * Produced by the max-flow solver after running Edmonds-Karp and extracting
 * assignments from the flow values on Reviewer→Submission edges.
 */
struct SolverResult {
    int maxFlow = 0;   ///< Total flow value = total number of assignments made.

    std::vector<Assignment>             assignments;    ///< All (reviewer, submission) pairs with flow = 1.
    std::vector<UnfulfilledSubmission>  unfulfilled;    ///< Submissions that didn't get enough reviews.

    /**
     * @brief Risk analysis results: list of "risky" reviewer IDs.
     *
     * For RiskAnalysis = 1: reviewers whose individual removal makes the
     * assignment infeasible. For RiskAnalysis = K: sets of K reviewers whose
     * combined removal causes infeasibility. (For K>1, this stores the
     * flattened list of offending reviewer IDs from the worst-case analysis.)
     */
    std::vector<EntityId> riskyReviewers;

    /**
     * @brief Whether the assignment is fully satisfied.
     *
     * @param totalDemand Expected total flow = n * MinRev.
     * @return true if maxFlow == totalDemand.
     */
    [[nodiscard]] bool isComplete(int totalDemand) const {
        return maxFlow == totalDemand;
    }
};

// ============================================================================
// Error reporting
// ============================================================================

/**
 * @brief Custom exception for parsing errors with line-number context.
 *
 * Thrown by the Parser when the input file contains malformed data,
 * enabling clear error messages that reference the offending line.
 */
class ParseError : public std::runtime_error {
public:
    /**
     * @brief Constructs a ParseError with file context.
     *
     * @param message Description of the error.
     * @param lineNumber 1-based line number where the error was detected.
     * @param section The section header (e.g., "#Submissions") being parsed.
     */
    ParseError(const std::string& message, int lineNumber,
               const std::string& section = "")
        : std::runtime_error(buildMessage(message, lineNumber, section)),
          lineNumber_(lineNumber), section_(section) {}

    [[nodiscard]] int         lineNumber() const { return lineNumber_; }
    [[nodiscard]] std::string section()    const { return section_; }

private:
    int         lineNumber_;
    std::string section_;

    static std::string buildMessage(const std::string& msg, int line,
                                    const std::string& section) {
        std::ostringstream oss;
        oss << "Parse error";
        if (!section.empty()) oss << " in [" << section << "]";
        oss << " at line " << line << ": " << msg;
        return oss.str();
    }
};

#endif // DOMAIN_H
