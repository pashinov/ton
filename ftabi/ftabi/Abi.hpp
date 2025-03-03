#pragma once

#include <optional>

#include <td/utils/JsonBuilder.h>

#include "utils.hpp"

namespace ftabi {

struct FunctionAbi {
  std::string name;
  InputParams inputs;
  OutputParams outputs;
  std::optional<uint32_t> id;
};

struct ContractAbi {
  auto create_function(std::string name) -> td::Result<Function>;

  HeaderParams header{};
  std::unordered_map<std::string, FunctionAbi> functions{};
};

auto abi_param_from_json(td::JsonValue& object) -> td::Result<ParamRef>;
auto abi_params_from_json(td::JsonValue& object) -> td::Result<std::vector<ParamRef>>;
auto abi_header_from_json(td::JsonValue& object) -> td::Result<HeaderParams>;
auto abi_function_from_json(td::JsonValue& object) -> td::Result<FunctionAbi>;
auto abi_functions_from_json(td::JsonValue& object) -> td::Result<std::unordered_map<std::string, FunctionAbi>>;

auto value_from_json(ParamRef param, td::JsonValue& object) -> td::Result<ValueRef>;

auto contract_abi_from_json(td::JsonValue& object) -> td::Result<ContractAbi>;
auto function_call_from_json(const Function& function, td::JsonValue& object) -> td::Result<FunctionCall>;

}  // namespace ftabi
