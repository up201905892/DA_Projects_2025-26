/**
 * @file main.cpp
 * @brief Entry point for the Scientific Conference Organization Tool.
 *
 * Supports two execution modes as specified in the project description:
 *
 * 1. **Interactive mode** (default): Presents a command-line menu that
 *    exposes all implemented functionalities.
 *
 * 2. **Batch mode** (`-b` flag): Reads input, solves, and writes output
 *    non-interactively. Invoked as:
 *    @code
 *      ./myProg -b input.csv [risk.csv]
 *    @endcode
 *
 * @par Implemented tasks:
 * - T1.1: Menu interface + batch mode
 * - T1.2: Input parsing and data inspection
 * - T2.1: Basic assignment (primary domains, no risk)
 * - T2.2: Risk analysis K=1
 *
 * @author DA 2026 - Programming Project I
 * @date Spring 2026
 * @version 1.0
 */

#include "parser.h"
#include "graph.h"
#include "output.h"

#include <iostream>
#include <string>
#include <memory>

// ============================================================================
// Batch mode
// ============================================================================

/**
 * @brief Executes the tool in batch mode (non-interactive).
 *
 * Usage: ./myProg -b input.csv output.csv
 *
 * All output — assignments AND risk analysis (if configured) — is written
 * to a single @p outputFile.  The filename stored in the CSV's OutputFileName
 * parameter is ignored in batch mode; the command-line argument takes precedence.
 *
 * @param inputFile   Path to the input CSV file.
 * @param outputFile  Path to the (unified) output file.
 * @return 0 on success, 1 on error.
 */
int runBatch(const std::string& inputFile, const std::string& outputFile) {
    try {
        Parser parser;
        ProblemData data = parser.parse(inputFile);

        SolverResult result;

        if (data.config.shouldAnalyzeRisk()) {
            result = Solver::solveWithRiskAnalysis(data);
        } else {
            result = Solver::solve(data);
        }

        // Single unified output: assignments first, then risk (if configured).
        // writeToFile appends the risk section when includeRisk=true (the default).
        OutputWriter::writeToFile(outputFile, result, data);

        return 0;

    } catch (const ParseError& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "ERROR: " << e.what() << std::endl;
        return 1;
    }
}

// ============================================================================
// Interactive mode
// ============================================================================

/**
 * @brief Displays the interactive menu.
 */
void showMenu() {
    std::cout << "\n========================================\n";
    std::cout << "  Conference Review Assignment Tool\n";
    std::cout << "  DA 2026 - Programming Project I\n";
    std::cout << "========================================\n";
    std::cout << "  1. Load input file\n";
    std::cout << "  2. Show problem summary\n";
    std::cout << "  3. List submissions\n";
    std::cout << "  4. List reviewers\n";
    std::cout << "  5. Run assignment (Max-Flow)\n";
    std::cout << "  6. Run assignment with Risk Analysis\n";
    std::cout << "  7. Show last result\n";
    std::cout << "  0. Exit\n";
    std::cout << "========================================\n";
    std::cout << "  Choice: ";
}

/**
 * @brief Runs the tool in interactive mode with a command-line menu.
 *
 * If @p preloadedFile is non-empty the file is parsed immediately and a
 * summary is printed before the menu loop begins, giving the user instant
 * feedback that the data was loaded successfully.
 *
 * @param preloadedFile Optional path to pre-load before entering the menu.
 * @return 0 on normal exit.
 */
