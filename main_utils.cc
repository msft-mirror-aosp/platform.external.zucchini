// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/zucchini/main_utils.h"

#include <stddef.h>

#include <memory>
#include <ostream>
#include <vector>

#include "base/command_line.h"
#include "base/logging.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "components/zucchini/io_utils.h"
#include "components/zucchini/zucchini_commands.h"

#if defined(OS_WIN)
#include <windows.h>  // This include must come first.

#include <psapi.h>
#endif

namespace {

#if defined(OS_WIN)
#endif

/******** Command ********/

// Specifications for a Zucchini command.
struct Command {
  constexpr Command(const char* name_in,
                    const char* usage_in,
                    int num_args_in,
                    CommandFunction command_function_in)
      : name(name_in),
        usage(usage_in),
        num_args(num_args_in),
        command_function(command_function_in) {}
  Command(const Command&) = default;
  ~Command() = default;

  // Unique name of command. |-name| is used to select from command-line.
  const char* const name;

  // Usage help text of command.
  const char* const usage;

  // Number of arguments (assumed to be filenames) used by the command.
  const int num_args;

  // Main function to run for the command.
  const CommandFunction command_function;
};

/******** List of Zucchini commands ********/

constexpr Command kCommands[] = {
    {"gen", "-gen <old_file> <new_file> <patch_file> [-raw]", 3, &MainGen},
    {"apply", "-apply <old_file> <patch_file> <new_file>", 3, &MainApply},
    {"read", "-read <exe> [-dump]", 1, &MainRead},
    {"detect", "-detect <archive_file> [-dd=format#]", 1, &MainDetect},
    {"match", "-match <old_file> <new_file>", 2, &MainMatch},
    {"crc32", "-crc32 <file>", 1, &MainCrc32},
};

/******** ScopedResourceUsageTracker ********/

// A class to track and log system resource usage.
class ScopedResourceUsageTracker {
 public:
  // Initializes states for tracking.
  ScopedResourceUsageTracker() {
    start_time_ = base::TimeTicks::Now();

#if defined(OS_WIN)
    PROCESS_MEMORY_COUNTERS pmc;
    if (::GetProcessMemoryInfo(::GetCurrentProcess(), &pmc, sizeof(pmc))) {
      start_peak_page_file_usage_ = pmc.PeakPagefileUsage;
      start_peak_working_set_size_ = pmc.PeakWorkingSetSize;
    }
#endif
  }

  // Computes and prints usage.
  ~ScopedResourceUsageTracker() {
    base::TimeTicks end_time = base::TimeTicks::Now();

#if defined(OS_WIN)
    size_t cur_peak_page_file_usage = 0;
    size_t cur_peak_working_set_size = 0;
    PROCESS_MEMORY_COUNTERS pmc;
    if (::GetProcessMemoryInfo(::GetCurrentProcess(), &pmc, sizeof(pmc))) {
      cur_peak_page_file_usage = pmc.PeakPagefileUsage;
      cur_peak_working_set_size = pmc.PeakWorkingSetSize;
    }

    LOG(INFO) << "Zucchini.PeakPagefileUsage "
              << cur_peak_page_file_usage / 1024 << " KiB";
    LOG(INFO) << "Zucchini.PeakPagefileUsageChange "
              << (cur_peak_page_file_usage - start_peak_page_file_usage_) / 1024
              << " KiB";
    LOG(INFO) << "Zucchini.PeakWorkingSetSize "
              << cur_peak_working_set_size / 1024 << " KiB";
    LOG(INFO) << "Zucchini.PeakWorkingSetSizeChange "
              << (cur_peak_working_set_size - start_peak_working_set_size_) /
                     1024
              << " KiB";
#endif  // !defined(OS_MACOSX)

    LOG(INFO) << "Zucchini.TotalTime " << (end_time - start_time_).InSecondsF()
              << " s";
  }

 private:
  base::TimeTicks start_time_;
#if defined(OS_WIN)
  size_t start_peak_page_file_usage_ = 0;
  size_t start_peak_working_set_size_ = 0;
#endif  // !defined(OS_MACOSX)
};

/******** Helper functions ********/

// Translates |command_line| arguments to a vector of base::FilePath (expecting
// exactly |expected_count|). On success, writes the results to |paths| and
// returns true. Otherwise returns false.
bool CheckAndGetFilePathParams(const base::CommandLine& command_line,
                               size_t expected_count,
                               std::vector<base::FilePath>* paths) {
  const base::CommandLine::StringVector& args = command_line.GetArgs();
  if (args.size() != expected_count)
    return false;

  paths->clear();
  paths->reserve(args.size());
  for (const auto& arg : args)
    paths->emplace_back(arg);
  return true;
}

// Prints main Zucchini usage text.
void PrintUsage(std::ostream& err) {
  err << "Usage:" << std::endl;
  for (const Command& command : kCommands)
    err << "  zucchini " << command.usage << std::endl;
}

}  // namespace

/******** Exported Functions ********/

zucchini::status::Code RunZucchiniCommand(const base::CommandLine& command_line,
                                          std::ostream& out,
                                          std::ostream& err) {
  // Look for a command with name that matches input.
  const Command* command_use = nullptr;
  for (const Command& command : kCommands) {
    if (command_line.HasSwitch(command.name)) {
      if (command_use) {        // Too many commands found.
        command_use = nullptr;  // Set to null to flag error.
        break;
      }
      command_use = &command;
    }
  }

  // Expect exactly 1 matching command. If 0 or >= 2, print usage and quit.
  if (!command_use) {
    err << "Must have exactly one of:" << std::endl;
    err << "  [";
    zucchini::PrefixSep sep(", ");
    for (const Command& command : kCommands)
      err << sep << "-" << command.name;
    err << "]" << std::endl;
    PrintUsage(err);
    return zucchini::status::kStatusInvalidParam;
  }

  // Try to parse filename arguments. On failure, print usage and quit.
  std::vector<base::FilePath> paths;
  if (!CheckAndGetFilePathParams(command_line, command_use->num_args, &paths)) {
    err << command_use->usage << std::endl;
    PrintUsage(err);
    return zucchini::status::kStatusInvalidParam;
  }

  ScopedResourceUsageTracker resource_usage_tracker;
  return command_use->command_function({command_line, paths, out, err});
}
