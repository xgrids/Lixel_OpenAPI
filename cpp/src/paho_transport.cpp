// ITransport on top of paho.mqtt.c (asynchronous API).
//
// paho calls our callbacks from its own thread. Everything that thread touches is reachable
// through State, which is shared with the callbacks, so a callback that arrives after the
// transport was destroyed finds a valid object that says "closed".

#include <MQTTAsync.h>

#include <atomic>
#include <condition_variable>
#include <cstdio>
#include <cstring>
#include <memory>
#include <mutex>
#include <random>
#include <shared_mutex>
#include <string>

#include "lixel/openapi/transport.h"

namespace lixel {
namespace openapi {

namespace {

// An operation the caller waits for (connect, subscribe, disconnect).
struct Op {
  std::mutex mu;
  std::condition_variable cv;
  bool done = false;
  bool ok = false;
  std::string reason;
  void Finish(bool success, const std::string& why) {
    std::lock_guard<std::mutex> lock(mu);
    if (done) return;
    done = true;
    ok = success;
    reason = why;
    cv.notify_all();
  }
};

std::string Reason(MQTTAsync_failureData* d, const char* fallback) {
  if (d == nullptr) return fallback;
  std::string s = d->message != nullptr ? d->message : fallback;
  char code[32];
  std::snprintf(code, sizeof(code), " (code %d)", d->code);
  return s + code;
}

// Callback context for one operation: keeps the Op alive until paho has called back.
struct OpContext {
  std::shared_ptr<Op> op;
};

void OnOpSuccess(void* context, MQTTAsync_successData*) {
  std::unique_ptr<OpContext> ctx(static_cast<OpContext*>(context));
  ctx->op->Finish(true, "");
}
void OnOpFailure(void* context, MQTTAsync_failureData* data) {
  std::unique_ptr<OpContext> ctx(static_cast<OpContext*>(context));
  ctx->op->Finish(false, Reason(data, "operation failed"));
}

std::string MakeClientId() {
  std::random_device rd;
  char buf[48];
  std::snprintf(buf, sizeof(buf), "lixel-cpp-%08x%08x", rd(), rd());
  return buf;
}

struct State {
  std::mutex mu;  // guards listener and closed
  ITransportListener* listener = nullptr;
  bool closed = true;  // true: do not call the listener
  std::atomic<bool> connected{false};
  std::atomic<bool> ever_connected{false};
};

class PahoTransport : public ITransport {
 public:
  PahoTransport() : state_(std::make_shared<State>()) {}
  ~PahoTransport() override { Disconnect(); }

  void SetListener(ITransportListener* l) override {
    std::lock_guard<std::mutex> lock(state_->mu);
    state_->listener = l;
  }

  Result Connect(const TransportOptions& o) override {
    std::lock_guard<std::mutex> serialize(api_mu_);
    if (handle_ != nullptr) return Result();  // already created and connected, or connecting

    const std::string uri =
        std::string(o.use_tls ? "ssl://" : "tcp://") + o.host + ":" + std::to_string(o.port);
    const std::string id = o.client_id.empty() ? MakeClientId() : o.client_id;

    MQTTAsync handle = nullptr;
    int rc =
        MQTTAsync_create(&handle, uri.c_str(), id.c_str(), MQTTCLIENT_PERSISTENCE_NONE, nullptr);
    if (rc != MQTTASYNC_SUCCESS) {
      return Result::Make(Errc::ConnectFailed,
                          "MQTTAsync_create failed, code " + std::to_string(rc));
    }
    {
      std::lock_guard<std::mutex> lock(state_->mu);
      state_->closed = false;
    }
    // The callbacks get a pointer to a heap object that lives as long as the handle.
    ctx_.reset(new std::shared_ptr<State>(state_));
    MQTTAsync_setCallbacks(handle, ctx_.get(), &PahoTransport::OnLost, &PahoTransport::OnArrived,
                           nullptr);
    MQTTAsync_setConnected(handle, ctx_.get(), &PahoTransport::OnConnectedCb);

    // The strings in these structures must stay valid until MQTTAsync_connect returns.
    MQTTAsync_connectOptions conn = MQTTAsync_connectOptions_initializer;
    MQTTAsync_SSLOptions ssl = MQTTAsync_SSLOptions_initializer;
    conn.keepAliveInterval = static_cast<int>(o.keep_alive.count() / 1000);
    conn.cleansession = 1;
    conn.MQTTVersion = MQTTVERSION_3_1_1;
    conn.automaticReconnect = 1;
    conn.minRetryInterval = 1;
    conn.maxRetryInterval = 10;
    conn.connectTimeout = static_cast<int>(
        o.connect_timeout.count() / 1000 > 0 ? o.connect_timeout.count() / 1000 : 1);
    if (!o.username.empty()) conn.username = o.username.c_str();
    if (!o.password.empty()) conn.password = o.password.c_str();
    if (o.use_tls) {
      ssl.enableServerCertAuth = o.verify_server ? 1 : 0;
      ssl.verify = o.verify_server ? 1 : 0;  // also check that the certificate names this host
      if (!o.ca_file.empty()) ssl.trustStore = o.ca_file.c_str();
      conn.ssl = &ssl;
    }
    auto op = std::make_shared<Op>();
    conn.onSuccess = &OnOpSuccess;
    conn.onFailure = &OnOpFailure;
    conn.context = new OpContext{op};

    rc = MQTTAsync_connect(handle, &conn);
    if (rc != MQTTASYNC_SUCCESS) {
      delete static_cast<OpContext*>(conn.context);
      handle_ = handle;  // let DestroyLocked release it
      DestroyLocked();
      return Result::Make(Errc::ConnectFailed,
                          "MQTTAsync_connect failed, code " + std::to_string(rc));
    }
    {
      std::unique_lock<std::shared_timed_mutex> exclusive(handle_mu_);
      handle_ = handle;
    }

    std::unique_lock<std::mutex> lock(op->mu);
    // Wait a little longer than paho's own connect timeout so that its failure report wins.
    const bool finished = op->cv.wait_for(lock, o.connect_timeout + std::chrono::milliseconds(1500),
                                          [&] { return op->done; });
    if (!finished) {
      lock.unlock();
      AbortLocked();
      return Result::Make(Errc::ConnectTimeout, "no answer from " + uri);
    }
    if (!op->ok) {
      const std::string why = op->reason;
      lock.unlock();
      AbortLocked();
      return Result::Make(Errc::ConnectFailed, why);
    }
    state_->connected = true;
    state_->ever_connected = true;
    return Result();
  }

