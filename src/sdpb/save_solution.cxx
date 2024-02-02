#include "sdp_solve/sdp_solve.hxx"
#include "sdpb_util/ostream/set_stream_precision.hxx"
#include "sdpb_util/write_distmatrix.hxx"

namespace fs = std::filesystem;

namespace
{
  void write_psd_block(const fs::path &outfile,
                       const El::DistMatrix<El::BigFloat> &block)
  {
    std::ofstream stream;
    if(block.DistRank() == block.Root())
      {
        stream.open(outfile);
      }
    El::Print(block,
              std::to_string(block.Height()) + " "
                + std::to_string(block.Width()),
              "\n", stream);
    if(block.DistRank() == block.Root())
      {
        stream << "\n";
        ASSERT(stream.good(), "Error when writing to: ", outfile);
      }
  }
}

void save_solution(const SDP_Solver &solver,
                   const SDP_Solver_Terminate_Reason &terminate_reason,
                   const int64_t &solver_runtime,
                   const fs::path &out_directory,
                   const Write_Solution &write_solution,
                   const std::vector<size_t> &block_indices,
                   const Verbosity &verbosity)
{
  // Internally, El::Print() sync's everything to the root core and
  // outputs it from there.  So do not actually open the file on
  // anything but the root node.

  std::ofstream out_stream;
  if(El::mpi::Rank() == 0)
    {
      if(verbosity >= Verbosity::regular)
        {
          std::cout << "Saving solution to      : " << out_directory << '\n';
        }
      fs::create_directories(out_directory);
      const fs::path output_path(out_directory / "out.txt");
      out_stream.open(output_path);
      set_stream_precision(out_stream);
      out_stream << "terminateReason = \"" << terminate_reason << "\";\n"
                 << "primalObjective = " << solver.primal_objective << ";\n"
                 << "dualObjective   = " << solver.dual_objective << ";\n"
                 << "dualityGap      = " << solver.duality_gap << ";\n"
                 << "primalError     = " << solver.primal_error() << ";\n"
                 << "dualError       = " << solver.dual_error << ";\n"
                 << "Solver runtime  = " << solver_runtime << ";\n";
      ASSERT(out_stream.good(), "Error when writing to: ", output_path);
    }
  if(write_solution.vector_y)
    {
      // y is duplicated among blocks, so only need to print out copy
      // from the first block of rank 0.
      const auto &y_dist = solver.y.blocks.at(0);
      if(y_dist.Root() == 0)
        {
          // Copy from all ranks owning a block to rank zero
          El::DistMatrix<El::BigFloat, El::CIRC, El::CIRC> y_circ(y_dist);
          ASSERT(y_circ.Root() == 0);
          if(El::mpi::Rank() == 0)
            {
              // local matrix
              const El::Matrix<El::BigFloat> &y = y_circ.Matrix();
              ASSERT(y.Height() == y_dist.Height(), "y.Height()=", y.Height(),
                     " y_dist.Height()=", y_dist.Height());
              ASSERT(y.Width() == 1);
              const fs::path y_path(out_directory / "y.txt");
              std::ofstream y_stream(y_path);
              auto title = El::BuildString(y.Height(), " ", y.Width());
              El::Print(y, title, "\n", y_stream);
              y_stream << "\n";
              ASSERT(y_stream.good(), "Error when writing to: ", y_path);
            }
        }
    }

  for(size_t block = 0; block != solver.x.blocks.size(); ++block)
    {
      size_t block_index(block_indices.at(block));
      if(write_solution.vector_x)
        {
          write_distmatrix(solver.x.blocks.at(block),
                           out_directory
                             / ("x_" + std::to_string(block_index) + ".txt"));
        }
      for(size_t psd_block(0); psd_block < 2; ++psd_block)
        {
          std::string suffix(std::to_string(2 * block_index + psd_block)
                             + ".txt");

          if(write_solution.matrix_X
             && solver.X.blocks.at(2 * block + psd_block).Height() != 0)
            {
              write_psd_block(out_directory / ("X_matrix_" + suffix),
                              solver.X.blocks.at(2 * block + psd_block));
            }
          if(write_solution.matrix_Y
             && solver.Y.blocks.at(2 * block + psd_block).Height() != 0)
            {
              write_psd_block(out_directory / ("Y_matrix_" + suffix),
                              solver.Y.blocks.at(2 * block + psd_block));
            }
        }
    }
}
