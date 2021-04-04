#include "../Outer_Parameters.hxx"
#include "setup_constraints.hxx"

#include "../../ostream_set.hxx"
#include "../../ostream_map.hxx"
#include "../../ostream_vector.hxx"
#include "../../set_stream_precision.hxx"

namespace
{
  void copy_matrix(const El::Matrix<El::BigFloat> &source,
                   El::DistMatrix<El::BigFloat> &destination)
  {
    for(int64_t row(0); row < destination.LocalHeight(); ++row)
      {
        int64_t global_row(destination.GlobalRow(row));
        for(int64_t column(0); column < destination.LocalWidth(); ++column)
          {
            int64_t global_column(destination.GlobalCol(column));
            destination.SetLocal(row, column,
                                 source(global_row, global_column));
          }
      }
  }

  void
  copy_matrix(const El::DistMatrix<El::BigFloat, El::STAR, El::STAR> &source,
              El::Matrix<El::BigFloat> &destination)
  {
    destination.Resize(source.LocalHeight(), source.LocalWidth());
    for(int64_t row(0); row < source.LocalHeight(); ++row)
      {
        for(int64_t column(0); column < source.LocalWidth(); ++column)
          {
            destination(row, column) = source.GetLocal(row, column);
          }
      }
  }

  void fill_weights(const El::Matrix<El::BigFloat> &y, const size_t &max_index,
                    const std::vector<El::BigFloat> &normalization,
                    std::vector<El::BigFloat> &weights)
  {
    weights.at(max_index) = 1;
    for(size_t block_row(0); block_row != size_t(y.Height()); ++block_row)
      {
        const size_t index(block_row + (block_row < max_index ? 0 : 1));
        weights.at(index) = y(block_row, 0);
        weights.at(max_index) -= weights.at(index) * normalization.at(index);
      }
    weights.at(max_index) /= normalization.at(max_index);
  }
}

void compute_y_transform(
  const std::vector<std::vector<std::vector<std::vector<Function>>>>
    &function_blocks,
  const std::vector<std::set<El::BigFloat>> &points,
  const std::vector<El::BigFloat> &objectives,
  const std::vector<El::BigFloat> &normalization,
  const Outer_Parameters &parameters, const size_t &max_index,
  const El::Grid &global_grid,
  El::DistMatrix<El::BigFloat, El::STAR, El::STAR> &yp_to_y,
  El::DistMatrix<El::BigFloat, El::STAR, El::STAR> &dual_objective_b_star,
  El::BigFloat &primal_c_scale);

boost::optional<int64_t> load_checkpoint(
  const boost::filesystem::path &checkpoint_directory,
  boost::optional<int64_t> &backup_generation, int64_t &current_generation,
  El::DistMatrix<El::BigFloat, El::STAR, El::STAR> &yp_to_y_star,
  El::DistMatrix<El::BigFloat, El::STAR, El::STAR> &dual_objective_b_star,
  El::Matrix<El::BigFloat> &y, std::vector<std::set<El::BigFloat>> &points,
  El::BigFloat &threshold, El::BigFloat &primal_c_scale);

void find_new_points(
  const size_t &num_blocks, const size_t &rank, const size_t &num_procs,
  const El::BigFloat &infinity,
  const std::vector<std::vector<std::vector<std::vector<Function>>>>
    &function_blocks,
  const std::vector<El::BigFloat> &weights,
  const std::vector<std::set<El::BigFloat>> &points,
  std::vector<std::vector<El::BigFloat>> &new_points, bool &has_new_points);

void save_checkpoint(
  const boost::filesystem::path &checkpoint_directory,
  const Verbosity &verbosity,
  const El::DistMatrix<El::BigFloat, El::STAR, El::STAR> &yp_to_y_star,
  const El::DistMatrix<El::BigFloat, El::STAR, El::STAR> &dual_objective_b_star,
  const El::Matrix<El::BigFloat> &y,
  const std::vector<std::set<El::BigFloat>> &points,
  const El::BigFloat &infinity, const El::BigFloat &threshold,
  const El::BigFloat &primal_c_scale,
  boost::optional<int64_t> &backup_generation, int64_t &current_generation);

