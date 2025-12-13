#include "ModuleParams.h"

ModuleParams::ModuleParams() = default;

void ModuleParams::clear_all_params()
{
    params_cleared();
    param_listeners.reset();
    forwarding_params->clearParameterRange (0, num_forward_parameters);
    float_params.clear();
    choice_params.clear();
    holder = nullptr;
}

void ModuleParams::finished_loading_params()
{
    holder = std::make_unique<chowdsp::ParamHolder> (nullptr, "Module Params", false);
    holder->add (float_params, choice_params);
    forwarding_params->setParameterRange (0,
                                          std::min (static_cast<int> (float_params.size() + choice_params.size()),
                                                    num_forward_parameters),
                                          [this] (int idx) -> chowdsp::ParameterForwardingInfo
                                          {
                                              auto index = static_cast<size_t> (idx);
                                              if (index < float_params.size())
                                              {
                                                  auto* param = float_params[index].get();
                                                  return { param, param->name };
                                              }

                                              index -= float_params.size();
                                              if (index < choice_params.size())
                                              {
                                                  auto* param = choice_params[index].get();
                                                  return { param, param->name };
                                              }

                                              return {};
                                          });
    param_listeners.emplace (*holder);
    params_added();
}
