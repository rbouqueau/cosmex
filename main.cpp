#include <cstdio>
#include <coroutine>
#include <utility>

struct State {
  struct promise_type {
    State get_return_object() { return {std::coroutine_handle<promise_type>::from_promise(*this) }; }
    std::suspend_never initial_suspend() { std::printf("initial_suspend\n"); return {}; }
    std::suspend_always final_suspend() noexcept { std::printf("final_suspend\n"); return {}; }
    void unhandled_exception() {}
  };

  State(std::coroutine_handle<promise_type> handle) noexcept : handle_{handle} {}
  ~State() noexcept { if (handle_) handle_.destroy(); }

private:
  std::coroutine_handle<promise_type> handle_;
};

/************** your stuff ****************/

enum {
  Connect,
  Ping,
  Established,
  Timeout,
  Disconnect
};

template<class Event>
struct StateMachine {
  auto operator co_await() {
    struct {
      StateMachine& sm;

      auto await_ready() const noexcept -> bool {
        std::printf("\tawait_ready\n");
        return sm.event;
      }
      auto await_suspend(std::coroutine_handle<> coroutine) noexcept {
        std::printf("\tawait_suspend\n");
        sm.coroutine = coroutine;
        return true;
      }
      auto await_resume() const noexcept {
        std::printf("\tawait_resume\n");
        struct reset {
          Event& event;
          ~reset() { event = {}; }
        } _{sm.event};
        return std::pair{sm.event, sm.event};
      }
    } awaiter{*this};

    return awaiter;
  }

  void processEvent(const Event& event) {
    this->event = event;
    coroutine.resume();
  }

private:
  Event event{};
  std::coroutine_handle<> coroutine{};
};

/************** /your stuff ****************/

struct Connection {
  void processEvent(const auto& event) { sm.processEvent(event); }

private:
  State connection() {
    for (;;) {
disconnected:
      if (auto [event, data] = co_await sm; event == Connect) {
connecting:
        std::printf("establish\n");
        if (auto [event, data] = co_await sm; event == Established) {
connected:
          switch (auto [event, data] = co_await sm; event) {
            case Ping: std::printf("ping\n"); goto connected;
            case Timeout: goto connecting;
            case Disconnect: std::printf("close\n"); goto disconnected;
          }
        }
      }
    }
  }

  StateMachine<int> sm{};
  State initial{connection()};
};

int main() {
  Connection connection{};
  connection.processEvent(Connect);
  connection.processEvent(Established);
  connection.processEvent(Ping);
  connection.processEvent(Disconnect);
}
