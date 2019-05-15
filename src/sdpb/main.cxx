//=======================================================================
// Copyright 2014-2015 David Simmons-Duffin.
// Distributed under the MIT License.
// (See accompanying file LICENSE or copy at
//  http://opensource.org/licenses/MIT)
//=======================================================================

#include "SDP_Solver_Parameters.hxx"
#include "Block_Info.hxx"
#include "../Timers.hxx"

#include <El.hpp>

#include <boost/date_time/posix_time/posix_time.hpp>

Timers
solve(const Block_Info &block_info, const SDP_Solver_Parameters &parameters);

void write_timing(const boost::filesystem::path &checkpoint_out,
                  const Block_Info &block_info, const Timers &timers,
                  const bool &debug);

int main(int argc, char **argv)
{
  El::Environment env(argc, argv);

  try
    {
      SDP_Solver_Parameters parameters(argc, argv);
      if(!parameters.is_valid())
        {
          return 0;
        }

      El::gmp::SetPrecision(parameters.precision);
      if(parameters.verbosity >= Verbosity::regular && El::mpi::Rank() == 0)
        {
          std::cout << "SDPB started at "
                    << boost::posix_time::second_clock::local_time() << '\n'
                    << parameters << '\n';
        }

      Block_Info block_info(parameters.sdp_directory, parameters.checkpoint_in,
                            parameters.procs_per_node,
                            parameters.proc_granularity, parameters.verbosity);
      // Only generate a block_timings file if
      // 1) We are running in parallel
      // 2) We did not load a block_timings file
      // 3) We are not going to load a checkpoint.
      if(El::mpi::Size(El::mpi::COMM_WORLD) > 1
         && block_info.block_timings_filename.empty()
         && !exists(parameters.checkpoint_in
                    / ("checkpoint." + std::to_string(El::mpi::Rank()))))
        {
          if(parameters.verbosity >= Verbosity::regular
             && El::mpi::Rank() == 0)
            {
              std::cout << "Performing a timing run\n";
            }
          SDP_Solver_Parameters timing_parameters(parameters);
          timing_parameters.max_iterations = 2;
          timing_parameters.no_final_checkpoint = true;
          timing_parameters.checkpoint_interval
            = std::numeric_limits<int64_t>::max();
          timing_parameters.verbosity = Verbosity::none;
          Timers timers(solve(block_info, timing_parameters));

          write_timing(timing_parameters.checkpoint_out, block_info, timers,
                       timing_parameters.verbosity >= Verbosity::debug);
          El::mpi::Barrier(El::mpi::COMM_WORLD);
          block_info
            = Block_Info(parameters.sdp_directory, parameters.checkpoint_out,
                         parameters.procs_per_node,
                         parameters.proc_granularity, parameters.verbosity);

          parameters.max_runtime -= timers.front().second.elapsed_seconds();
        }
      else if(!block_info.block_timings_filename.empty()
              && block_info.block_timings_filename
                   != (parameters.checkpoint_out / "block_timings"))
        {
          if(El::mpi::Rank() == 0)
            {
              create_directories(parameters.checkpoint_out);
              copy_file(block_info.block_timings_filename,
                        parameters.checkpoint_out / "block_timings");
            }
        }
      solve(block_info, parameters);
    }
  catch(std::exception &e)
    {
      El::ReportException(e);
      El::mpi::Abort(El::mpi::COMM_WORLD, 1);
    }
  catch(...)
    {
      El::mpi::Abort(El::mpi::COMM_WORLD, 1);
    }
}
