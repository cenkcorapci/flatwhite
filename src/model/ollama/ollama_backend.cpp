#include <localagent/model/ollama/ollama_backend.hpp>
#include <localagent/model/ollama/stream_parser.hpp>

#include <nlohmann/json.hpp>

#include <curl/curl.h>

#include <cctype>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace localagent::ollama {
namespace {

using json = nlohmann::json;

size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
  const auto total = size * nmemb;
  auto* out = static_cast<std::string*>(userdata);
  out->append(ptr, total);
  return total;
}

size_t stream_callback(char* ptr, size_t size, size_t nmemb, void* userdata) {
  const auto total = size * nmemb;
  auto* cb = static_cast<StreamDataCallback*>(userdata);
  if (*cb) {
    return (*cb)(std::string_view{ptr, total}) ? total : 0;
  }
  return total;
}

json message_to_json(const Message& message) {
  json node;
  node["role"] = to_string(message.role);
  node["content"] = message.content;
  if (message.name) {
    node["name"] = *message.name;
  }
  if (message.tool_call_id) {
    node["tool_call_id"] = *message.tool_call_id;
  }
  return node;
}

json tool_to_json(const ToolDefinition& tool) {
  json node;
  node["type"] = "function";
  node["function"] = {
      {"name", tool.name},
      {"description", tool.description},
      {"parameters", json::parse(tool.parameters_json.empty() ? "{}" : tool.parameters_json)},
  };
  return node;
}

json build_chat_payload(const ChatRequest& request, bool stream) {
  json payload;
  payload["model"] = request.model.str();
  payload["stream"] = stream;

  json messages = json::array();
  for (const auto& message : request.messages) {
    messages.push_back(message_to_json(message));
  }
  payload["messages"] = std::move(messages);

  if (!request.tools.empty()) {
    json tools = json::array();
    for (const auto& tool : request.tools) {
      tools.push_back(tool_to_json(tool));
    }
    payload["tools"] = std::move(tools);
  }

  payload["options"] = {
      {"temperature", request.sampling.temperature},
      {"top_p", request.sampling.top_p},
      {"num_predict", request.sampling.max_tokens},
  };

  if (request.sampling.stop) {
    payload["options"]["stop"] = *request.sampling.stop;
  }

  return payload;
}

Capabilities infer_capabilities(std::string_view model_name) {
  Capabilities caps = capability(Capability::Chat);
  std::string lower(model_name);
  for (auto& ch : lower) {
    ch = static_cast<char>(std::tolower(static_cast<unsigned char>(ch)));
  }
  if (lower.find("embed") != std::string::npos) {
    return capability(Capability::Embedding);
  }
  caps = caps | Capability::ToolUse | Capability::Code;
  if (lower.find("reason") != std::string::npos || lower.find("r1") != std::string::npos) {
    caps = caps | Capability::Reasoning;
  }
  return caps;
}

HttpResponse perform_post(CURL* handle, const std::string& url, const std::string& body,
                          bool stream, StreamDataCallback on_data) {
  HttpResponse response;
  std::string response_body;

  curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
  curl_easy_setopt(handle, CURLOPT_POST, 1L);
  curl_easy_setopt(handle, CURLOPT_POSTFIELDS, body.c_str());
  curl_easy_setopt(handle, CURLOPT_POSTFIELDSIZE, static_cast<long>(body.size()));

  struct curl_slist* headers = nullptr;
  headers = curl_slist_append(headers, "Content-Type: application/json");
  curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);

  if (stream && on_data) {
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, stream_callback);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &on_data);
  } else {
    curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(handle, CURLOPT_WRITEDATA, &response_body);
  }

  const CURLcode code = curl_easy_perform(handle);
  if (code != CURLE_OK) {
    response.error = curl_easy_strerror(code);
  } else {
    curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &response.status_code);
    response.body = std::move(response_body);
  }

  curl_slist_free_all(headers);
  return response;
}

