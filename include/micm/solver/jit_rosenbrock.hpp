/* Copyright (C) 2023 National Center for Atmospheric Research,
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 * Much of this solver was formulated and implemented from this book:
 * Hairer, E., Wanner, G., 1996. Solving Ordinary Differential Equations II: Stiff and Differential-Algebraic Problems, 2nd
 * edition. ed. Springer, Berlin ; New York. The source code for many (all?) of the solvers in that book can be found here:
 * http://www.unige.ch/~hairer/software.html
 *
 * Some extensions to the rosenbrock solver formulated there were formulated in this paper
 * Sandu, A., Verwer, J.G., Blom, J.G., Spee, E.J., Carmichael, G.R., Potra, F.A., 1997. Benchmarking stiff ode solvers for
 * atmospheric chemistry problems II: Rosenbrock solvers. Atmospheric Environment 31, 3459–3472.
 * https://doi.org/10.1016/S1352-2310(97)83212-8
 *
 */
#pragma once

#include <chrono>
#include <ctime>
#include <micm/jit/jit_compiler.hpp>
#include <micm/jit/jit_function.hpp>
#include <micm/solver/rosenbrock.hpp>

namespace micm
{

  template<template<class> class MatrixPolicy = Matrix, template<class> class SparseMatrixPolicy = SparseMatrix>
  class JitRosenbrockSolver : public RosenbrockSolver<MatrixPolicy, SparseMatrixPolicy>
  {
    std::shared_ptr<JitCompiler> compiler_;
    llvm::orc::ResourceTrackerSP function_resource_tracker_;
    using FuncPtr = void (*)(double*, const double);
    FuncPtr alpha_minus_jacobian_ = nullptr;

   public:
    /// @brief Builds a Rosenbrock solver for the given system, processes, and solver parameters
    /// @param system The chemical system to create the solver for
    /// @param processes The collection of chemical processes that will be applied during solving
    JitRosenbrockSolver(
        std::shared_ptr<JitCompiler> compiler,
        const System& system,
        const std::vector<Process>& processes,
        const RosenbrockSolverParameters& parameters)
        : RosenbrockSolver<MatrixPolicy, SparseMatrixPolicy>(system, processes, parameters),
          compiler_(compiler)
    {
      this->GenerateAlphaMinusJacobian();
    }

    ~JitRosenbrockSolver()
    {
      if (function_resource_tracker_ != NULL)
      {
        llvm::ExitOnError exit_on_error;
        exit_on_error(function_resource_tracker_->remove());
      }
    }

    /// @brief compute [alpha * I - dforce_dy]
    /// @param jacobian Jacobian matrix (dforce_dy)
    /// @param alpha
    void AlphaMinusJacobian(SparseMatrixPolicy<double>& jacobian, const double& alpha) const
    {
      double a = alpha;
      if (alpha_minus_jacobian_)
      {
        for (auto& elem : jacobian.AsVector())
          elem = -elem;
        alpha_minus_jacobian_(jacobian.AsVector().data(), a);
      }
      else
      {
        throw std::runtime_error("Failed to generate the alpha minus jacobia JIT function.");
      }
    }

   private:
    void GenerateAlphaMinusJacobian()
    {
      // save sizes needed throughout the function
      std::size_t n_cells = this->jacobian_.GroupVectorSize();
      std::size_t number_of_nonzero_jacobian_elements = this->jacobian_.AsVector().size();

      // Get the current timestamp using std::chrono::high_resolution_clock
      std::chrono::high_resolution_clock::time_point now = std::chrono::high_resolution_clock::now();
      std::chrono::nanoseconds ns = std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch());
      long long timestamp = ns.count();

      // Convert the timestamp to a string
      std::string timestampStr = std::to_string(timestamp);

      // Create the function name with the timestamp
      std::string functionName = "alpha_minus_jacobian_" + timestampStr;

      // Create the JitFunction with the modified name
      JitFunction func = JitFunction::create(compiler_)
                             .name("alpha_minus_jacobian")
                             .arguments({ { "jacobian", JitType::DoublePtr }, { "alpha", JitType::Double } })
                             .return_type(JitType::Void);

      // constants
      llvm::Value* zero = llvm::ConstantInt::get(*(func.context_), llvm::APInt(64, 0));
      llvm::Value* negative_one = llvm::ConstantFP::get(*(func.context_), llvm::APFloat(-1.0));
      llvm::Value* jacobian_size =
          llvm::ConstantInt::get(*(func.context_), llvm::APInt(64, number_of_nonzero_jacobian_elements));

      // types
      llvm::Type* double_type = func.GetType(JitType::Double);

      // iterative over the blocks of the jacobian and add the alpha value
      // jacobian_vector[i_elem + i_cell] += alpha;
      for (const auto& i_elem : this->jacobian_diagonal_elements_)
      {
        llvm::Value* ptr_index[1];

        auto cell_loop = func.StartLoop("add alpha", 0, n_cells);
        llvm::Value* elem_id = llvm::ConstantInt::get(*(func.context_), llvm::APInt(64, i_elem));

        ptr_index[0] = func.builder_->CreateNSWAdd(cell_loop.index_, elem_id);

        llvm::Value* indexer =
            func.builder_->CreateGEP(double_type, func.arguments_[0].ptr_, ptr_index, "index jacobian array");
        llvm::Value* jacobian_element = func.builder_->CreateLoad(double_type, indexer, "load jacobian element");

        jacobian_element = func.builder_->CreateFAdd(jacobian_element, func.arguments_[1].ptr_, "add alpha");
        func.builder_->CreateStore(jacobian_element, indexer);

        func.EndLoop(cell_loop);
      }

      func.builder_->CreateRetVoid();

      auto target = func.Generate();

      alpha_minus_jacobian_ = (FuncPtr)(intptr_t)target.second;
      function_resource_tracker_ = target.first;
    }
  };
}  // namespace micm