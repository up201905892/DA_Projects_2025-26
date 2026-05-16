/**
 * @file main.cpp
 * @brief Entry point for the Compiler Register Allocation Tool.
 *
 * Supports two execution modes:
 *  - Batch mode:       myProg -b ranges.txt registers.txt allocation.txt
 *  - Interactive mode: myProg  (launches menu-driven UI)
 *
 * Error messages are written to stderr; all other output to stdout or file.
 */

#include "Parser.h"
#include "Graph.h"
#include "Allocator.h"

#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <cstdlib>

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------

static void runBatch(const std::string& rangesFile,
                     const std::string& configFile,
                     const std::string& outputFile);

static void runInteractive();

// ---------------------------------------------------------------------------
// Batch mode helpers
// ---------------------------------------------------------------------------

/**
 * @brief Execute the full allocation pipeline once using file paths.
 *
 * Pipeline:
 *  1. Parse live ranges and configuration files.
 *  2. Build live webs.
 *  3. Build the interference graph.
 *  4. Run the selected allocation algorithm.
 *  5. Write the output file.
 *
 * @param rangesFile Path to the live ranges input file.
 * @param configFile Path to the registers/algorithm configuration file.
 * @param outputFile Path to the allocation output file.
 *
 * @complexity O(E^2 * L + W^2), dominated by web construction and graph coloring,
 *             where E is the number of live range entries, L is the average
 *             number of points per range, and W is the number of webs.
 */
static void runBatch(const std::string& rangesFile,
                     const std::string& configFile,
                     const std::string& outputFile) {
    try {
        // 1. Parse input files.
        std::vector<RangeEntry> entries = Parser::parseRanges(rangesFile);
        AllocConfig config = Parser::parseConfig(configFile);

        // 2. Build webs and interference graph.
        std::vector<Web> webs = buildWebs(entries);
        Graph graph = buildInterferenceGraph(webs);

        // 3. Allocate registers.
        Allocator alloc;
        AllocationResult result = alloc.allocate(webs, graph, config);

        // 4. Write output.
        writeOutput(outputFile, webs, result, config.numRegisters);

        if (result.success) {
            std::cout << "Allocation succeeded. Output written to: "
                      << outputFile << '\n';
        } else if (result.registersUsed == 0 &&
                   std::all_of(result.colors.begin(), result.colors.end(),
                               [](int c) { return c == SPILL_COLOR; })) {
            std::cerr << "Allocation failed. No valid assignment was possible "
                      << "with the provided register limit. Output written to: "
                      << outputFile << '\n';
        } else {
            std::cerr << "Allocation completed with memory assignments. "
                      << "Output written to: " << outputFile << '\n';
        }

    } catch (const std::exception& ex) {
        std::cerr << "[FATAL] " << ex.what() << '\n';
        std::exit(EXIT_FAILURE);
    }
}

// ---------------------------------------------------------------------------
// Interactive mode helpers
// ---------------------------------------------------------------------------

/**
 * @brief Print the interactive menu.
 *
 * @complexity O(1).
 */
static void printMenu() {
    std::cout << "\n=== Register Allocator ===\n"
              << "  1) Load ranges file\n"
              << "  2) Load config file\n"
              << "  3) Show webs\n"
              << "  4) Show interference graph\n"
              << "  5) Run allocation\n"
              << "  6) Save output to file\n"
              << "  0) Exit\n"
              << "Choice: ";
}

/**
 * @brief Pretty-print the interference graph as an adjacency list.
 *
 * @param webs Vector of webs.
 * @param graph Interference graph.
 *
 * @complexity O(W^2), where W is the number of webs.
 */
static void printGraph(const std::vector<Web>& webs, const Graph& graph) {
    if (webs.empty()) {
        std::cout << "(no webs loaded)\n";
        return;
    }

    std::cout << "Interference Graph (" << webs.size() << " webs):\n";

    for (int i = 0; i < static_cast<int>(webs.size()); ++i) {
        std::cout << "  web" << i << " [" << webs[i].varName << "]: ";

        bool first = true;
        for (int nb : graph.neighbors(i)) {
            if (!first) std::cout << ", ";
            std::cout << "web" << nb;
            first = false;
        }

        if (first)
            std::cout << "(no interferences)";

        std::cout << '\n';
    }
}

/**
 * @brief Print an allocation result to the console.
 *
 * This function mirrors the output-file format. It reports the number of
 * registers actually used, not just the number of registers available.
 *
 * @param webs Vector of webs.
 * @param result Allocation result.
 * @param numRegisters Number of available registers. Kept for interface
 *                     compatibility, but result.registersUsed is printed.
 *
 * @complexity O(W * R), where W is the number of webs and R is the number of
 *             registers used.
 */
static void printResult(const std::vector<Web>& webs,
                        const AllocationResult& result,
                        int numRegisters) {
    (void)numRegisters;

    std::cout << "\n--- Allocation Result ---\n";
    std::cout << "webs: " << webs.size() << '\n';

    for (int i = 0; i < static_cast<int>(webs.size()); ++i)
        std::cout << "web" << i << ": " << webs[i].toString() << '\n';

    // Complete failure: all webs assigned to memory.
    if (!result.success &&
        (result.registersUsed == 0 ||
         std::all_of(result.colors.begin(), result.colors.end(),
                     [](int c) { return c == SPILL_COLOR; }))) {
        std::cout << "registers: 0\n";

        for (int i = 0; i < static_cast<int>(webs.size()); ++i)
            std::cout << "M: web" << i << '\n';

        return;
    }

    std::cout << "registers: " << result.registersUsed << '\n';

    for (int c = 0; c < result.registersUsed; ++c) {
        for (int i = 0; i < static_cast<int>(webs.size()); ++i) {
            if (result.colors[i] == c)
                std::cout << 'r' << c << ": web" << i << '\n';
        }
    }

    for (int i = 0; i < static_cast<int>(webs.size()); ++i) {
        if (result.colors[i] == SPILL_COLOR)
            std::cout << "M: web" << i << '\n';
    }
}

