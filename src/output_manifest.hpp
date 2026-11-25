#ifndef OUTPUT_MANIFEST_HPP
#define OUTPUT_MANIFEST_HPP

#include <cstddef>
#include <string>
#include <vector>

// Everything that pcb2gcode writes is recorded here so that --manifest can
// list the files for GUIs and scripts.  Paths are as given to the stream that
// wrote the file and lengths are in inches.  It is safe to add files from
// several threads.
namespace output_manifest {

struct Tool {
  int number; // The T word in the G-code.
  double diameter;
};

void add_gcode(const std::string& path, const std::string& layer,
               const std::vector<Tool>& tools);
void add_svg(const std::string& path, const std::string& layer,
             const std::string& stage);
// An image of the toolpaths of one tool.  is_final is for the toolpaths after
// post-processing, which are the ones in the G-code.
void add_svg(const std::string& path, const std::string& layer,
             const std::string& stage, bool is_final, size_t tool_index,
             double tool_diameter);

// The manifest that would be written to manifest_path, as JSON.
std::string to_json(const std::string& manifest_path);
void write(const std::string& manifest_path);
void clear();

} // namespace output_manifest

#endif // OUTPUT_MANIFEST_HPP
