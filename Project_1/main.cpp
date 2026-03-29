/**
 * @file main.cpp
 * @brief Entry point for the Scientific Conference Organization Tool.
 *
 * Supports two execution modes:
 *
 * 1. Interactive mode (default): Presents a command-line menu.
 * 2. Batch mode (-b): ./myProg -b input.csv output.csv
 *
 * Interactive conventions used in this project:
 * - Input test files are assumed to be in:  dataset/input/
 * - Output files are written to:            dataset/output/
 */

#include "parser.h"
#include "graph.h"
#include "output.h"

#include <iostream>
#include <string>
#include <memory>
#include <fstream>
#include <limits>

// ============================================================================
// Helper functions
// ============================================================================

std::string buildInputPath(const std::string& filename) {
    return "dataset/input/" + filename;
}

std::string extractFileName(const std::string& path) {
    size_t pos1 = path.find_last_of('/');
    size_t pos2 = path.find_last_of('\\');
    size_t pos;

    if (pos1 == std::string::npos && pos2 == std::string::npos) {
        return path;
    } else if (pos1 == std::string::npos) {
        pos = pos2;
    } else if (pos2 == std::string::npos) {
        pos = pos1;
    } else {
        pos = std::max(pos1, pos2);
    }

    return path.substr(pos + 1);
}

std::string buildInteractiveOutputPath(const std::string& configuredName) {
    return "dataset/output/" + extractFileName(configuredName);
}

bool fileExistsReadable(const std::string& path) {
    std::ifstream f(path.c_str());
    return f.good();
}

bool readInt(int& value) {
    if (std::cin >> value) return true;

    std::cin.clear();
    std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
    return false;
}

// ============================================================================
// Batch mode
// ============================================================================

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

        std::ofstream test(outputFile.c_str());
        if (!test.is_open()) {
            std::cerr << "ERROR: Cannot open output file: " << outputFile << std::endl;
            return 1;
        }
        test.close();

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

int runInteractive(const std::string& preloadedFile = "") {
    std::unique_ptr<ProblemData> data;
    std::unique_ptr<SolverResult> lastResult;

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

        if (!readInt(choice)) {
            std::cout << "  Invalid input. Enter a number.\n";
            continue;
        }

        switch (choice) {
        case 1: {
            std::cout << "  Enter input filename (inside dataset/input/): ";
            std::string filename;
            std::cin >> filename;

            std::string fullPath = buildInputPath(filename);

            if (!fileExistsReadable(fullPath)) {
                std::cerr << "  ERROR: Cannot open input file: " << fullPath << std::endl;
                break;
            }

            try {
                Parser parser;
                data = std::make_unique<ProblemData>(parser.parse(fullPath));
                lastResult.reset();
                std::cout << "  File loaded successfully: " << fullPath << "\n";
                OutputWriter::printSummary(*data);
            } catch (const std::exception& e) {
                std::cerr << "  ERROR: " << e.what() << std::endl;
                data.reset();
            }
            break;
        }

        case 2: {
            if (!data) {
                std::cout << "  No data loaded. Use option 1 first.\n";
                break;
            }
            OutputWriter::printSummary(*data);
            break;
        }

        case 3: {
            if (!data) {
                std::cout << "  No data loaded. Use option 1 first.\n";
                break;
            }
            OutputWriter::listSubmissions(*data);
            break;
        }

        case 4: {
            if (!data) {
                std::cout << "  No data loaded. Use option 1 first.\n";
                break;
            }
            OutputWriter::listReviewers(*data);
            break;
        }

        case 5: {
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

            if (data->config.shouldReport()) {
                std::string outputPath = buildInteractiveOutputPath(data->config.outputFileName);
                OutputWriter::writeToFile(outputPath, *lastResult, *data);
            }

            OutputWriter::printToConsole(*lastResult, *data);
            break;
        }

        case 6: {
            if (!data) {
                std::cout << "  No data loaded. Use option 1 first.\n";
                break;
            }

            std::cout << "  Enter risk parameter K (1 for single reviewer): ";
            int riskK;
            if (!readInt(riskK)) {
                std::cout << "  Invalid input. K must be an integer.\n";
                break;
            }

            if (riskK < 1) {
                std::cout << "  Invalid K. Must be >= 1.\n";
                break;
            }

            int originalRisk = data->config.riskAnalysis;
            data->config.riskAnalysis = riskK;

            std::cout << "  Running Max-Flow with Risk Analysis (K="
                      << riskK << ")...\n";

            if (riskK == 1) {
                lastResult = std::make_unique<SolverResult>(
                    Solver::solveWithRiskAnalysis(*data));
            } else {
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
                for (size_t i = 0; i < lastResult->riskyReviewers.size(); ++i) {
                    if (i > 0) std::cout << ", ";
                    std::cout << lastResult->riskyReviewers[i];
                }
                std::cout << "\n";
            } else if (lastResult->isComplete(totalDemand)) {
                std::cout << "  No risky reviewers found (assignment is robust).\n";
            }

            if (data->config.shouldReport()) {
                std::string outputPath = buildInteractiveOutputPath(data->config.outputFileName);
                OutputWriter::writeToFile(outputPath, *lastResult, *data);
            }

            OutputWriter::printToConsole(*lastResult, *data);

            data->config.riskAnalysis = originalRisk;
            break;
        }

        case 7: {
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

int main(int argc, char* argv[]) {
    if (argc == 4 && std::string(argv[1]) == "-b") {
        return runBatch(argv[2], argv[3]);
    }

    if (argc >= 2 && std::string(argv[1]) == "-b") {
        std::cerr << "ERROR: Invalid batch mode usage.\n";
        std::cerr << "Usage: ./myProg -b input.csv output.csv\n";
        return 1;
    }

    if (argc == 2) {
        return runInteractive(argv[1]);
    }

    if (argc > 2) {
        std::cerr << "ERROR: Invalid usage.\n";
        std::cerr << "Usage:\n";
        std::cerr << "  ./myProg\n";
        std::cerr << "  ./myProg input.csv\n";
        std::cerr << "  ./myProg -b input.csv output.csv\n";
        return 1;
    }

    return runInteractive();
}