ModelInfo default_fake_model() {
  return ModelInfo{
      .id = ModelId{"fake-model"},
      .display_name = "fake-model",
      .capabilities = capability(Capability::Chat) | Capability::ToolUse | Capability::Code,
      .context_length = 8192,
      .quality_score = 0.8f,
      .speed_score = 1.0f,
      .memory_mb = 2048,
      .cost_score = 0.2f,
  };
}

}  // namespace

HttpResponse CurlHttpTransport::post(const std::string& url, const std::string& json_body,
                                     bool stream, StreamDataCallback on_data) {
  CURL* handle = curl_easy_init();
  if (handle == nullptr) {
    return HttpResponse{.status_code = 0, .body = {}, .error = "curl_easy_init failed"};
  }

  const auto response = perform_post(handle, url, json_body, stream, std::move(on_data));
  curl_easy_cleanup(handle);
  return response;
}

HttpResponse CurlHttpTransport::get(const std::string& url) {
  CURL* handle = curl_easy_init();
  if (handle == nullptr) {
    return HttpResponse{.status_code = 0, .body = {}, .error = "curl_easy_init failed"};
  }

  HttpResponse response;
  std::string response_body;
  curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
  curl_easy_setopt(handle, CURLOPT_HTTPGET, 1L);
  curl_easy_setopt(handle, CURLOPT_WRITEFUNCTION, write_callback);
  curl_easy_setopt(handle, CURLOPT_WRITEDATA, &response_body);

  const CURLcode code = curl_easy_perform(handle);
  if (code != CURLE_OK) {
    response.error = curl_easy_strerror(code);
  } else {
    curl_easy_getinfo(handle, CURLINFO_RESPONSE_CODE, &response.status_code);
    response.body = std::move(response_body);
  }

  curl_easy_cleanup(handle);
  return response;
}

OllamaBackend::OllamaBackend(OllamaConfig config, std::shared_ptr<HttpTransport> transport)
    : config_(std::move(config)),
      transport_(transport ? std::move(transport)
                           : std::make_shared<CurlHttpTransport>()) {}

ChatResponse OllamaBackend::chat(const ChatRequest& request, StreamCallback stream) {
  const bool streaming = stream != nullptr || request.stream;
  const auto payload = build_chat_payload(request, streaming);

  std::vector<ChatChunk> chunks;
  std::string buffered;

  StreamDataCallback on_data;
  if (streaming) {
    on_data = [&](std::string_view data) {
      buffered.append(data.data(), data.size());
      std::size_t pos = 0;
      while ((pos = buffered.find('\n')) != std::string::npos) {
        const auto line = std::string_view(buffered.data(), pos);
        if (auto chunk = parse_stream_line(line)) {
          if (stream) {
            stream(*chunk);
          }
          chunks.push_back(*chunk);
        }
        buffered.erase(0, pos + 1);
      }
      return true;
    };
  }

  const auto url = config_.base_url + "/api/chat";
  const auto response = transport_->post(url, payload.dump(), streaming, on_data);

  if (!response.error.empty()) {
    throw std::runtime_error("ollama chat request failed: " + response.error);
  }
  if (response.status_code < 200 || response.status_code >= 300) {
    throw std::runtime_error("ollama chat HTTP " + std::to_string(response.status_code) + ": " +
                             response.body);
  }

  if (chunks.empty()) {
    const auto parsed = parse_stream(response.body);
    chunks = std::move(parsed.chunks);
    if (stream) {
      for (const auto& chunk : chunks) {
        stream(chunk);
      }
    }
  } else if (!buffered.empty()) {
    if (auto chunk = parse_stream_line(buffered)) {
      if (stream) {
        stream(*chunk);
      }
      chunks.push_back(*chunk);
    }
  }

  auto aggregated = aggregate_chunks(chunks);
  if (aggregated.model.empty()) {
    aggregated.model = request.model.str();
  }
  return aggregated;
}

