#include "common.hxx"

#include <boost/filesystem.hpp>

TEST_CASE("outer_limits")
{
  Test_Util::Test_Case_Runner runner("outer_limits");

  boost::filesystem::path data_dir = runner.data_dir;
  boost::filesystem::path output_dir = runner.output_dir;

  Test_Util::Named_Args_Map args{
    {"--functions", (data_dir / "toy_functions.json").string()},
    {"--out", (output_dir / "toy_functions_out.json").string()},
    {"--checkpointDir", (output_dir / "ck").string()},
    {"--points", (data_dir / "toy_functions_points.json").string()},
    {"--precision", "128"},
    {"--dualityGapThreshold", "1e-10"},
    {"--primalErrorThreshold", "1e-10"},
    {"--dualErrorThreshold", "1e-10"},
    {"--initialMatrixScalePrimal", "1e1"},
    {"--initialMatrixScaleDual", "1e1"},
    {"--maxIterations", "1000"},
    {"--verbosity", "1"},
  };

  int res = runner.create_nested("run").mpi_run({"build/outer_limits"}, args);
  REQUIRE(res == 0);

  auto out = output_dir / "toy_functions_out.json";
  auto out_orig = data_dir / "toy_functions_out_orig.json";
  int diff_res = runner.create_nested("diff").diff(out, out_orig);
  REQUIRE(diff_res == 0);
}
