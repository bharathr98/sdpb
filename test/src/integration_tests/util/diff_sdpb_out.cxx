#include "diff.hxx"

#include "Float.hxx"

#include <catch2/catch_amalgamated.hpp>

#include <boost/algorithm/algorithm.hpp>
#include <boost/algorithm/string.hpp>

// Parser classes
namespace
{
  struct Parse_Matrix_Txt
  {
    int height;
    int width;
    Float_Vector elements;
    Parse_Matrix_Txt(const boost::filesystem::path &path,
                     unsigned int binary_precision)
    {
      Float_Binary_Precision _(binary_precision);

      CAPTURE(path);
      REQUIRE(is_regular_file(path));
      boost::filesystem::ifstream is(path);
      height = width = 0;
      is >> height;
      is >> width;
      REQUIRE(height > 0);
      REQUIRE(width > 0);
      elements.reserve(height * width);
      for(int i = 0; i < height; ++i)
        {
          for(int k = 0; k < width; ++k)
            {
              Float f;
              is >> f;
              elements.push_back(f);
            }
        }
    }
  };

  // out.txt produced by SDPB
  struct Parse_Sdpb_Out_Txt : boost::noncopyable
  {
    std::string terminate_reason;
    std::map<std::string, Float> float_map;

    Parse_Sdpb_Out_Txt(const boost::filesystem::path &path,
                       unsigned int binary_precision)
    {
      Float_Binary_Precision _(binary_precision);

      CAPTURE(path);
      REQUIRE(is_regular_file(path));
      boost::filesystem::ifstream is(path);
      std::string line;
      while(std::getline(is, line))
        {
          // line format:
          // key = value;
          CAPTURE(line);
          std::vector<std::string> tokens;
          boost::split(tokens, line, boost::is_any_of("=;"));
          if(tokens.size() <= 1)
            break; // empty string (the last one)

          REQUIRE(tokens.size() == 3); // key, value, empty "" at the end
          auto key = tokens[0];
          auto value = tokens[1];
          boost::trim(key);
          boost::trim(value);
          if(key == "terminateReason")
            {
              terminate_reason = value;
            }
          else
            {
              float_map[key] = Float(value);
            }
        }
      REQUIRE(!terminate_reason.empty());
    }
  };
}

// Helper functions
namespace
{
  using Test_Util::REQUIRE_Equal::diff;
  // Compare matrices written by SDPB save_solution() method,
  // e.g. y.txt or X.txt
  void diff_matrix_txt(const boost::filesystem::path &a_matrix_txt,
                       const boost::filesystem::path &b_matrix_txt,
                       unsigned int binary_precision)
  {
    CAPTURE(a_matrix_txt);
    CAPTURE(b_matrix_txt);
    Parse_Matrix_Txt a(a_matrix_txt, binary_precision);
    Parse_Matrix_Txt b(b_matrix_txt, binary_precision);
    diff(a.height, b.height);
    diff(a.width, b.width);
    diff(a.elements, b.elements);
  }

  // Compare out.txt
  void diff_sdpb_out_txt(const boost::filesystem::path &a_out_txt,
                         const boost::filesystem::path &b_out_txt,
                         unsigned int binary_precision,
                         const std::vector<std::string> &keys_to_compare)
  {
    CAPTURE(a_out_txt);
    CAPTURE(b_out_txt);
    Parse_Sdpb_Out_Txt a(a_out_txt, binary_precision);
    Parse_Sdpb_Out_Txt b(a_out_txt, binary_precision);

    auto keys = keys_to_compare;
    // By default, test each key except for "Solver runtime"
    if(keys.empty())
      keys = {"terminateReason", "primalObjective", "dualObjective",
              "dualityGap",      "primalError",     "dualError"};
    CAPTURE(keys);

    for(const auto &key : keys)
      {
        CAPTURE(key);
        if(key == "terminateReason")
          {
            REQUIRE(a.terminate_reason == b.terminate_reason);
            continue;
          }
        if(key == "Solver runtime")
          WARN("Solver runtime may differ for different runs, "
               "do you really want to check it?");

        auto &a_map = a.float_map;
        auto &b_map = b.float_map;
        REQUIRE(a_map.find(key) != a_map.end());
        REQUIRE(b_map.find(key) != b_map.end());

        diff(a_map[key], b_map[key]);
      }
  }
}

// Implementation
namespace Test_Util::REQUIRE_Equal
{
  void diff_sdpb_output_dir(const boost::filesystem::path &a_out_dir,
                            const boost::filesystem::path &b_out_dir,
                            unsigned int binary_precision,
                            const std::vector<std::string> &filenames,
                            const std::vector<std::string> &out_txt_keys)
  {
    CAPTURE(a_out_dir);
    CAPTURE(b_out_dir);
    REQUIRE(is_directory(a_out_dir));
    REQUIRE(is_directory(b_out_dir));

    std::vector<std::string> my_filenames = filenames;
    if(my_filenames.empty())
      {
        boost::filesystem::directory_iterator a_it(a_out_dir);
        boost::filesystem::directory_iterator b_it(b_out_dir);
        boost::filesystem::directory_iterator end{};
        for(const auto &a : boost::filesystem::directory_iterator(a_out_dir))
          {
            CAPTURE(a);
            REQUIRE(boost::filesystem::is_regular_file(a));
            my_filenames.push_back(a.path().filename().string());
          }
        // Check that all files from b_out_dir exist in a_out_dir
        for(const auto &b : boost::filesystem::directory_iterator(b_out_dir))
          {
            CAPTURE(b);
            REQUIRE(is_regular_file(a_out_dir / b.path().filename()));
          }
      }

    for(const auto &name : my_filenames)
      {
        auto a = a_out_dir / name;
        auto b = b_out_dir / name;

        if(name == "out.txt")
          diff_sdpb_out_txt(a, b, binary_precision, out_txt_keys);
        else
          diff_matrix_txt(a, b, binary_precision);
      }
  }
}
