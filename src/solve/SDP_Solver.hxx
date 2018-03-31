//=======================================================================
// Copyright 2014-2015 David Simmons-Duffin.
// Distributed under the MIT License.
// (See accompanying file LICENSE or copy at
//  http://opensource.org/licenses/MIT)
//=======================================================================

#pragma once

#include "Block_Diagonal_Matrix.hxx"
#include "SDP.hxx"
#include "SDP_Solver_Terminate_Reason.hxx"

#include "../SDP_Solver_Parameters.hxx"

#include <boost/filesystem.hpp>

// SDPSolver contains the data structures needed during the running of
// the interior point algorithm.  Each structure is allocated when an
// SDPSolver is initialized, and reused in each iteration.
//
class SDP_Solver
{
public:
  // SDP to solve.
  SDP sdp;

  // parameters for initialization and iteration
  SDP_Solver_Parameters parameters;

  /********************************************/
  // Current point

  // a Vector of length P = sdp.primalObjective.size()
  Vector x;

  // a Block_Diagonal_Matrix with block sizes given by
  // sdp.psdMatrixBlockDims()
  Block_Diagonal_Matrix X;

  // a Vector of length N = sdp.dualObjective.size()
  Vector y;

  // a Block_Diagonal_Matrix with the same structure as X
  Block_Diagonal_Matrix Y;

  /********************************************/
  // Search direction
  //
  // These quantities have the same structure as (x, X, y, Y). They
  // are computed twice each iteration: once in the predictor step,
  // and once in the corrector step.
  //
  Vector dx;
  Block_Diagonal_Matrix dX;
  Vector dy;
  Block_Diagonal_Matrix dY;

  /********************************************/
  // Solver status

  // NB: here, primalObjective and dualObjective refer to the current
  // values of the objective functions.  In the class SDP, they refer
  // to the vectors c and b.  Hopefully the name-clash won't cause
  // confusion.
  Real primalObjective; // f + c . x
  Real dualObjective;   // f + b . y
  Real dualityGap;      // normalized difference of objectives

  // Discrepancy in the primal equality constraints, a
  // Block_Diagonal_Matrix with the same structure as X, called 'P' in
  // the manual:
  //
  //   PrimalResidues = \sum_p A_p x_p - X
  //
  Block_Diagonal_Matrix PrimalResidues;
  Real primalError; // maxAbs(PrimalResidues)

  // Discrepancy in the dual equality constraints, a Vector of length
  // P, called 'd' in the manual:
  //
  //   dualResidues = c - Tr(A_* Y) - B y
  //
  Vector dualResidues;
  Real dualError; // maxAbs(dualResidues)

  /********************************************/
  // Intermediate computations.

  // the Cholesky decompositions of X and Y, each lower-triangular
  // BlockDiagonalMatrices with the same block sizes as X and Y
  Block_Diagonal_Matrix XCholesky;
  Block_Diagonal_Matrix YCholesky;

  // Z = X^{-1} (PrimalResidues Y - R), a Block_Diagonal_Matrix with the
  // same block sizes as X and Y
  Block_Diagonal_Matrix Z;

  // R = mu I - X Y for the predictor step
  // R = mu I - X Y - dX dY for the corrector step
  // R has the same block sizes as X and Y.
  Block_Diagonal_Matrix R;

  // Bilinear pairings needed for computing the Schur complement
  // matrix.  For example,
  //
  //   BilinearPairingsXInv.blocks[b].elt(
  //     (d_j+1) s + k1,
  //     (d_j+1) r + k2
  //   ) = v_{b,k1}^T (X.blocks[b]^{-1})^{(s,r)} v_{b,k2}
  //
  //     0 <= k1,k2 <= sdp.degrees[j] = d_j
  //     0 <= s,r < sdp.dimensions[j] = m_j
  //
  // where j corresponds to b and M^{(s,r)} denotes the (s,r)-th
  // (d_j+1)x(d_j+1) block of M.
  //
  // BilinearPairingsXInv has one block for each block of X.  The
  // dimension of BilinearPairingsXInv.block[b] is (d_j+1)*m_j.  See
  // SDP.h for more information on d_j and m_j.
  //
  Block_Diagonal_Matrix BilinearPairingsXInv;
  //
  // BilinearPairingsY is analogous to BilinearPairingsXInv, with
  // X^{-1} -> Y.
  //
  Block_Diagonal_Matrix BilinearPairingsY;

  // The Schur complement matrix S: a Block_Diagonal_Matrix with one
  // block for each 0 <= j < J.  SchurComplement.blocks[j] has dimension
  // (d_j+1)*m_j*(m_j+1)/2
  //
  Block_Diagonal_Matrix SchurComplement;

  // SchurComplementCholesky = L', the Cholesky decomposition of the
  // stabilized Schur complement matrix S' = S + U U^T.
  Block_Diagonal_Matrix SchurComplementCholesky;

  // SchurOffDiagonal = L'^{-1} FreeVarMatrix, needed in solving the
  // Schur complement equation.
  Matrix SchurOffDiagonal;

