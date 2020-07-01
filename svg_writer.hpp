#ifndef SVG_WRITER_HPP
#define SVG_WRITER_HPP

#include <fstream>

class svg_writer {
 public:
  svg_writer(std::string filename, box_type_fp bounding_box);
  template <typename multi_polygon_type_t>
      void add(const multi_polygon_type_t& geometry, double opacity, bool stroke);
  void add(const multi_linestring_type_fp& mls, coordinate_type_fp width, bool stroke);
  void add(const std::vector<polygon_type_fp>& geometries, double opacity,
           int r = -1, int g = -1, int b = -1);
  void add(const linestring_type_fp& paths, coordinate_type_fp width, unsigned int r, unsigned int g, unsigned int b);
  void add(const multi_linestring_type_fp& paths, coordinate_type_fp width, unsigned int r, unsigned int g, unsigned int b);

 protected:
  std::ofstream output_file;
  const box_type_fp bounding_box;
  std::unique_ptr<bg::svg_mapper<point_type_fp> > mapper;
};

#endif //SVG_WRITER_HPP
