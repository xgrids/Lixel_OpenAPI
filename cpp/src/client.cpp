#include "lixel/openapi/client.h"

#include <google/protobuf/descriptor.h>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <list>
#include <map>
#include <mutex>
#include <set>
#include <utility>
#include <vector>

#include "lixel/openapi/codec.h"
#include "lixel/openapi/codes.h"
#include "openapi/aiot_msgs.pb.h"

namespace lixel {
namespace openapi {

namespace pb = google::protobuf;

namespace {

// True while the current thread is running one of our handlers.
thread_local int t_callback_depth = 0;

struct CallbackScope {
  CallbackScope() { ++t_callback_depth; }
  ~CallbackScope() { --t_callback_depth; }
};

// Finds the "header" field of a message, or nullptr when it has none.
const pb::FieldDescriptor* HeaderField(const pb::Descriptor* d) {
  const pb::FieldDescriptor* f = d->FindFieldByName("header");
  if (f == nullptr || f->cpp_type() != pb::FieldDescriptor::CPPTYPE_MESSAGE || f->is_repeated())
    return nullptr;
  if (f->message_type()->full_name() != "openapi.aiot_msgs.Header") return nullptr;
  return f;
}

const ::openapi::aiot_msgs::Header* ReadHeader(const pb::Message& m) {
  const pb::FieldDescriptor* f = HeaderField(m.GetDescriptor());
  if (f == nullptr) return nullptr;
  const pb::Reflection* r = m.GetReflection();
  if (!r->HasField(m, f)) return nullptr;
  return static_cast<const ::openapi::aiot_msgs::Header*>(&r->GetMessage(m, f));
}

// Reads Error.code and Error.desc of a response. False when the message has no Error field.
bool ReadError(const pb::Message& m, std::uint32_t* code, std::string* desc) {
  const pb::FieldDescriptor* f = m.GetDescriptor()->FindFieldByName("error");
  if (f == nullptr || f->cpp_type() != pb::FieldDescriptor::CPPTYPE_MESSAGE) return false;
  const pb::Reflection* r = m.GetReflection();
  if (!r->HasField(m, f)) return false;
  const auto& e = static_cast<const ::openapi::aiot_msgs::Error&>(r->GetMessage(m, f));
  *code = e.code();
  *desc = e.desc();
  return true;
}

}  // namespace

namespace {

// Splits "a/b//c" into {"a", "b", "", "c"}. An empty string gives one empty level.
std::vector<std::string> Levels(const std::string& s) {
  std::vector<std::string> out;
  std::size_t start = 0;
  while (true) {
    const std::size_t slash = s.find('/', start);
    if (slash == std::string::npos) {
      out.push_back(s.substr(start));
      return out;
    }
    out.push_back(s.substr(start, slash - start));
    start = slash + 1;
  }
}

}  // namespace

bool TopicMatches(const std::string& filter, const std::string& topic) {
  const std::vector<std::string> f = Levels(filter);
  const std::vector<std::string> t = Levels(topic);
  for (std::size_t i = 0; i < f.size(); ++i) {
    if (f[i] == "#")
      return i + 1 == f.size();       // '#' is only valid as the last level; it matches the rest
    if (i >= t.size()) return false;  // the topic has fewer levels than the filter
    if (f[i] != "+" && f[i] != t[i]) return false;
  }
  return f.size() == t.size();
}

// One request waiting for its response.
struct Waiter {
  std::mutex mu;
  std::condition_variable cv;
  bool done = false;
  bool aborted = false;  // connection closed while waiting
  std::unique_ptr<pb::Message>
      received;  // owned here, so a late response never writes to the caller
  std::uint32_t seq = 0;
};

struct Client::Impl {
  ClientOptions options;
  std::unique_ptr<ITransport> transport;

  mutable std::mutex mu;  // guards everything below
  struct Sub {
    SubscriptionId id;
    std::string filter;
    RawHandler handler;
  };
  SubscriptionId next_sub_id = 0;
  std::list<Sub> subs;
  std::set<std::string> broker_subscribed;  // filters the broker has acknowledged
  // key: response topic, value: waiters on that topic
  std::multimap<std::string, std::shared_ptr<Waiter>> waiters;
  std::uint32_t next_seq = 0;
  bool connected_flag = false;

  std::atomic<std::uint64_t> dropped{0};
  std::atomic<bool> shutting_down{false};

  std::uint32_t NextSeq() {
    // caller holds mu
    ++next_seq;
    if (next_seq == 0) ++next_seq;  // 0 is the proto3 default and cannot be told from "not set"
    return next_seq;
  }

