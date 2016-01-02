#include "tools/generate_profiles.hpp"

#include <experimental/filesystem>  // NOLINT
#include <fstream>

#include "glog/logging.h"
#include "google/protobuf/descriptor.h"
#include "serialization/journal.pb.h"
#include "tools/journal_proto_processor.hpp"

namespace principia {
namespace tools {

namespace {
const char kWarning[] = "// Warning!  This file was generated by running a "
                        "program (see project |tools|).\n"
                        "// If you change it, the changes will be lost the "
                        "next time the generator is\n"
                        "// run.  You should change the generator instead.\n\n";
}  // namespace

void GenerateProfiles() {
  JournalProtoProcessor processor;
  processor.ProcessMessages();

  // Now write the output.
  std::experimental::filesystem::path const directory =
      SOLUTION_DIR / "journal";
  std::ofstream profiles_generated_h(directory / "profiles.generated.h");
  CHECK(profiles_generated_h.good());
  profiles_generated_h << kWarning;
  for (auto const& cpp_method_type : processor.GetCppMethodTypes()) {
    profiles_generated_h << cpp_method_type;
  }

  std::ofstream profiles_generated_cc(directory / "profiles.generated.cc");
  CHECK(profiles_generated_cc.good());
  profiles_generated_cc << kWarning;
  for (auto const& cpp_interchange_implementation :
          processor.GetCppInterchangeImplementations()) {
    profiles_generated_cc << cpp_interchange_implementation;
  }
  for (auto const& cpp_method_implementation :
          processor.GetCppMethodImplementations()) {
    profiles_generated_cc << cpp_method_implementation;
  }
}

}  // namespace tools
}  // namespace principia