  void Disconnect() override {
    std::lock_guard<std::mutex> serialize(api_mu_);
    if (handle_ == nullptr) return;
    state_->connected = false;
    // From here on the listener is not called any more, whatever paho still delivers.
    {
      std::lock_guard<std::mutex> lock(state_->mu);
      state_->closed = true;
    }
    if (state_->connected.load()) {
      auto op = std::make_shared<Op>();
      MQTTAsync_disconnectOptions d = MQTTAsync_disconnectOptions_initializer;
      d.timeout = 1000;
      d.onSuccess = &OnOpSuccess;
      d.onFailure = &OnOpFailure;
      d.context = new OpContext{op};
      if (MQTTAsync_disconnect(handle_, &d) == MQTTASYNC_SUCCESS) {
        std::unique_lock<std::mutex> lock(op->mu);
        op->cv.wait_for(lock, std::chrono::milliseconds(3000), [&] { return op->done; });
      } else {
        delete static_cast<OpContext*>(d.context);
      }
    }
    DestroyLocked();
  }

  bool IsConnected() const override {
    std::shared_lock<std::shared_timed_mutex> use(handle_mu_);
    // Our own flag, not MQTTAsync_isConnected: paho's function takes an internal lock that the
    // callback thread already holds, so calling it from a message handler (a handler that
    // publishes a reply, for example) blocks forever.
    return state_->connected.load() && handle_ != nullptr;
  }

  Result Publish(const std::string& topic, const std::string& payload) override {
    std::shared_lock<std::shared_timed_mutex> use(handle_mu_);
    if (handle_ == nullptr || !state_->connected.load()) {
      return Result::Make(Errc::NotConnected, "not connected");
    }
    MQTTAsync_message msg = MQTTAsync_message_initializer;
    msg.payload = const_cast<char*>(payload.data());  // paho copies it before returning
    msg.payloadlen = static_cast<int>(payload.size());
    msg.qos = 0;
    msg.retained = 0;
    MQTTAsync_responseOptions ro = MQTTAsync_responseOptions_initializer;
    const int rc = MQTTAsync_sendMessage(handle_, topic.c_str(), &msg, &ro);
    if (rc != MQTTASYNC_SUCCESS) {
      return Result::Make(Errc::TransportError,
                          "MQTTAsync_sendMessage failed, code " + std::to_string(rc));
    }
    return Result();
  }

  Result Subscribe(const std::string& filter, std::chrono::milliseconds timeout) override {
    auto op = std::make_shared<Op>();
    MQTTAsync_responseOptions ro = MQTTAsync_responseOptions_initializer;
    ro.onSuccess = &OnOpSuccess;
    ro.onFailure = &OnOpFailure;
    ro.context = new OpContext{op};
    {
      std::shared_lock<std::shared_timed_mutex> use(handle_mu_);
      if (handle_ == nullptr || !state_->connected.load()) {
        delete static_cast<OpContext*>(ro.context);
        return Result::Make(Errc::NotConnected, "not connected");
      }
      const int rc = MQTTAsync_subscribe(handle_, filter.c_str(), 0, &ro);
      if (rc != MQTTASYNC_SUCCESS) {
        delete static_cast<OpContext*>(ro.context);
        return Result::Make(Errc::TransportError,
                            "MQTTAsync_subscribe failed, code " + std::to_string(rc));
      }
    }
    if (timeout.count() <= 0) return Result();  // queued; see ITransport::Subscribe
    std::unique_lock<std::mutex> lock(op->mu);
    if (!op->cv.wait_for(lock, timeout, [&] { return op->done; })) {
      return Result::Make(Errc::RequestTimeout, "no acknowledgement for subscription " + filter);
    }
    if (!op->ok) return Result::Make(Errc::TransportError, op->reason);
    return Result();
  }

