#include <gtest/gtest.h>

#include <micm/solver/rosenbrock.hpp>
#include <micm/util/matrix.hpp>
#include <micm/util/sparse_matrix.hpp>
#include <micm/util/sparse_matrix_vector_ordering.hpp>
#include <micm/util/vector_matrix.hpp>
#include <random>

#include "chapman_ode_solver.hpp"
#include "util.hpp"

template<template<class> class MatrixPolicy, template<class> class SparseMatrixPolicy>
void testRateConstants()
{
  micm::ChapmanODESolver fixed_solver{};
  auto solver = getThreeStageMultiCellChapmanSolver<MatrixPolicy, SparseMatrixPolicy>(3);

  auto state = solver.GetState();
  auto fixed_state = fixed_solver.GetState();
  const std::vector<std::vector<double>> photo_rates{ { 1.0e-4, 1.0e-5, 1.0e-6 },
                                                      { 3.2e-4, 7.3e-5, 3.2e-6 },
                                                      { 5.2e-4, 8.2e-5, 4.6e-6 } };
  state.custom_rate_parameters_ = photo_rates;
  state.conditions_[0].temperature_ = 284.19;  // [K]
  state.conditions_[0].pressure_ = 101245.0;   // [Pa]
  state.conditions_[1].temperature_ = 215.02;  // [K]
  state.conditions_[1].pressure_ = 100789.2;   // [Pa]
  state.conditions_[2].temperature_ = 299.31;  // [K]
  state.conditions_[2].pressure_ = 101398.0;   // [Pa]

  solver.UpdateState(state);

  for (size_t i{}; i < 3; ++i)
  {
    fixed_state.conditions_[0].temperature_ = state.conditions_[i].temperature_;
    fixed_state.conditions_[0].pressure_ = state.conditions_[i].pressure_;
    fixed_state.custom_rate_parameters_[0] = photo_rates[i];
    fixed_solver.UpdateState(fixed_state);

    EXPECT_EQ(state.rate_constants_[i].size(), fixed_state.rate_constants_[0].size());
    for (size_t j{}; j < state.rate_constants_[i].size(); ++j)
    {
      EXPECT_EQ(state.rate_constants_[i][j], fixed_state.rate_constants_[0][j]);
    }
  }
}

template<template<class> class MatrixPolicy, template<class> class SparseMatrixPolicy>
void testForcing()
{
  std::random_device rnd_device;
  std::mt19937 engine{ rnd_device() };
  std::lognormal_distribution dist(-2.0, 2.0);

  micm::ChapmanODESolver fixed_solver{};
  auto solver = getThreeStageMultiCellChapmanSolver<MatrixPolicy, SparseMatrixPolicy>(3);

  auto state = solver.GetState();

  auto& state_vec = state.variables_.AsVector();
  std::generate(begin(state_vec), end(state_vec), [&]() { return dist(engine); });
  auto& rate_const_vec = state.rate_constants_.AsVector();
  std::generate(begin(rate_const_vec), end(rate_const_vec), [&]() { return dist(engine); });

  MatrixPolicy<double> forcing(3, 9);
  solver.CalculateForcing(state.rate_constants_, state.variables_, forcing);

  for (std::size_t i{}; i < 3; ++i)
  {
    double number_density_air = 1.0;
    std::vector<double> rate_constants = state.rate_constants_[i];
    std::vector<double> variables(state.variables_[i].size());
    for (std::size_t j{}; j < state.variables_[i].size(); ++j)
      variables[j] = state.variables_[i][state.variable_map_[fixed_solver.species_names()[j]]];
    std::vector<double> fixed_forcing = fixed_solver.force(rate_constants, variables, number_density_air);

    EXPECT_EQ(forcing[i].size(), fixed_forcing.size());
    for (std::size_t j{}; j < fixed_forcing.size(); ++j)
    {
      double a = forcing[i][state.variable_map_[fixed_solver.species_names()[j]]];
      double b = fixed_forcing[j];
      EXPECT_NEAR(a, b, (std::abs(a) + std::abs(b)) * 1.0e-8 + 1.0e-12);
    }
  }
}

template<class T>
using DenseMatrix = micm::Matrix<T>;
template<class T>
using SparseMatrix = micm::SparseMatrix<T>;

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

TEST(RegressionRosenbrock, RateConstants)
{
  testRateConstants<DenseMatrix, SparseMatrix>();
}

TEST(RegressionRosenbrock, VectorRateConstants)
{
  testRateConstants<Group1VectorMatrix, Group1SparseVectorMatrix>();
  testRateConstants<Group2VectorMatrix, Group2SparseVectorMatrix>();
  testRateConstants<Group3VectorMatrix, Group3SparseVectorMatrix>();
  testRateConstants<Group4VectorMatrix, Group4SparseVectorMatrix>();
}

TEST(RegressionRosenbrock, Forcing)
{
  testForcing<DenseMatrix, SparseMatrix>();
}

TEST(RegressionRosenbrock, VectorForcing)
{
  testForcing<Group1VectorMatrix, Group1SparseVectorMatrix>();
  testForcing<Group2VectorMatrix, Group2SparseVectorMatrix>();
  testForcing<Group3VectorMatrix, Group3SparseVectorMatrix>();
  testForcing<Group4VectorMatrix, Group4SparseVectorMatrix>();
}