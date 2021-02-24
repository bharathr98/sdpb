#include "eval_function.hxx"
#include <El.hpp>
#include <vector>

El::BigFloat eval_weighted_functions(
  const El::BigFloat &infinity,
  const std::vector<
    std::vector<std::vector<std::map<El::BigFloat, El::BigFloat>>>> &functions,
  const El::BigFloat &x, const std::vector<El::BigFloat> &weights)
{
  const size_t matrix_dim(functions.size());
  El::Matrix<El::BigFloat> m(matrix_dim, matrix_dim);
  for(size_t row(0); row != matrix_dim; ++row)
    for(size_t column(0); column <= row; ++column)
      {
        auto &function(functions.at(row).at(column));
        if(weights.size() != function.size())
          {
            throw std::runtime_error("INTERNAL ERROR mismatch: "
                                     + std::to_string(weights.size()) + " "
                                     + std::to_string(function.size()));
          }
        El::BigFloat element(0);
        for(size_t index(0); index != weights.size(); ++index)
          {
            element += weights[index] * eval_function(infinity, function[index], x);
          }
        m.Set(row, column, element);
      }

  // FIXME: Use the square of the matrix rather than the smallest
  // eigenvalue?  That would map to B^T B.

  El::Matrix<El::BigFloat> eigenvalues;
  /// There is a bug in El::HermitianEig when there is more than
  /// one level of recursion when computing eigenvalues.  One fix
  /// is to increase the cutoff so that there is no more than one
  /// level of recursion.

  /// An alternate workaround is to compute both eigenvalues and
  /// eigenvectors, but that seems to be significantly slower.
  El::HermitianEigCtrl<El::BigFloat> hermitian_eig_ctrl;
  hermitian_eig_ctrl.tridiagEigCtrl.dcCtrl.cutoff = matrix_dim / 2 + 1;

  /// The default number of iterations is 40.  That is sometimes
  /// not enough, so we bump it up significantly.
  hermitian_eig_ctrl.tridiagEigCtrl.dcCtrl.secularCtrl.maxIterations = 400;
  El::HermitianEig(El::UpperOrLowerNS::LOWER, m, eigenvalues,
                   hermitian_eig_ctrl);
  return El::Min(eigenvalues);
}
