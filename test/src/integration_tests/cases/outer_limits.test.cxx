#include "integration_tests/common.hxx"

#include <filesystem>

using namespace std::string_literals;
namespace fs = std::filesystem;

TEST_CASE("outer_limits")
{
  INFO("Simple pvm2functions + outer_limits test");
  // TODO test also sdp2functions

  int num_procs = GENERATE(1, 2, 6);
  DYNAMIC_SECTION("num_procs=" << num_procs)
  {
    Test_Util::Test_Case_Runner runner("outer_limits/mpirun-"
                                       + std::to_string(num_procs));
    fs::path data_dir = runner.data_dir.parent_path(); //test/data/outer_limits
    fs::path output_dir = runner.output_dir;

    fs::path functions_json = output_dir / "functions.json";
    fs::path functions_orig_json = data_dir / "functions_orig.json";
    int precision = 128;

    {
      INFO("run pvm2functions");
      fs::path pvm_xml = Test_Config::test_data_dir / "pvm2sdp" / "pvm.xml";

      // TODO allow running pvm2functions in parallel
      runner.create_nested("pvm2functions")
        .run({"build/pvm2functions", std::to_string(precision), pvm_xml,
              functions_json});
      Test_Util::REQUIRE_Equal::diff_functions_json(
        functions_json, functions_orig_json, precision, precision / 2);
    }

    auto out_json = output_dir / "out.json";
    Test_Util::Test_Case_Runner::Named_Args_Map args{
      {"--functions", functions_json},
      {"--out", out_json},
      {"--checkpointDir", (output_dir / "ck").string()},
      {"--points", (data_dir / "points.json").string()},
      {"--precision", std::to_string(precision)},
      {"--dualityGapThreshold", "1e-10"},
      {"--primalErrorThreshold", "1e-10"},
      {"--dualErrorThreshold", "1e-10"},
      {"--initialMatrixScalePrimal", "1e1"},
      {"--initialMatrixScaleDual", "1e1"},
      {"--maxIterations", "1000"},
      {"--verbosity", "1"},
    };

    {
      INFO("run outer_limits");
      runner.create_nested("outer_limits")
        .mpi_run({"build/outer_limits"}, args, num_procs);

      auto out_orig_json = data_dir / "out_orig.json";
      Test_Util::REQUIRE_Equal::diff_outer_limits(out_json, out_orig_json,
                                                  precision, precision / 2);
    }
  }
}