#include <gtest/gtest.h>

#include <micm/jit/jit_compiler.hpp>
#include <micm/process/arrhenius_rate_constant.hpp>
#include <micm/solver/jit_rosenbrock.hpp>
#include <micm/solver/rosenbrock.hpp>
#include <micm/util/matrix.hpp>
#include <micm/util/sparse_matrix.hpp>
#include <micm/util/sparse_matrix_vector_ordering.hpp>
#include <micm/util/vector_matrix.hpp>

template<template<class> class MatrixPolicy, template<class> class SparseMatrixPolicy>
micm::JitRosenbrockSolver<MatrixPolicy, SparseMatrixPolicy> getSolver(
    std::shared_ptr<micm::JitCompiler> jit,
    std::size_t number_of_grid_cells)
{
  // ---- foo  bar  baz  quz  quuz
  // foo   0    1    2    -    -
  // bar   3    4    5    -    -
  // baz   6    -    7    -    -
  // quz   -    8    -    9    -
  // quuz 10    -   11    -    12

  auto foo = micm::Species("foo");
  auto bar = micm::Species("bar");
  auto baz = micm::Species("baz");
  auto quz = micm::Species("quz");
  auto quuz = micm::Species("quuz");

  micm::Phase gas_phase{ std::vector<micm::Species>{ foo, bar, baz, quz, quuz } };

  micm::Process r1 = micm::Process::create()
                         .reactants({ foo, baz })
                         .products({ yields(bar, 1), yields(quuz, 2.4) })
                         .phase(gas_phase)
                         .rate_constant(micm::ArrheniusRateConstant({ .A_ = 2.0e-11, .B_ = 0, .C_ = 110 }));

  micm::Process r2 = micm::Process::create()
                         .reactants({ bar })
                         .products({ yields(foo, 1), yields(quz, 1.4) })
                         .phase(gas_phase)
                         .rate_constant(micm::ArrheniusRateConstant({ .A_ = 1.0e-6 }));

  micm::Process r3 = micm::Process::create().reactants({ quz }).products({}).phase(gas_phase).rate_constant(
      micm::ArrheniusRateConstant({ .A_ = 3.5e-6 }));

  return micm::JitRosenbrockSolver<MatrixPolicy, SparseMatrixPolicy>(
      jit,
      micm::System(micm::SystemParameters{ .gas_phase_ = gas_phase }),
      std::vector<micm::Process>{ r1, r2, r3 },
      micm::RosenbrockSolverParameters::three_stage_rosenbrock_parameters(number_of_grid_cells, false));
}

template<class T>
using SparseMatrix = micm::SparseMatrix<T>;