std::vector<El::BigFloat> compute_optimal(
  const std::vector<std::vector<std::vector<std::vector<Function>>>>
    &function_blocks,
  const std::vector<std::vector<El::BigFloat>> &initial_points,
  const std::vector<El::BigFloat> &objectives,
  const std::vector<El::BigFloat> &normalization,
  const Outer_Parameters &parameters_in)
{
  if(initial_points.size() != function_blocks.size())
    {
      throw std::runtime_error(
        "Size are different: Positive_Matrix_With_Prefactor: "
        + std::to_string(function_blocks.size())
        + ", initial points: " + std::to_string(initial_points.size()));
    }
  Outer_Parameters parameters(parameters_in);

  const size_t rank(El::mpi::Rank()), num_procs(El::mpi::Size()),
    num_weights(normalization.size());

  const size_t num_blocks(initial_points.size());
  std::vector<El::BigFloat> weights(num_weights, 0);
  std::vector<std::set<El::BigFloat>> points(num_blocks);
  std::vector<std::vector<El::BigFloat>> new_points(num_blocks);

  // GMP does not have a special infinity value, so we use max double.
  const El::BigFloat infinity(std::numeric_limits<double>::max());
  // Use the input points and add inifinty
  for(size_t block(0); block < num_blocks; ++block)
    {
      for(auto &point : initial_points.at(block))
        {
          points.at(block).emplace(point);
        }
      points.at(block).emplace(infinity);
    }

  // TODO: This is duplicated from sdp2input/write_output/write_output.cxx
  size_t max_index(0);
  El::BigFloat max_normalization(0);
  for(size_t index(0); index != normalization.size(); ++index)
    {
      const El::BigFloat element(Abs(normalization[index]));
      if(element > max_normalization)
        {
          max_normalization = element;
          max_index = index;
        }
    }

  const El::Grid global_grid;
  El::DistMatrix<El::BigFloat, El::STAR, El::STAR> yp_to_y_star(global_grid),
    dual_objective_b_star(global_grid);
  El::BigFloat primal_c_scale;

  // TODO: Load checkpoint
  parameters.solver.duality_gap_threshold = 1.1;
  El::Matrix<El::BigFloat> yp_saved(yp_to_y_star.Height(), 1);
  El::Zero(yp_saved);

  int64_t current_generation(0);
  boost::optional<int64_t> backup_generation;
  load_checkpoint(parameters.solver.checkpoint_in, backup_generation,
                  current_generation, yp_to_y_star, dual_objective_b_star,
                  yp_saved, points, parameters.solver.duality_gap_threshold,
                  primal_c_scale);
  if(backup_generation)
    {
      El::Matrix<El::BigFloat> y(yp_saved.Height(), 1);

      El::Gemv(El::Orientation::NORMAL, El::BigFloat(1.0),
               yp_to_y_star.LockedMatrix(), yp_saved, El::BigFloat(0.0), y);

      if(El::mpi::Rank() == 0 && parameters.verbosity >= Verbosity::regular)
        {
          std::cout << "Loaded checkpoint " << backup_generation << "\n";
        }
      fill_weights(y, max_index, normalization, weights);
    }
  else
    {
      compute_y_transform(function_blocks, points, objectives, normalization,
                          parameters, max_index, global_grid, yp_to_y_star,
                          dual_objective_b_star, primal_c_scale);
    }

  while(parameters.solver.duality_gap_threshold
        > parameters_in.solver.duality_gap_threshold)
    {
      std::map<size_t, size_t> new_to_old;
      size_t num_constraints(0), old_index(0);
      std::vector<size_t> matrix_dimensions;
      for(size_t block(0); block != num_blocks; ++block)
        {
          for(size_t offset(0); offset != points.at(block).size(); ++offset)
            {
              new_to_old.emplace(num_constraints + offset, old_index + offset);
            }
          old_index += points.at(block).size();
          for(auto &point : new_points.at(block))
            {
              points.at(block).emplace(point);
            }
          num_constraints += points.at(block).size();
          matrix_dimensions.insert(matrix_dimensions.end(),
                                   points.at(block).size(),
                                   function_blocks[block].size());
          if(rank == 0 && parameters.verbosity >= Verbosity::debug)
            {
              std::cout << "points: " << block << " " << points.at(block)
                        << "\n";
            }
        }
      if(rank == 0 && parameters.verbosity >= Verbosity::regular)
        {
          std::cout << "num_constraints: " << num_constraints << "\n";
        }

      std::vector<std::vector<El::BigFloat>> primal_objective_c;
      primal_objective_c.reserve(num_constraints);
      std::vector<El::Matrix<El::BigFloat>> free_var_matrix;
      free_var_matrix.reserve(num_constraints);

      setup_constraints(max_index, num_blocks, infinity, function_blocks,
                        normalization, points, primal_objective_c,
                        free_var_matrix);

      const El::BigFloat objective_const(objectives.at(max_index)
                                         / normalization.at(max_index));

      Block_Info block_info(matrix_dimensions, parameters.verbosity);

      El::Grid grid(block_info.mpi_comm.value);

      SDP sdp(objective_const, primal_objective_c, free_var_matrix,
              yp_to_y_star, dual_objective_b_star, primal_c_scale, block_info,
              grid);

      SDP_Solver solver(parameters.solver, parameters.verbosity,
                        parameters.require_initial_checkpoint, block_info,
                        grid, sdp.dual_objective_b.Height());

      for(auto &y_block : solver.y.blocks)
        {
          copy_matrix(yp_saved, y_block);
        }

      boost::property_tree::ptree parameter_properties(
        to_property_tree(parameters));
      bool has_new_points(false);
      while(!has_new_points
            && parameters.solver.duality_gap_threshold
                 > parameters_in.solver.duality_gap_threshold)
        {
          if(rank == 0 && parameters.verbosity >= Verbosity::regular)
            {
              std::cout << "Threshold: "
                        << parameters.solver.duality_gap_threshold << "\n";
            }

          Timers timers(parameters.verbosity >= Verbosity::debug);
          SDP_Solver_Terminate_Reason reason
            = solver.run(parameters.solver, parameters.verbosity,
                         parameter_properties, block_info, sdp, grid, timers);

          if(rank == 0 && parameters.verbosity >= Verbosity::regular)
            {
              set_stream_precision(std::cout);
              std::cout << "-----" << reason << "-----\n"
                        << '\n'
                        << "primalObjective = " << solver.primal_objective
                        << '\n'
                        << "dualObjective   = " << solver.dual_objective
                        << '\n'
                        << "dualityGap      = " << solver.duality_gap << '\n'
                        << "primalError     = " << solver.primal_error()
                        << '\n'
                        << "dualError       = " << solver.dual_error << '\n'
                        << '\n';
            }

          if(reason == SDP_Solver_Terminate_Reason::MaxComplementarityExceeded
             || reason == SDP_Solver_Terminate_Reason::MaxIterationsExceeded
             || reason == SDP_Solver_Terminate_Reason::MaxRuntimeExceeded)
            {
              std::stringstream ss;
              ss << "Can not find solution: " << reason;
              throw std::runtime_error(ss.str());
            }

          // y is duplicated among cores, so only need to print out copy on
          // the root node.
          // THe weight at max_index is determined by the normalization
          // condition dot(norm,weights)=1
          El::DistMatrix<El::BigFloat> yp(dual_objective_b_star.Height(), 1,
                                          yp_to_y_star.Grid());
          El::Zero(yp);
          El::DistMatrix<El::BigFloat> y(yp);
          El::DistMatrix<El::BigFloat, El::STAR, El::STAR> yp_star(
            solver.y.blocks.at(0));
          for(int64_t row(0); row != yp.LocalHeight(); ++row)
            {
              int64_t global_row(yp.GlobalRow(row));
              for(int64_t column(0); column != yp.LocalWidth(); ++column)
                {
                  int64_t global_column(yp.GlobalCol(column));
                  yp.SetLocal(row, column,
                              yp_star.GetLocal(global_row, global_column));
                }
            }
          El::Gemv(El::Orientation::NORMAL, El::BigFloat(1.0), yp_to_y_star,
                   yp, El::BigFloat(0.0), y);
          El::DistMatrix<El::BigFloat, El::STAR, El::STAR> y_star(y);

          fill_weights(y_star.LockedMatrix(), max_index, normalization,
                       weights);
          if(rank == 0 && parameters.verbosity >= Verbosity::regular)
            {
              set_stream_precision(std::cout);
              std::cout << "weight: " << weights << "\n";

              El::BigFloat optimal(0);
              for(size_t index(0); index < objectives.size(); ++index)
                {
                  optimal += objectives[index] * weights[index];
                }
              std::cout << "optimal: " << optimal << "\n";
            }
          find_new_points(num_blocks, rank, num_procs, infinity,
                          function_blocks, weights, points, new_points,
                          has_new_points);
          if(!has_new_points)
            {
              parameters.solver.duality_gap_threshold
                /= parameters.duality_gap_reduction;
            }
        }
      El::DistMatrix<El::BigFloat, El::STAR, El::STAR> yp_star(
        solver.y.blocks.front());
      copy_matrix(yp_star, yp_saved);
      save_checkpoint(parameters.solver.checkpoint_out, parameters.verbosity,
                      yp_to_y_star, dual_objective_b_star, yp_saved, points,
                      infinity, parameters.solver.duality_gap_threshold,
                      primal_c_scale, backup_generation, current_generation);
    }
  return weights;
}
