// A transport for tests: no network. The test decides what the "device" answers.
#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <set>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "lixel/openapi/transport.h"

namespace testing_support {

class FakeTransport : public lixel::openapi::ITransport {
 public:
  using Result = lixel::openapi::Result;
  using Errc = lixel::openapi::Errc;

  // Called for every message the client publishes, on the publishing thread.
  using PublishHook =
      std::function<void(FakeTransport&, const std::string& topic, const std::string& payload)>;

  ~FakeTransport() override {
    Disconnect();
    StopWorker();
  }

  // ---- ITransport ----
  void SetListener(lixel::openapi::ITransportListener* l) override {
    std::lock_guard<std::mutex> lock(mu_);
    listener_ = l;
  }
  Result Connect(const lixel::openapi::TransportOptions& o) override {
    if (refuse_connect) return Result::Make(Errc::ConnectFailed, "refused by the fake");
    options = o;
    connected_ = true;
    StartWorker();
    return Result();
  }
  void Disconnect() override {
    connected_ = false;
    // wait until the worker is not inside a listener call, then no more calls happen
    std::lock_guard<std::mutex> lock(deliver_mu_);
    std::lock_guard<std::mutex> lock2(mu_);
    pending_.clear();
  }
  bool IsConnected() const override { return connected_; }
  Result Publish(const std::string& topic, const std::string& payload) override {
    if (!connected_) return Result::Make(Errc::NotConnected, "not connected");
    PublishHook hook;
    {
      std::lock_guard<std::mutex> lock(mu_);
      published.emplace_back(topic, payload);
      hook = on_publish;
    }
    if (hook) hook(*this, topic, payload);
    return Result();
  }
  Result Subscribe(const std::string& filter, std::chrono::milliseconds timeout) override {
    if (!connected_) return Result::Make(Errc::NotConnected, "not connected");
    if (fail_subscribe) return Result::Make(Errc::TransportError, "subscribe refused by the fake");
    std::lock_guard<std::mutex> lock(mu_);
    subscribed.insert(filter);
    subscribe_timeouts.push_back(timeout);
    return Result();
  }
  Result Unsubscribe(const std::string& filter) override {
    std::lock_guard<std::mutex> lock(mu_);
    subscribed.erase(filter);
    return Result();
  }

  // ---- test controls ----
  // Delivers a message to the client from the transport's own thread, after `delay`.
  void Inject(const std::string& topic, const std::string& payload,
              std::chrono::milliseconds delay = std::chrono::milliseconds(0)) {
    std::lock_guard<std::mutex> lock(mu_);
    pending_.push_back(
        {std::chrono::steady_clock::now() + delay, topic, payload, Kind::Message, false});
    cv_.notify_all();
  }
  // Simulates a connection loss followed by a reconnect (the broker has forgotten the
  // subscriptions).
  void SimulateReconnect() {
    {
      std::lock_guard<std::mutex> lock(mu_);
      subscribed.clear();
      pending_.push_back({std::chrono::steady_clock::now(), "", "", Kind::Lost, false});
      pending_.push_back({std::chrono::steady_clock::now(), "", "", Kind::Connected, true});
    }
    cv_.notify_all();
  }

  std::vector<std::pair<std::string, std::string>> Published() const {
    std::lock_guard<std::mutex> lock(mu_);
    return published;
  }
  bool IsSubscribed(const std::string& f) const {
    std::lock_guard<std::mutex> lock(mu_);
    return subscribed.count(f) != 0;
  }
  std::set<std::string> Subscribed() const {
    std::lock_guard<std::mutex> lock(mu_);
    return subscribed;
  }
  std::vector<std::chrono::milliseconds> SubscribeTimeouts() const {
    std::lock_guard<std::mutex> lock(mu_);
    return subscribe_timeouts;
  }
  void SetPublishHook(PublishHook h) {
    std::lock_guard<std::mutex> lock(mu_);
    on_publish = std::move(h);
  }

  bool refuse_connect = false;
  std::atomic<bool> fail_subscribe{false};
  lixel::openapi::TransportOptions options;

 private:
  enum class Kind { Message, Lost, Connected };
  struct Item {
    std::chrono::steady_clock::time_point at;
    std::string topic, payload;
    Kind kind;
    bool reconnected;
  };

  void StartWorker() {
    if (worker_.joinable()) return;
    stop_ = false;
    worker_ = std::thread([this] { Run(); });
  }
  void StopWorker() {
    {
      std::lock_guard<std::mutex> lock(mu_);
      stop_ = true;
    }
    cv_.notify_all();
    if (worker_.joinable()) worker_.join();
  }
  void Run() {
    std::unique_lock<std::mutex> lock(mu_);
    while (!stop_) {
      if (pending_.empty()) {
        cv_.wait(lock);
        continue;
      }
      auto soonest = pending_.begin();
      for (auto it = pending_.begin(); it != pending_.end(); ++it) {
        if (it->at < soonest->at) soonest = it;
      }
      if (soonest->at > std::chrono::steady_clock::now()) {
        cv_.wait_until(lock, soonest->at);
        continue;
      }
      Item item = *soonest;
      pending_.erase(soonest);
      lixel::openapi::ITransportListener* l = listener_;
      lock.unlock();
      {
        std::lock_guard<std::mutex> deliver(deliver_mu_);
        if (l != nullptr && connected_) {
          if (item.kind == Kind::Message)
            l->OnMessage(item.topic, item.payload);
          else if (item.kind == Kind::Lost)
            l->OnConnectionLost("simulated");
          else
            l->OnConnected(item.reconnected);
        }
      }
      lock.lock();
    }
  }

  mutable std::mutex mu_;
  std::mutex deliver_mu_;  // held while a listener call runs
  std::condition_variable cv_;
  lixel::openapi::ITransportListener* listener_ = nullptr;
  std::atomic<bool> connected_{false};
  bool stop_ = false;
  std::thread worker_;
  std::deque<Item> pending_;
  std::vector<std::pair<std::string, std::string>> published;
  std::set<std::string> subscribed;
  std::vector<std::chrono::milliseconds> subscribe_timeouts;
  PublishHook on_publish;
};

}  // namespace testing_support
