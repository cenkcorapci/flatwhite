#pragma once

#include <localagent/model/model_types.hpp>

#include <functional>
#include <vector>

namespace localagent {

class ModelBackend {
public:
  using StreamCallback = std::function<void(const ChatChunk&)>;

  virtual ~ModelBackend() = default;

  [[nodiscard]] virtual ChatResponse chat(const ChatRequest& request,
                                          StreamCallback stream = nullptr) = 0;

  [[nodiscard]] virtual std::vector<ModelInfo> list_models() = 0;

  virtual void pull_model(const ModelId& model) = 0;

  [[nodiscard]] virtual EmbeddingResult embed(const EmbeddingRequest& request) = 0;
};

}  // namespace localagent
