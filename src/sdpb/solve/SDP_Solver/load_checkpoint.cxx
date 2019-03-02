#include "../SDP_Solver.hxx"

#include <boost/filesystem.hpp>
#include <boost/filesystem/fstream.hpp>

template <typename T>
void read_local_blocks(T &t, boost::filesystem::ifstream &checkpoint_stream)
{
  El::BigFloat zero(0);
  const size_t serialized_size(zero.SerializedSize());
  std::vector<uint8_t> local_array(serialized_size);

  for(auto &block : t.blocks)
    {
      int64_t local_height, local_width;
      checkpoint_stream.read(reinterpret_cast<char *>(&local_height),
                             sizeof(int64_t));
      checkpoint_stream.read(reinterpret_cast<char *>(&local_width),
                             sizeof(int64_t));
      if(local_height != block.LocalHeight()
         || local_width != block.LocalWidth())
        {
          std::stringstream ss;
          ss << "Incompatible checkpoint file.  Expected dimensions ("
             << block.LocalHeight() << "," << block.LocalWidth()
             << "), but found (" << local_height << "," << local_width << ")";

          throw std::runtime_error(ss.str());
        }

      for(int64_t row = 0; row < local_height; ++row)
        for(int64_t column = 0; column < local_width; ++column)
          {
            El::BigFloat input;
            checkpoint_stream.read(
              reinterpret_cast<char *>(local_array.data()),
              std::streamsize(local_array.size()));
            input.Deserialize(local_array.data());

            block.SetLocal(row, column, input);
          }
    }
}

bool SDP_Solver::load_checkpoint(
  const boost::filesystem::path &checkpoint_directory,
  const Verbosity &verbosity)
{
  boost::filesystem::path checkpoint_filename(
    checkpoint_directory / ("checkpoint." + std::to_string(El::mpi::Rank())));

  if(!exists(checkpoint_filename))
    {
      return false;
    }

  boost::filesystem::ifstream checkpoint_stream(checkpoint_filename);
  if(verbosity >= Verbosity::regular && El::mpi::Rank() == 0)
    {
      std::cout << "Loading checkpoint from : " << checkpoint_directory
                << '\n';
    }
  read_local_blocks(x, checkpoint_stream);
  read_local_blocks(X, checkpoint_stream);
  read_local_blocks(y, checkpoint_stream);
  read_local_blocks(Y, checkpoint_stream);
  return true;
}