int runInteractive(const std::string& preloadedFile = "") {
    std::unique_ptr<ProblemData> data;
    std::unique_ptr<SolverResult> lastResult;

    // --- Pre-load file if one was provided on the command line ---
    if (!preloadedFile.empty()) {
        try {
            Parser parser;
            data = std::make_unique<ProblemData>(parser.parse(preloadedFile));
            std::cout << "  File loaded: " << preloadedFile << "\n";
            OutputWriter::printSummary(*data);
        } catch (const std::exception& e) {
            std::cerr << "  ERROR loading file: " << e.what() << std::endl;
            data.reset();
        }
    }

    int choice = -1;
    while (choice != 0) {
        showMenu();

        if (!(std::cin >> choice)) {
            std::cin.clear();
            std::cin.ignore(10000, '\n');
            std::cout << "  Invalid input. Enter a number.\n";
            continue;
        }

        switch (choice) {
        case 1: {
            // --- Load input file ---
            std::cout << "  Enter input file path: ";
            std::string filename;
            std::cin >> filename;
            try {
                Parser parser;
                data = std::make_unique<ProblemData>(parser.parse(filename));
                lastResult.reset();
                std::cout << "  File loaded successfully.\n";
                OutputWriter::printSummary(*data);
            } catch (const std::exception& e) {
                std::cerr << "  ERROR: " << e.what() << std::endl;
                data.reset();
            }
            break;
        }

        case 2: {
            // --- Problem summary ---
            if (!data) {
                std::cout << "  No data loaded. Use option 1 first.\n";
                break;
            }
            OutputWriter::printSummary(*data);
            break;
        }

        case 3: {
            // --- List submissions ---
            if (!data) {
                std::cout << "  No data loaded. Use option 1 first.\n";
                break;
            }
            OutputWriter::listSubmissions(*data);
            break;
        }

        case 4: {
            // --- List reviewers ---
            if (!data) {
                std::cout << "  No data loaded. Use option 1 first.\n";
                break;
            }
            OutputWriter::listReviewers(*data);
            break;
        }

        case 5: {
            // --- Run assignment (T2.1) ---
            if (!data) {
                std::cout << "  No data loaded. Use option 1 first.\n";
                break;
            }

            std::cout << "  Running Max-Flow assignment...\n";
            lastResult = std::make_unique<SolverResult>(Solver::solve(*data));

            int totalDemand = static_cast<int>(data->numSubmissions()) *
                              data->config.minReviewsPerSubmission;

            std::cout << "  Max-Flow value: " << lastResult->maxFlow
                      << " / " << totalDemand << "\n";

            if (lastResult->isComplete(totalDemand)) {
                std::cout << "  Status: COMPLETE (all submissions assigned)\n";
            } else {
                std::cout << "  Status: PARTIAL ("
                          << lastResult->unfulfilled.size()
                          << " submissions under-reviewed)\n";
            }

            // Write to file if reporting is enabled
            if (data->config.shouldReport()) {
                OutputWriter::writeToFile(data->config.outputFileName,
                                          *lastResult, *data);
            }

            // Print to console
            OutputWriter::printToConsole(*lastResult, *data);
            break;
        }

        case 6: {
            // --- Run with Risk Analysis (T2.2) ---
            if (!data) {
                std::cout << "  No data loaded. Use option 1 first.\n";
                break;
            }

            int riskK;
            std::cout << "  Enter risk parameter K (1 for single reviewer): ";
            std::cin >> riskK;
            if (riskK < 1) {
                std::cout << "  Invalid K. Must be >= 1.\n";
                break;
            }

            // Temporarily set risk parameter
            int origRisk = data->config.riskAnalysis;
            data->config.riskAnalysis = riskK;

            std::cout << "  Running Max-Flow with Risk Analysis (K="
                      << riskK << ")...\n";

            if (riskK == 1) {
                lastResult = std::make_unique<SolverResult>(
                    Solver::solveWithRiskAnalysis(*data));
            } else {
                // K > 1: run base assignment, risk K>1 not yet implemented
                std::cout << "  Note: Risk K > 1 analysis is outlined but not "
                          << "fully implemented (T2.3).\n";
                lastResult = std::make_unique<SolverResult>(Solver::solve(*data));
            }

            int totalDemand = static_cast<int>(data->numSubmissions()) *
                              data->config.minReviewsPerSubmission;

            std::cout << "  Max-Flow value: " << lastResult->maxFlow
                      << " / " << totalDemand << "\n";

            if (!lastResult->riskyReviewers.empty()) {
                std::cout << "  Risky reviewers (" << lastResult->riskyReviewers.size()
                          << "): ";
                for (size_t i = 0; i < lastResult->riskyReviewers.size(); i++) {
                    if (i > 0) std::cout << ", ";
                    std::cout << lastResult->riskyReviewers[i];
                }
                std::cout << "\n";
            } else if (lastResult->isComplete(totalDemand)) {
                std::cout << "  No risky reviewers found (assignment is robust).\n";
            }

            // Write outputs
            if (data->config.shouldReport()) {
                OutputWriter::writeToFile(data->config.outputFileName,
                                          *lastResult, *data);
            }
            OutputWriter::printToConsole(*lastResult, *data);

            // Restore original risk parameter
            data->config.riskAnalysis = origRisk;
            break;
        }

        case 7: {
            // --- Show last result ---
            if (!lastResult) {
                std::cout << "  No results yet. Run option 5 or 6 first.\n";
                break;
            }
            if (!data) {
                std::cout << "  No data loaded.\n";
                break;
            }
            OutputWriter::printToConsole(*lastResult, *data);
            break;
        }

        case 0:
            std::cout << "  Exiting. Goodbye!\n";
            break;

        default:
            std::cout << "  Invalid choice. Try again.\n";
            break;
        }
    }

    return 0;
}

// ============================================================================
// Main entry point
// ============================================================================

/**
 * @brief Program entry point.
 *
 * @par Usage:
 * - Interactive:                `./myProg`
 * - Interactive (pre-load):     `./myProg input.csv`
 * - Batch:                      `./myProg -b input.csv output.csv`
 *
 * Batch mode requires **exactly** 4 arguments: the program name, `-b`, the
 * input file, and the output file.  All output (assignments and risk analysis)
 * goes to @c output.csv.
 *
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return 0 on success, 1 on error.
 */
int main(int argc, char* argv[]) {
    // --- Batch mode: ./myProg -b input.csv output.csv ---
    if (argc == 4 && std::string(argv[1]) == "-b") {
        return runBatch(argv[2], argv[3]);
    }

    // --- Interactive mode ---
    // If a single filename argument is provided, pre-load it before the menu.
    if (argc == 2 && std::string(argv[1]) != "-b") {
        return runInteractive(argv[1]);
    }

    return runInteractive();
}
