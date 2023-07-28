#include "common.hxx"

#include <boost/filesystem.hpp>

TEST_CASE("spectrum")
{
  Test_Util::Test_Case_Runner runner("spectrum");

  boost::filesystem::path data_dir = runner.data_dir;
  boost::filesystem::path output_dir = runner.output_dir;

  Test_Util::Named_Args_Map args{
    {"--input", (data_dir / "pvm.xml").string()},
    {"--solution", (data_dir / "solution").string()},
    {"--output", (output_dir / "spectrum.json").string()},
    {"--precision", "1024"},
    {"--threshold", "1e-10"},
    {"--format", "PVM"}};

  int res = runner.create_nested("run").mpi_run({"build/spectrum"}, args);
  REQUIRE(res == 0);

  auto out = output_dir / "spectrum.json";
  auto out_orig = data_dir / "spectrum_orig.json";
  int diff_res = runner.create_nested("diff").diff(out, out_orig);
  REQUIRE(diff_res == 0);
}