// ---------------------------------------------------------------------------
// runInteractive
// ---------------------------------------------------------------------------

/**
 * @brief Run the interactive menu-driven user interface.
 *
 * The user may load input files, inspect webs, inspect the interference graph,
 * run allocation, and save the result to a file.
 *
 * @complexity Depends on the selected menu operations.
 */
static void runInteractive() {
    std::vector<RangeEntry> entries;
    AllocConfig config;
    std::vector<Web> webs;
    Graph graph(0);
    AllocationResult result;

    bool hasWebs = false;
    bool hasConfig = false;
    bool hasResult = false;

    while (true) {
        printMenu();

        int choice = -1;
        std::cin >> choice;

        if (std::cin.fail()) {
            std::cin.clear();
            std::cin.ignore(10000, '\n');
            std::cout << "Invalid option.\n";
            continue;
        }

        std::cin.ignore(10000, '\n');

        switch (choice) {
            case 0:
                std::cout << "Goodbye.\n";
                return;

            case 1: {
                std::cout << "Ranges file path: ";

                std::string path;
                std::getline(std::cin, path);

                try {
                    entries = Parser::parseRanges(path);
                    webs = buildWebs(entries);
                    graph = buildInterferenceGraph(webs);

                    hasWebs = true;
                    hasResult = false;

                    std::cout << "Loaded " << webs.size() << " web(s).\n";
                } catch (const std::exception& ex) {
                    std::cerr << "[ERROR] " << ex.what() << '\n';
                }

                break;
            }

            case 2: {
                std::cout << "Config file path: ";

                std::string path;
                std::getline(std::cin, path);

                try {
                    config = Parser::parseConfig(path);
                    hasConfig = true;
                    hasResult = false;

                    std::cout << "Config loaded: "
                              << config.numRegisters
                              << " register(s), algorithm=";

                    switch (config.type) {
                        case AlgorithmType::BASIC:
                            std::cout << "basic";
                            break;

                        case AlgorithmType::SPILLING:
                            std::cout << "spilling(K=" << config.K << ')';
                            break;

                        case AlgorithmType::SPLITTING:
                            std::cout << "splitting(K=" << config.K << ')';
                            break;

                        case AlgorithmType::FREE:
                            std::cout << "free";
                            break;
                    }

                    std::cout << '\n';
                } catch (const std::exception& ex) {
                    std::cerr << "[ERROR] " << ex.what() << '\n';
                }

                break;
            }

            case 3: {
                if (!hasWebs) {
                    std::cout << "No ranges loaded. Use option 1 first.\n";
                    break;
                }

                std::cout << "Webs (" << webs.size() << "):\n";

                for (const auto& w : webs) {
                    std::cout << "  web" << w.id << " [" << w.varName
                              << "]: " << w.toString() << '\n';
                }

                break;
            }

            case 4: {
                if (!hasWebs) {
                    std::cout << "No ranges loaded. Use option 1 first.\n";
                    break;
                }

                printGraph(webs, graph);
                break;
            }

            case 5: {
                if (!hasWebs) {
                    std::cout << "No ranges loaded.\n";
                    break;
                }

                if (!hasConfig) {
                    std::cout << "No config loaded.\n";
                    break;
                }

                try {
                    Allocator alloc;
                    result = alloc.allocate(webs, graph, config);
                    hasResult = true;

                    printResult(webs, result, config.numRegisters);
                } catch (const std::exception& ex) {
                    std::cerr << "[ERROR] " << ex.what() << '\n';
                }

                break;
            }

            case 6: {
                if (!hasResult) {
                    std::cout << "No result yet. Run allocation first (option 5).\n";
                    break;
                }

                std::cout << "Output file path: ";

                std::string path;
                std::getline(std::cin, path);

                try {
                    writeOutput(path, webs, result, config.numRegisters);
                    std::cout << "Written to: " << path << '\n';
                } catch (const std::exception& ex) {
                    std::cerr << "[ERROR] " << ex.what() << '\n';
                }

                break;
            }

            default:
                std::cout << "Unknown option.\n";
                break;
        }
    }
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

/**
 * @brief Program entry point.
 *
 * Batch usage:
 * @code
 * myProg -b ranges.txt registers.txt allocation.txt
 * @endcode
 *
 * Interactive usage:
 * @code
 * myProg
 * @endcode
 *
 * @param argc Number of command-line arguments.
 * @param argv Command-line argument values.
 * @return EXIT_SUCCESS on success, EXIT_FAILURE on incorrect usage or fatal error.
 *
 * @complexity Depends on selected execution mode.
 */
int main(int argc, char* argv[]) {
    if (argc == 5 && std::string(argv[1]) == "-b") {
        runBatch(argv[2], argv[3], argv[4]);
        return EXIT_SUCCESS;
    }

    if (argc == 1) {
        runInteractive();
        return EXIT_SUCCESS;
    }

    std::cerr << "Usage:\n"
              << "  " << argv[0]
              << " -b ranges.txt registers.txt allocation.txt\n"
              << "  " << argv[0]
              << "  (interactive mode)\n";

    return EXIT_FAILURE;
}