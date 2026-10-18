#include <iostream>
#include <iterator>
#include <json-c/json.h>
#include <licensebox/client.hpp>
#include <memory>

using Json = std::unique_ptr<json_object, decltype(&json_object_put)>;
json_object *get(json_object *o, const char *name) {
  json_object *v = nullptr;
  json_object_object_get_ex(o, name, &v);
  return v;
}
std::string text(json_object *o, const char *name) {
  auto value = json_object_get_string(get(o, name));
  return value ? value : "";
}
int main() {
  std::string input((std::istreambuf_iterator<char>(std::cin)), {});
  Json request(json_tokener_parse(input.c_str()), json_object_put);
  if (!request)
    return 2;
  auto *o = request.get();
  auto op = text(o, "op");
  char *raw = nullptr;
  Json value(nullptr, json_object_put);
  std::string error;
  lb_public_key keys[64];
  std::size_t count = 0;
  auto *key_object = get(o, "keys");
  if (key_object) {
    json_object_object_foreach(key_object, k, v) {
      if (count >= 64)
        return 2;
      keys[count++] = {k, json_object_get_string(v)};
    }
  }
  try {
    if (op == "derive" || op == "native") {
      char binding[128];
      auto result = op == "derive"
                        ? lb_derive_binding(text(o, "product").c_str(),
                                            text(o, "source").c_str(),
                                            text(o, "raw_id").c_str(), binding,
                                            sizeof(binding))
                        : lb_native_binding(text(o, "product").c_str(), binding,
                                            sizeof(binding));
      if (result != LB_OK)
        throw licensebox::Error(result, lb_result_code(result),
                                "binding failed");
      value.reset(json_object_new_string(binding));
    } else if (op == "verify") {
      auto result = lb_verify_lease(
          text(o, "token").c_str(), text(o, "product").c_str(), keys, count,
          json_object_get_double(get(o, "now")), &raw);
      if (result != LB_OK)
        throw licensebox::Error(result, lb_result_code(result),
                                "verification failed");
      value.reset(json_tokener_parse(raw));
      lb_free(raw);
      raw = nullptr;
    } else {
      licensebox::Config config;
      config.product = text(o, "product");
      config.server = text(o, "server");
      config.directory = text(o, "directory");
      config.version = "conformance";
      for (std::size_t i = 0; i < count; i++)
        config.keys.emplace(keys[i].id, keys[i].public_key);
      config.clock = [&] { return json_object_get_double(get(o, "now")); };
      config.on_sign_in = [](const auto &, const auto &) {};
      if (get(o, "binding"))
        config.machine_binding = [&] { return text(o, "binding"); };
      licensebox::Client client(std::move(config));
      std::string output;
      if (op == "lease")
        output = client.lease();
      else if (op == "require")
        output = client.require(text(o, "module").empty() ? "core"
                                                          : text(o, "module"));
      else if (op == "repeat") {
        client.require("core");
        output = client.require("core");
      } else if (op == "status")
        output = client.status();
      else if (op == "sign_in")
        output = client.sign_in(3);
      else if (op == "logout")
        client.logout();
      else if (op == "modules")
        output = client.modules();
      else if (op == "requests")
        output = client.requests();
      else if (op == "request")
        output = client.request_access({"export"}, "café – test");
      else if (op == "access-link")
        value.reset(json_object_new_string(client.access_link().c_str()));
      else if (op == "record") {
        client.record("export", true, .125);
        client.record("export", false, .025);
      } else
        return 2;
      if (!output.empty())
        value.reset(json_tokener_parse(output.c_str()));
    }
  } catch (const licensebox::Error &e) {
    error = e.code;
  }
  Json answer(json_object_new_object(), json_object_put);
  json_object_object_add(answer.get(), "ok",
                         json_object_new_boolean(error.empty()));
  if (!error.empty())
    json_object_object_add(answer.get(), "error",
                           json_object_new_string(error.c_str()));
  else
    json_object_object_add(answer.get(), "value", value.release());
  std::cout << json_object_to_json_string_ext(answer.get(),
                                              JSON_C_TO_STRING_PLAIN)
            << '\n';
}