  Result Unsubscribe(const std::string& filter) override {
    std::shared_lock<std::shared_timed_mutex> use(handle_mu_);
    if (handle_ == nullptr || !state_->connected.load()) return Result();
    MQTTAsync_responseOptions ro = MQTTAsync_responseOptions_initializer;
    const int rc = MQTTAsync_unsubscribe(handle_, filter.c_str(), &ro);
    if (rc != MQTTASYNC_SUCCESS) {
      return Result::Make(Errc::TransportError,
                          "MQTTAsync_unsubscribe failed, code " + std::to_string(rc));
    }
    return Result();
  }

 private:
  using StatePtr = std::shared_ptr<State>;

  // paho callbacks. `context` is the heap shared_ptr<State> owned by the transport.
  static State* StateOf(void* context) { return static_cast<StatePtr*>(context)->get(); }

  static void OnLost(void* context, char* cause) {
    State* s = StateOf(context);
    s->connected = false;
    std::lock_guard<std::mutex> lock(s->mu);
    if (!s->closed && s->listener != nullptr)
      s->listener->OnConnectionLost(cause != nullptr ? cause : "connection lost");
  }

  static int OnArrived(void* context, char* topic, int topic_len, MQTTAsync_message* message) {
    State* s = StateOf(context);
    const std::string t =
        topic_len > 0 ? std::string(topic, static_cast<size_t>(topic_len)) : std::string(topic);
    const std::string payload(static_cast<const char*>(message->payload),
                              static_cast<size_t>(message->payloadlen));
    MQTTAsync_freeMessage(&message);
    MQTTAsync_free(topic);
    // Hold the lock while the listener runs: Disconnect() takes it to set `closed`, so once
    // Disconnect has returned, no listener call is in progress or can start.
    std::lock_guard<std::mutex> lock(s->mu);
    if (!s->closed && s->listener != nullptr) s->listener->OnMessage(t, payload);
    return 1;  // message handled
  }

  static void OnConnectedCb(void* context, char*) {
    State* s = StateOf(context);
    // The first connection is reported to Connect() through the operation callback; this is
    // also called for it, so tell reconnects apart.
    const bool reconnected = s->ever_connected.exchange(true);
    s->connected = true;
    std::lock_guard<std::mutex> lock(s->mu);
    if (!s->closed && s->listener != nullptr && reconnected) s->listener->OnConnected(true);
  }

  // Both need api_mu_ to be held by the caller.
  void AbortLocked() {
    {
      std::lock_guard<std::mutex> lock(state_->mu);
      state_->closed = true;
    }
    state_->connected = false;
    std::unique_lock<std::shared_timed_mutex> exclusive(handle_mu_);
    if (handle_ != nullptr) {
      MQTTAsync h = handle_;
      handle_ = nullptr;
      // A connect that did not finish may still have callbacks pending; destroying ends them.
      MQTTAsync_disconnectOptions d = MQTTAsync_disconnectOptions_initializer;
      d.timeout = 0;
      MQTTAsync_disconnect(h, &d);
      MQTTAsync_destroy(&h);
    }
    ctx_.reset();
  }

  void DestroyLocked() {
    std::unique_lock<std::shared_timed_mutex> exclusive(handle_mu_);
    if (handle_ != nullptr) {
      MQTTAsync h = handle_;
      handle_ = nullptr;
      MQTTAsync_destroy(&h);
    }
    ctx_.reset();
    state_->connected = false;
    state_->ever_connected = false;
  }

  StatePtr state_;
  std::unique_ptr<StatePtr> ctx_;  // what paho hands back to the callbacks
  // handle_mu_: users of the handle (Publish, Subscribe, Unsubscribe, IsConnected) hold it shared;
  // whoever destroys the handle holds it exclusively. api_mu_ serializes Connect and Disconnect.
  MQTTAsync handle_ = nullptr;
  mutable std::shared_timed_mutex handle_mu_;
  std::mutex api_mu_;  // serializes Connect and Disconnect
};

}  // namespace

std::unique_ptr<ITransport> MakePahoTransport() {
  return std::unique_ptr<ITransport>(new PahoTransport);
}

}  // namespace openapi
}  // namespace lixel
