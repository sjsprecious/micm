#include <gtest/gtest.h>

#include <micm/configure/solver_config.hpp>

TEST(TunnelingConfig, DetectsInvalidConfig)
{
  micm::SolverConfig solver_config;

  // Read and parse the configure files
  micm::ConfigParseStatus status = solver_config.ReadAndParse("./unit_configs/process/tunneling/missing_reactants");
  EXPECT_EQ(micm::ConfigParseStatus::RequiredKeyNotFound, status);
  status = solver_config.ReadAndParse("./unit_configs/process/tunneling/missing_products");
  EXPECT_EQ(micm::ConfigParseStatus::RequiredKeyNotFound, status);
}

TEST(TunnelingConfig, ParseConfig)
{
  micm::SolverConfig solver_config;

  micm::ConfigParseStatus status = solver_config.ReadAndParse("./unit_configs/process/tunneling/valid");
  EXPECT_EQ(micm::ConfigParseStatus::Success, status);

  micm::SolverParameters solver_params = solver_config.GetSolverParams();

  auto& process_vector = solver_params.processes_;

  // first reaction
  {
    EXPECT_EQ(process_vector[0].reactants_.size(), 3);
    EXPECT_EQ(process_vector[0].reactants_[0].name_, "foo");
    EXPECT_EQ(process_vector[0].reactants_[1].name_, "quz");
    EXPECT_EQ(process_vector[0].reactants_[2].name_, "quz");
    EXPECT_EQ(process_vector[0].products_.size(), 2);
    EXPECT_EQ(process_vector[0].products_[0].first.name_, "bar");
    EXPECT_EQ(process_vector[0].products_[0].second, 1.0);
    EXPECT_EQ(process_vector[0].products_[1].first.name_, "baz");
    EXPECT_EQ(process_vector[0].products_[1].second, 3.2);
    micm::TunnelingRateConstant* tunneling_rate_constant =
        dynamic_cast<micm::TunnelingRateConstant*>(process_vector[0].rate_constant_.get());
    auto& params = tunneling_rate_constant->parameters_;
    EXPECT_EQ(params.A_, 1.0);
    EXPECT_EQ(params.B_, 0.0);
    EXPECT_EQ(params.C_, 0.0);
  }

  // second reaction
  {
    EXPECT_EQ(process_vector[1].reactants_.size(), 2);
    EXPECT_EQ(process_vector[1].reactants_[0].name_, "bar");
    EXPECT_EQ(process_vector[1].reactants_[1].name_, "baz");
    EXPECT_EQ(process_vector[1].products_.size(), 2);
    EXPECT_EQ(process_vector[1].products_[0].first.name_, "bar");
    EXPECT_EQ(process_vector[1].products_[0].second, 0.5);
    EXPECT_EQ(process_vector[1].products_[1].first.name_, "foo");
    EXPECT_EQ(process_vector[1].products_[1].second, 1.0);
    micm::TunnelingRateConstant* tunneling_rate_constant =
        dynamic_cast<micm::TunnelingRateConstant*>(process_vector[1].rate_constant_.get());
    auto& params = tunneling_rate_constant->parameters_;
    EXPECT_EQ(params.A_, 32.1);
    EXPECT_EQ(params.B_, -2.3);
    EXPECT_EQ(params.C_, 102.3);
  }
}
