#include <gtest/gtest.h>

#include <micm/process/ternary_chemical_activation_rate_constant.hpp>
#include <micm/solver/state.hpp>
#include <micm/system/system.hpp>

TEST(TernaryChemicalActivationRateConstant, CalculateWithMinimalArugments)
{
  micm::State<micm::Matrix> state{ 0, 0, 1 };
  state.conditions_[0].temperature_ = 301.24;  // [K]
  state.conditions_[0].air_density_ = 42.2;    // [mol mol-1]
  std::vector<double>::const_iterator params = state.custom_rate_parameters_[0].begin();
  micm::TernaryChemicalActivationRateConstantParameters ternary_params;
  ternary_params.k0_A_ = 1.0;
  ternary_params.kinf_A_ = 1.0;
  micm::TernaryChemicalActivationRateConstant ternary{ ternary_params };
  auto k = ternary.calculate(state.conditions_[0], params);
  double k0 = 1.0;
  double kinf = 1.0;
  EXPECT_NEAR(k, k0 / (1.0 + 42.4 * k0 / kinf) * pow(0.6, 1.0 / (1 + pow(log10(42.2 * k0 / kinf), 2))), 0.001);
}

TEST(TernaryChemicalActivationRateConstant, CalculateWithAllArugments)
{
  micm::State<micm::Matrix> state{ 0, 0, 1 };
  double temperature = 301.24;
  state.conditions_[0].temperature_ = temperature;  // [K]
  state.conditions_[0].air_density_ = 42.2;         // [mol mol-1]
  std::vector<double>::const_iterator params = state.custom_rate_parameters_[0].begin();
  micm::TernaryChemicalActivationRateConstant ternary{ micm::TernaryChemicalActivationRateConstantParameters{
      .k0_A_ = 1.2,
      .k0_B_ = 2.3,
      .k0_C_ = 302.3,
      .kinf_A_ = 2.6,
      .kinf_B_ = -3.1,
      .kinf_C_ = 402.1,
      .Fc_ = 0.9,
      .N_ = 1.2 } };
  auto k = ternary.calculate(state.conditions_[0], params);
  double k0 = 1.2 * exp(302.3 / temperature) * pow(temperature / 300.0, 2.3);
  double kinf = 2.6 * exp(402.1 / temperature) * pow(temperature / 300.0, -3.1);
  EXPECT_NEAR(k, k0 / (1.0 + 42.2 * k0 / kinf) * pow(0.9, 1.0 / (1.0 + 1.0 / 1.2 * pow(log10(42.2 * k0 / kinf), 2))), 0.001);
}