  // As explained in the manual, we use the `stabilized' Schur
  // complement matrix S' = S + U U^T.  Here, the 'update' matrix U
  // has columns given by
  //
  //   U = ( Lambda_{p_1} e_{p_1}, ..., Lambda_{p_M} e_{p_M} )
  //
  // where e_p is a unit vector in the p-th direction and the
  // Lambda_{p_m} are constants.  If p_m appears above, we say the
  // direction p_m has been `stabilized.'  Because U is sparse, we
  // encode it in the smaller data structures below.
  //
  // Recall: S is block diagonal, with blocks labeled by 0 <= j < J.
  //
  // schurStabilizeIndices[j] = a list of which directions of
  // SchurComplement.blocks[j] have been stabilized (counting from 0 at
  // the upper left of each block), for 0 <= j < J.
  vector<vector<int>> schurStabilizeIndices;
  //
  // schurStabilizeLambdas[j] = a list of constants Lambda for each
  // stabilized direction in schurStabilizeIndices[j], for 0 <= j < J.
  vector<vector<Real>> schurStabilizeLambdas;
  //
  // a list of block indices {j_0, j_1, ..., j_{M-1} } for blocks
  // which have at least one stabilized direction.  We say the blocks
  // j_m have been `stabilized.'  Note that the number M of stabilized
  // blocks could change with each iteration.
  vector<int> stabilizeBlockIndices;
  //
  // For each 0 <= m < M, stabilizeBlockUpdateRow records the row of
  // U corresponding to the first stabilized direction in the j_m'th
  // block of SchurComplement:
  //
  //   stabilizeBlockUpdateRow[m] = schurStabilizeIndices[j_m][0] +
  //                                SchurComplement.blockStartIndices[j_m]
  //
  vector<int> stabilizeBlockUpdateRow;
  //
  // For each 0 <= m < M, stabilizeBlockUpdateColumn records the
  // column of U corresponding to the first stabilized direction in
  // the j_m'th block of SchurComplement.
  vector<int> stabilizeBlockUpdateColumn;
  //
  // For each 0 <= m < M, stabilizeBlocks is the submatrix of U
  // corresponding to the j_m'th block of SchurComplement.
  vector<Matrix> stabilizeBlocks;

  // Q = B' L'^{-T} L'^{-1} B' - {{0, 0}, {0, 1}}, where B' =
  // (FreeVarMatrix U).  Q is needed in the factorization of the Schur
  // complement equation.  Q has dimension N'xN', where
  //
  //   N' = cols(B) + cols(U) = N + cols(U)
  //
  // where N is the dimension of the dual objective function.  Note
  // that N' could change with each iteration.
  Matrix Q;
  // a vector of length N', needed for the LU decomposition of Q.
  vector<Integer> Qpivots;

  // dyExtended = (dy, z), where z are the extra coordinates introduced
  // in the stabilized Schur complement equation.
  Vector dyExtended;

  /********************************************/
  // Additional workspace variables

  // A Block_Diagonal_Matrix with the same structure as X, Y.  Needed
  // for computing step lengths which preserve positive
  // semidefiniteness.
  Block_Diagonal_Matrix StepMatrixWorkspace;

  // Needed during the computation of BilinearPairingsY, BilinearPairingsXInv
  vector<Matrix> bilinearPairingsWorkspace;

  // Needed during the step-length computation, where we must compute
  // eigenvalues of a Block_Diagonal_Matrix
  vector<Vector> eigenvaluesWorkspace;

  // Needed during the step-length computation, where we must compute
  // the QR decomposition of a Block_Diagonal_Matrix
  vector<Vector> QRWorkspace;

  /********************************************/
  // Methods

  // Create a new solver for a given SDP, with the given parameters
  SDP_Solver(const SDP &sdp, const SDP_Solver_Parameters &parameters);

  // Run the solver, backing up to checkpointFile
  SDP_Solver_Terminate_Reason
  run(const boost::filesystem::path checkpointFile);

  // Input/output
  void saveCheckpoint(const boost::filesystem::path &checkpointFile);
  void loadCheckpoint(const boost::filesystem::path &checkpointFile);
  void saveSolution(const SDP_Solver_Terminate_Reason,
                    const boost::filesystem::path &outFile);
  void printHeader();
  void printIteration(int iteration, Real mu, Real primalStepLength,
                      Real dualStepLength, Real betaCorrector);

  void testMultiplication(const int m_init, const int m_fin, const int m_step);

private:
  // Compute data needed to solve the Schur complement equation
  void initializeSchurComplementSolver(
    const Block_Diagonal_Matrix &BilinearPairingsXInv,
    const Block_Diagonal_Matrix &BilinearPairingsY,
    const Real &choleskyStabilizeThreshold);

  // Solve the Schur complement equation in-place for dx, dy.  dx and
  // dy will initially be set to the right-hand side of the equation.
  // This function replaces them with the corresponding solutions.
  void solveSchurComplementEquation(Vector &dx, Vector &dy);

  // Compute (dx, dX, dy, dY), given the current mu, a reduction
  // parameter beta.  `correctorPhase' specifies whether to use the
  // R-matrix corresponding to the corrector step (if false, we use
  // the predictor R-matrix)
  void computeSearchDirection(const Real &beta, const Real &mu,
                              const bool correctorPhase);
};
