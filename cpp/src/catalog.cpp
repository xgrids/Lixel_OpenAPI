#include "lixel/openapi/catalog.h"

#include <cstring>

namespace lixel {
namespace openapi {

namespace {

const ApiInterface kTable[] = {
#define LIXEL_API(key, rt, rm, st, sm, pt, pm) {key, rt, rm, st, sm, pt, pm},
#include "catalog_table.inc"
#undef LIXEL_API
};

bool Same(const char* a, const char* b) {
  return a != nullptr && b != nullptr && std::strcmp(a, b) == 0;
}

}  // namespace

const ApiInterface* Interfaces(std::size_t* count) noexcept {
  if (count != nullptr) *count = sizeof(kTable) / sizeof(kTable[0]);
  return kTable;
}

const ApiInterface* FindInterface(const char* key) noexcept {
  for (const ApiInterface& i : kTable) {
    if (Same(i.key, key)) return &i;
  }
  return nullptr;
}

const ApiInterface* FindInterfaceByTopic(const char* topic) noexcept {
  for (const ApiInterface& i : kTable) {
    if (Same(i.request_topic, topic) || Same(i.response_topic, topic) ||
        Same(i.report_topic, topic)) {
      return &i;
    }
  }
  return nullptr;
}

const char* MessageForTopic(const char* topic) noexcept {
  for (const ApiInterface& i : kTable) {
    if (Same(i.request_topic, topic)) return i.request_message;
    if (Same(i.response_topic, topic)) return i.response_message;
    if (Same(i.report_topic, topic)) return i.report_message;
  }
  return nullptr;
}

}  // namespace openapi
}  // namespace lixel