  // Fills header.seq / openapi_key / device_id of a copy of `src`. Returns nullptr on failure.
  std::unique_ptr<pb::Message> Stamp(const pb::Message& src, std::uint32_t seq) const {
    std::unique_ptr<pb::Message> copy(src.New());
    copy->CopyFrom(src);
    const pb::FieldDescriptor* f = HeaderField(copy->GetDescriptor());
    if (f == nullptr) return copy;  // a message without a header is sent as it is
    auto* header = static_cast<::openapi::aiot_msgs::Header*>(
        copy->GetReflection()->MutableMessage(copy.get(), f));
    if (seq != 0) header->set_seq(seq);
    if (!options.openapi_key.empty()) header->set_openapi_key(options.openapi_key);
    if (!options.device_id.empty()) header->set_device_id(options.device_id);
    return copy;
  }
};

Client::Client(ClientOptions options) : Client(std::move(options), MakePahoTransport()) {}

Client::Client(ClientOptions options, std::unique_ptr<ITransport> transport) : impl_(new Impl) {
  impl_->options = std::move(options);
  impl_->transport = std::move(transport);
  impl_->transport->SetListener(this);
}

Client::~Client() {
  Disconnect();
  // After Disconnect the transport makes no more listener calls.
  impl_->transport->SetListener(nullptr);
}

Result Client::Connect() {
  if (t_callback_depth > 0)
    return Result::Make(Errc::CalledFromCallback, "Connect called from a handler");
  if (impl_->transport->IsConnected()) return Result();
  Result r = impl_->transport->Connect(impl_->options.transport);
  if (!r.ok()) return r;
  {
    std::lock_guard<std::mutex> lock(impl_->mu);
    impl_->connected_flag = true;
    impl_->broker_subscribed.clear();  // a new session starts without subscriptions
  }
  // Restore the subscriptions registered before (for example by an earlier Connect).
  std::vector<std::string> filters;
  {
    std::lock_guard<std::mutex> lock(impl_->mu);
    for (const auto& s : impl_->subs) filters.push_back(s.filter);
  }
  for (const std::string& f : filters) {
    Result sr = impl_->transport->Subscribe(f, impl_->options.subscribe_timeout);
    if (sr.ok()) {
      std::lock_guard<std::mutex> lock(impl_->mu);
      impl_->broker_subscribed.insert(f);
    }
  }
  return Result();
}

void Client::Disconnect() {
  if (t_callback_depth > 0) return;  // not allowed from a handler; ignore rather than deadlock
  {
    std::lock_guard<std::mutex> lock(impl_->mu);
    impl_->connected_flag = false;
    for (auto& w : impl_->waiters) {
      std::lock_guard<std::mutex> wl(w.second->mu);
      w.second->aborted = true;
      w.second->done = true;
      w.second->cv.notify_all();
    }
  }
  impl_->transport->Disconnect();
  std::lock_guard<std::mutex> lock(impl_->mu);
  impl_->broker_subscribed.clear();
}

bool Client::IsConnected() const { return impl_->transport->IsConnected(); }

Result Client::Request(const std::string& request_topic, const pb::Message& request,
                       const std::string& response_topic, pb::Message* response,
                       std::chrono::milliseconds timeout) {
  if (t_callback_depth > 0) {
    return Result::Make(Errc::CalledFromCallback, "Request called from a subscription handler");
  }
  if (response == nullptr || request_topic.empty() || response_topic.empty()) {
    return Result::Make(Errc::InvalidArgument, "response is null or a topic is empty");
  }
  if (!impl_->transport->IsConnected()) return Result::Make(Errc::NotConnected, "not connected");
  if (timeout.count() <= 0) timeout = impl_->options.request_timeout;

  // The response topic must be subscribed (and acknowledged) before the request goes out.
  {
    bool need = false;
    {
      std::lock_guard<std::mutex> lock(impl_->mu);
      need = impl_->broker_subscribed.count(response_topic) == 0;
    }
    if (need) {
      Result sr = impl_->transport->Subscribe(response_topic, impl_->options.subscribe_timeout);
      if (!sr.ok()) return sr;
      std::lock_guard<std::mutex> lock(impl_->mu);
      impl_->broker_subscribed.insert(response_topic);
    }
  }

  auto waiter = std::make_shared<Waiter>();
  waiter->received.reset(response->New());
  std::uint32_t seq = 0;
  {
    std::lock_guard<std::mutex> lock(impl_->mu);
    seq = impl_->NextSeq();
    waiter->seq = seq;
    impl_->waiters.emplace(response_topic, waiter);
  }
  auto remove_waiter = [&]() {
    std::lock_guard<std::mutex> lock(impl_->mu);
    auto range = impl_->waiters.equal_range(response_topic);
    for (auto it = range.first; it != range.second; ++it) {
      if (it->second == waiter) {
        impl_->waiters.erase(it);
        break;
      }
    }
  };

  std::unique_ptr<pb::Message> stamped = impl_->Stamp(request, seq);
  std::string payload;
  if (!stamped->SerializeToString(&payload)) {
    remove_waiter();
    return Result::Make(Errc::EncodeFailed, "request could not be serialized");
  }
  Result pr = impl_->transport->Publish(request_topic, payload);
  if (!pr.ok()) {
    remove_waiter();
    return pr;
  }

  std::unique_lock<std::mutex> wl(waiter->mu);
  const bool got = waiter->cv.wait_for(wl, timeout, [&] { return waiter->done; });
  const bool aborted = waiter->aborted;
  std::unique_ptr<pb::Message> received;
  if (got && !aborted) received = std::move(waiter->received);
  wl.unlock();
  remove_waiter();

  if (aborted)
    return Result::Make(Errc::NotConnected, "disconnected while waiting for the response");
  if (!got || !received) {
    return Result::Make(Errc::RequestTimeout,
                        "no response on " + response_topic + " within the timeout");
  }
  response->CopyFrom(*received);

  std::uint32_t code = 0;
  std::string desc;
  if (ReadError(*response, &code, &desc)) {
    Result r;
    r.device_code = code;
    r.desc = desc;
    if (!IsSuccessCode(code)) r.code = Errc::DeviceError;
    return r;
  }
  return Result();  // a response without an Error field
}

Result Client::Publish(const std::string& topic, const pb::Message& message) {
  if (topic.empty()) return Result::Make(Errc::InvalidArgument, "topic is empty");
  std::uint32_t seq = 0;
  {
    std::lock_guard<std::mutex> lock(impl_->mu);
    seq = impl_->NextSeq();
  }
  std::unique_ptr<pb::Message> stamped = impl_->Stamp(message, seq);
  std::string payload;
  if (!stamped->SerializeToString(&payload))
    return Result::Make(Errc::EncodeFailed, "message could not be serialized");
  return impl_->transport->Publish(topic, payload);
}

Result Client::PublishRaw(const std::string& topic, const std::string& payload) {
  if (topic.empty()) return Result::Make(Errc::InvalidArgument, "topic is empty");
  return impl_->transport->Publish(topic, payload);
}

Result Client::Subscribe(const std::string& topic_filter, RawHandler handler, SubscriptionId* id) {
  if (id != nullptr) *id = 0;
  if (topic_filter.empty() || !handler)
    return Result::Make(Errc::InvalidArgument, "empty filter or handler");
  if (!impl_->transport->IsConnected()) return Result::Make(Errc::NotConnected, "not connected");
  bool need = false;
  SubscriptionId new_id = 0;
  {
    std::lock_guard<std::mutex> lock(impl_->mu);
    new_id = ++impl_->next_sub_id;
    impl_->subs.push_back({new_id, topic_filter, std::move(handler)});
    need = impl_->broker_subscribed.count(topic_filter) == 0;
  }
  if (!need) {
    if (id != nullptr) *id = new_id;
    return Result();
  }
  Result r = impl_->transport->Subscribe(topic_filter, impl_->options.subscribe_timeout);
  std::lock_guard<std::mutex> lock(impl_->mu);
  if (r.ok()) {
    impl_->broker_subscribed.insert(topic_filter);
    if (id != nullptr) *id = new_id;
  } else {
    // keep the registration out of the table when the broker did not accept it
    impl_->subs.remove_if([&](const Impl::Sub& sub) { return sub.id == new_id; });
  }
  return r;
}

Result Client::SubscribeMessage(const std::string& topic_filter, const std::string& message_name,
                                MessageHandler handler, SubscriptionId* id) {
  if (id != nullptr) *id = 0;
  if (!handler) return Result::Make(Errc::InvalidArgument, "handler is empty");
  if (NewMessage(message_name) == nullptr) {
    return Result::Make(Errc::InvalidArgument, "unknown message name: " + message_name);
  }
  Impl* impl = impl_.get();
  return Subscribe(
      topic_filter,
      [impl, message_name, handler](const std::string& topic, const std::string& payload) {
        std::unique_ptr<pb::Message> msg = NewMessage(message_name);
        if (!msg || !msg->ParseFromString(payload)) {
          ++impl->dropped;
          return;
        }
        handler(topic, *msg);
      },
      id);
}

Result Client::Unsubscribe(const std::string& topic_filter) {
  {
    std::lock_guard<std::mutex> lock(impl_->mu);
    impl_->subs.remove_if([&](const Impl::Sub& s) { return s.filter == topic_filter; });
    impl_->broker_subscribed.erase(topic_filter);
  }
  if (!impl_->transport->IsConnected()) return Result();
  return impl_->transport->Unsubscribe(topic_filter);
}

Result Client::RemoveSubscription(SubscriptionId id) {
  std::string filter;
  {
    std::lock_guard<std::mutex> lock(impl_->mu);
    auto it = std::find_if(impl_->subs.begin(), impl_->subs.end(),
                           [&](const Impl::Sub& sub) { return sub.id == id; });
    if (it == impl_->subs.end()) return Result();
    filter = it->filter;
    impl_->subs.erase(it);
    const bool shared = std::any_of(impl_->subs.begin(), impl_->subs.end(),
                                    [&](const Impl::Sub& sub) { return sub.filter == filter; });
    if (shared) return Result();
    impl_->broker_subscribed.erase(filter);
  }
  if (!impl_->transport->IsConnected()) return Result();
  return impl_->transport->Unsubscribe(filter);
}

std::uint64_t Client::DroppedMessages() const { return impl_->dropped.load(); }

// --- events from the transport ------------------------------------------------------------

void Client::OnMessage(const std::string& topic, const std::string& payload) {
  CallbackScope scope;

  // 1. Hand the message to a request that waits for it. The waiter is parsed on its own copy.
  std::vector<std::shared_ptr<Waiter>> candidates;
  {
    std::lock_guard<std::mutex> lock(impl_->mu);
    auto range = impl_->waiters.equal_range(topic);
    for (auto it = range.first; it != range.second; ++it) candidates.push_back(it->second);
  }
  for (const auto& w : candidates) {
    std::unique_ptr<pb::Message> parsed;
    {
      std::lock_guard<std::mutex> wl(w->mu);
      if (w->done) continue;
      parsed.reset(w->received->New());
    }
    if (!parsed->ParseFromString(payload)) {
      ++impl_->dropped;
      continue;
    }
    const ::openapi::aiot_msgs::Header* h = ReadHeader(*parsed);
    if (h == nullptr || h->seq() != w->seq) continue;  // another request's response, or a late one
    std::lock_guard<std::mutex> wl(w->mu);
    if (w->done) continue;
    w->received = std::move(parsed);
    w->done = true;
    w->cv.notify_all();
  }

  // 2. Hand the message to the subscription handlers. Copy them out first so that no lock is
  //    held while user code runs.
  std::vector<RawHandler> handlers;
  {
    std::lock_guard<std::mutex> lock(impl_->mu);
    for (const auto& s : impl_->subs) {
      if (TopicMatches(s.filter, topic)) handlers.push_back(s.handler);
    }
  }
  for (const RawHandler& h : handlers) h(topic, payload);
}

void Client::OnConnected(bool reconnected) {
  CallbackScope scope;
  if (!reconnected) return;
  // The broker forgot our subscriptions with the old session: subscribe again. Done on this
  // thread; the transport's Subscribe must be callable from here without waiting for a callback
  // that this same thread would have to deliver, so it only queues the request.
  std::vector<std::string> filters;
  {
    std::lock_guard<std::mutex> lock(impl_->mu);
    impl_->broker_subscribed.clear();
    for (const auto& s : impl_->subs) filters.push_back(s.filter);
    for (const auto& w : impl_->waiters) filters.push_back(w.first);
  }
  std::sort(filters.begin(), filters.end());
  filters.erase(std::unique(filters.begin(), filters.end()), filters.end());
  for (const std::string& f : filters) {
    Result r = impl_->transport->Subscribe(f, std::chrono::milliseconds(0));
    if (r.ok()) {
      std::lock_guard<std::mutex> lock(impl_->mu);
      impl_->broker_subscribed.insert(f);
    }
  }
}

void Client::OnConnectionLost(const std::string&) {
  CallbackScope scope;
  std::lock_guard<std::mutex> lock(impl_->mu);
  impl_->broker_subscribed.clear();
}

}  // namespace openapi
}  // namespace lixel
