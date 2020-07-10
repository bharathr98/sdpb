#pragma once

#include "Block.hxx"

#include <boost/filesystem.hpp>

struct Functional
{
  std::vector<Block> blocks;
  bool has_prefactor;

  Functional(const boost::filesystem::path &polynomials_path,
             const boost::filesystem::path &poles_path,
             const bool &Has_prefactor);
  std::vector<El::BigFloat>
  eval(const std::vector<El::BigFloat> &coords,
       const std::vector<std::vector<El::BigFloat>> &optimals);
};