std::vector<ModelInfo> OllamaBackend::list_models() {
  const auto url = config_.base_url + "/api/tags";
  const auto response = transport_->get(url);
  if (!response.error.empty()) {
    throw std::runtime_error("ollama list_models failed: " + response.error);
  }
  if (response.status_code < 200 || response.status_code >= 300) {
    throw std::runtime_error("ollama list_models HTTP " + std::to_string(response.status_code));
  }

  const json root = json::parse(response.body);
  std::vector<ModelInfo> models;
  if (!root.contains("models") || !root["models"].is_array()) {
    return models;
  }

  for (const auto& node : root["models"]) {
    if (!node.contains("name") || !node["name"].is_string()) {
      continue;
    }
    const auto name = node["name"].get<std::string>();
    ModelInfo info;
    info.id = ModelId{name};
    info.display_name = name;
    info.capabilities = infer_capabilities(name);
    if (node.contains("size") && node["size"].is_number_unsigned()) {
      info.memory_mb = static_cast<uint32_t>(node["size"].get<uint64_t>() / (1024 * 1024));
    }
    models.push_back(std::move(info));
  }
  return models;
}

void OllamaBackend::pull_model(const ModelId& model) {
  const auto url = config_.base_url + "/api/pull";
  const json payload{{"name", model.str()}, {"stream", false}};
  const auto response = transport_->post(url, payload.dump(), false, nullptr);
  if (!response.error.empty()) {
    throw std::runtime_error("ollama pull failed: " + response.error);
  }
  if (response.status_code < 200 || response.status_code >= 300) {
    throw std::runtime_error("ollama pull HTTP " + std::to_string(response.status_code));
  }
}

EmbeddingResult OllamaBackend::embed(const EmbeddingRequest& request) {
  const auto url = config_.base_url + "/api/embeddings";
  const json payload{{"model", request.model.str()}, {"prompt", request.text}};
  const auto response = transport_->post(url, payload.dump(), false, nullptr);
  if (!response.error.empty()) {
    throw std::runtime_error("ollama embed failed: " + response.error);
  }
  if (response.status_code < 200 || response.status_code >= 300) {
    throw std::runtime_error("ollama embed HTTP " + std::to_string(response.status_code));
  }

  const json root = json::parse(response.body);
  EmbeddingResult result;
  result.model = request.model.str();
  if (root.contains("embedding") && root["embedding"].is_array()) {
    for (const auto& value : root["embedding"]) {
      result.embedding.push_back(value.get<float>());
    }
  }
  return result;
}

FakeModelBackend::FakeModelBackend(std::vector<ScriptedResponse> script,
                                   std::vector<ModelInfo> models)
    : models_(std::move(models)) {
  for (auto& item : script) {
    script_.push(std::move(item));
  }
  if (models_.empty()) {
    models_.push_back(default_fake_model());
  }
}

void FakeModelBackend::enqueue(ScriptedResponse response) {
  script_.push(std::move(response));
}

ChatResponse FakeModelBackend::chat(const ChatRequest& request, StreamCallback stream) {
  ++chat_calls_;

  if (!script_.empty()) {
    auto scripted = std::move(script_.front());
    script_.pop();
    for (const auto& chunk : scripted.stream_chunks) {
      if (stream) {
        stream(chunk);
      }
    }
    if (scripted.response.model.empty()) {
      scripted.response.model =
          request.model.str().empty() ? "fake-model" : request.model.str();
    }
    return scripted.response;
  }

  ChatResponse response;
  response.model = request.model.str().empty() ? "fake-model" : request.model.str();
  response.message.role = MessageRole::Assistant;
  response.message.content = "fake response";
  if (stream) {
    stream(ChatChunk{.content_delta = response.message.content, .done = true});
  }
  return response;
}

std::vector<ModelInfo> FakeModelBackend::list_models() { return models_; }

void FakeModelBackend::pull_model(const ModelId& /*model*/) {}

EmbeddingResult FakeModelBackend::embed(const EmbeddingRequest& request) {
  EmbeddingResult result;
  result.model = request.model.str();
  result.embedding = {0.1f, 0.2f, 0.3f};
  return result;
}

bool ollama_reachable(const std::string& base_url) {
  CurlHttpTransport transport;
  const auto response = transport.get(base_url + "/api/tags");
  return response.error.empty() && response.status_code >= 200 && response.status_code < 300;
}

}  // namespace localagent::ollama