template<template<class> class MatrixPolicy, template<class> class SparseMatrixPolicy>
void testAlphaMinusJacobian(std::shared_ptr<micm::JitCompiler> jit, std::size_t number_of_grid_cells)
{
  auto solver = getSolver<MatrixPolicy, SparseMatrixPolicy>(jit, number_of_grid_cells);
  // return;
  auto jacobian = solver.jacobian_;

  EXPECT_EQ(jacobian.size(), number_of_grid_cells);
  EXPECT_EQ(jacobian[0].size(), 5);
  EXPECT_EQ(jacobian[0][0].size(), 5);
  EXPECT_GE(jacobian.AsVector().size(), 13 * number_of_grid_cells);
  for (auto& elem : jacobian.AsVector())
    elem = 100.0;
  for (std::size_t i_cell = 0; i_cell < number_of_grid_cells; ++i_cell)
  {
    jacobian[i_cell][0][0] = 12.2;
    jacobian[i_cell][0][1] = 24.3 * (i_cell + 2);
    jacobian[i_cell][0][2] = 42.3;
    jacobian[i_cell][1][0] = 0.43;
    jacobian[i_cell][1][1] = 23.4;
    jacobian[i_cell][1][2] = 83.4 / (i_cell + 3);
    jacobian[i_cell][2][0] = 4.74;
    jacobian[i_cell][2][2] = 6.91;
    jacobian[i_cell][3][1] = 59.1;
    jacobian[i_cell][3][3] = 83.4;
    jacobian[i_cell][4][0] = 78.5;
    jacobian[i_cell][4][2] = 53.6;
    jacobian[i_cell][4][4] = 1.0;
  }
  solver.AlphaMinusJacobian(jacobian, 42.042);
  for (std::size_t i_cell = 0; i_cell < number_of_grid_cells; ++i_cell)
  {
    EXPECT_NEAR(jacobian[i_cell][0][0], 42.042 - 12.2, 1.0e-5);
    EXPECT_NEAR(jacobian[i_cell][0][1], -24.3 * (i_cell + 2), 1.0e-5);
    EXPECT_NEAR(jacobian[i_cell][0][2], -42.3, 1.0e-5);
    EXPECT_NEAR(jacobian[i_cell][1][0], -0.43, 1.0e-5);
    EXPECT_NEAR(jacobian[i_cell][1][1], 42.042 - 23.4, 1.0e-5);
    EXPECT_NEAR(jacobian[i_cell][1][2], -83.4 / (i_cell + 3), 1.0e-5);
    EXPECT_NEAR(jacobian[i_cell][2][0], -4.74, 1.0e-5);
    EXPECT_NEAR(jacobian[i_cell][2][2], 42.042 - 6.91, 1.0e-5);
    EXPECT_NEAR(jacobian[i_cell][3][1], -59.1, 1.0e-5);
    EXPECT_NEAR(jacobian[i_cell][3][3], 42.042 - 83.4, 1.0e-5);
    EXPECT_NEAR(jacobian[i_cell][4][0], -78.5, 1.0e-5);
    EXPECT_NEAR(jacobian[i_cell][4][2], -53.6, 1.0e-5);
    EXPECT_NEAR(jacobian[i_cell][4][4], 42.042 - 1.0, 1.0e-5);
  }
}

template<class T>
using Group1VectorMatrix = micm::VectorMatrix<T, 1>;
template<class T>
using Group2VectorMatrix = micm::VectorMatrix<T, 2>;
template<class T>
using Group3VectorMatrix = micm::VectorMatrix<T, 3>;
template<class T>
using Group4VectorMatrix = micm::VectorMatrix<T, 4>;

template<class T>
using Group1SparseVectorMatrix = micm::SparseMatrix<T, micm::SparseMatrixVectorOrdering<1>>;
template<class T>
using Group2SparseVectorMatrix = micm::SparseMatrix<T, micm::SparseMatrixVectorOrdering<2>>;
template<class T>
using Group3SparseVectorMatrix = micm::SparseMatrix<T, micm::SparseMatrixVectorOrdering<3>>;
template<class T>
using Group4SparseVectorMatrix = micm::SparseMatrix<T, micm::SparseMatrixVectorOrdering<4>>;

TEST(JitRosenbrockSolver, DenseAlphaMinusJacobian)
{
  auto jit{ micm::JitCompiler::create() };
  if (auto err = jit.takeError())
  {
    llvm::logAllUnhandledErrors(std::move(err), llvm::errs(), "[JIT Error]");
    EXPECT_TRUE(false);
  }
  testAlphaMinusJacobian<Group1VectorMatrix, Group1SparseVectorMatrix>(jit.get(), 1);
  testAlphaMinusJacobian<Group2VectorMatrix, Group2SparseVectorMatrix>(jit.get(), 2);
  testAlphaMinusJacobian<Group3VectorMatrix, Group3SparseVectorMatrix>(jit.get(), 3);
  testAlphaMinusJacobian<Group4VectorMatrix, Group4SparseVectorMatrix>(jit.get(), 4);
}

// create mozart-micm
// enable multiple grid cells for KPP
// add mozart to performace