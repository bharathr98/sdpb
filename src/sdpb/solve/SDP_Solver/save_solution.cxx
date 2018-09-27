#include "../SDP_Solver.hxx"
#include "../../../set_stream_precision.hxx"

void SDP_Solver::save_solution(
  const SDP_Solver_Terminate_Reason terminate_reason,
  const boost::filesystem::path &out_file) const
{
  // El::Print requires all processors, but we do not want all processors
  // writing header information.
  // FIXME: Write separate files for each rank.

  boost::filesystem::ofstream ofs(out_file);
  set_stream_precision(ofs);
  if(El::mpi::Rank() == 0)
    {
      std::cout << "Saving solution to      : " << out_file << '\n';
      ofs << "terminateReason = \"" << terminate_reason << "\";\n"
          << "primalObjective = " << primal_objective << ";\n"
          << "dualObjective   = " << dual_objective << ";\n"
          << "dualityGap      = " << duality_gap << ";\n"
          << "primalError     = " << primal_error << ";\n"
          << "dualError       = " << dual_error << ";\n"
          << "y = {";
    }
  if(!y.blocks.empty())
    {
      El::Print(y.blocks.at(0), "", ofs);
    }

  if(El::mpi::Rank() == 0)
    {
      ofs << "};\nx = {";
    }

  for(auto &block : x.blocks)
    {
      El::Print(block, "", ofs);
    }

  if(El::mpi::Rank() == 0)
    {
      ofs << "};\n";
    }
}
