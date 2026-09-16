#pragma once

#include <localagent/model/backend.hpp>

#include <chrono>
#include <cstddef>
#include <functional>
#include <memory>
#include <queue>
#include <string>
#include <vector>

namespace localagent::ollama {

struct HttpResponse {
  long status_code{0};
  std::string body;
  std::string error;
};

using StreamDataCallback = std::function<bool(std::string_view chunk)>;

class HttpTransport {
public:
  virtual ~HttpTransport() = default;

  [[nodiscard]] virtual HttpResponse post(const std::string& url,
                                          const std::string& json_body,
                                          bool stream,
                                          StreamDataCallback on_data) = 0;

  [[nodiscard]] virtual HttpResponse get(const std::string& url) = 0;
};

class CurlHttpTransport final : public HttpTransport {
public:
  [[nodiscard]] HttpResponse post(const std::string& url, const std::string& json_body,
                                  bool stream, StreamDataCallback on_data) override;

  [[nodiscard]] HttpResponse get(const std::string& url) override;
};

struct OllamaConfig {
  std::string base_url{"http://127.0.0.1:11434"};
  std::chrono::seconds timeout{std::chrono::seconds{300}};
};

class OllamaBackend final : public ModelBackend {
public:
  explicit OllamaBackend(OllamaConfig config = {},
                         std::shared_ptr<HttpTransport> transport = nullptr);

  [[nodiscard]] ChatResponse chat(const ChatRequest& request,
                                  StreamCallback stream = nullptr) override;

  [[nodiscard]] std::vector<ModelInfo> list_models() override;

  void pull_model(const ModelId& model) override;

  [[nodiscard]] EmbeddingResult embed(const EmbeddingRequest& request) override;

  [[nodiscard]] const OllamaConfig& config() const noexcept { return config_; }

private:
  OllamaConfig config_;
  std::shared_ptr<HttpTransport> transport_;
};

class FakeModelBackend final : public ModelBackend {
public:
  struct ScriptedResponse {
    ChatResponse response;
    std::vector<ChatChunk> stream_chunks;
  };

  explicit FakeModelBackend(std::vector<ScriptedResponse> script = {},
                            std::vector<ModelInfo> models = {});

  void enqueue(ScriptedResponse response);

  [[nodiscard]] ChatResponse chat(const ChatRequest& request,
                                  StreamCallback stream = nullptr) override;

  [[nodiscard]] std::vector<ModelInfo> list_models() override;

  void pull_model(const ModelId& model) override;

  [[nodiscard]] EmbeddingResult embed(const EmbeddingRequest& request) override;

  [[nodiscard]] std::size_t chat_call_count() const noexcept { return chat_calls_; }

private:
  std::queue<ScriptedResponse> script_;
  std::vector<ModelInfo> models_;
  std::size_t chat_calls_{0};
};

[[nodiscard]] bool ollama_reachable(const std::string& base_url);

}  // namespace localagent::ollama
