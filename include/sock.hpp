#pragma once

#include <arpa/inet.h> // inet_ntoa(), inet_pton(), ntohs(), etc
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <netinet/in.h> // sockaddr_in, htons, htonl, INADDR_ANY
#include <semaphore>
#include <sstream>
#include <stop_token>
#include <sys/socket.h> // socket(), bind(), listen(), accept(), send(), recv()
#include <sys/types.h>  // basic system data types
#include <thread>
#include <unistd.h> // close()

#include <json.hpp> // nlohmann goat

using json = nlohmann::json;

inline unsigned cores() {
  auto c = std::thread::hardware_concurrency();
  return c ? c : 4;
}
inline std::counting_semaphore<> slots(std::max(8u, 2 * cores()));

struct slot_guard {
  std::counting_semaphore<> *s;
  ~slot_guard() { s->release(); }
};

namespace sock {
class socket {
private:
  int fd;
  sockaddr_in addr;

private:
  void bind(int port) {
    int yes = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) < 0) {
      throw std::runtime_error(std::string("setsockopt failed: ") + std::strerror(errno));
    }
    addr.sin_port = htons(port);
    if (::bind(fd, (sockaddr *)&addr, sizeof(addr)) < 0) {
      throw std::runtime_error(std::string("Failed to bind socket to port ") + std::to_string(port) + ": " + std::strerror(errno));
    }
  }

public:
  socket() {
    if ((fd = ::socket(AF_INET, SOCK_STREAM, 0)) < 0) {
      throw std::runtime_error("Failed to create socket");
    }
    std::memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
  }

  class request {
  public:
    std::string method, path, body;
    std::map<std::string, std::string> headers;
  };

  class response {
  private:
    std::string reason(int status) const {
      if (status == 201)
        return "Created";
      if (status == 401)
        return "Bad Request";
      if (status == 403)
        return "Forbidden";
      if (status == 404)
        return "Not Found";
      if (status == 500)
        return "Internal Server Error";
      return "OK";
    }

  public:
    int status;
    std::map<std::string, std::string> headers;
    std::string body;

    operator std::string() const {
      std::ostringstream oss;
      oss << "HTTP/1.1 " << status << ' ' << reason(status) << "\r\n";
      oss << "content-length: " << body.length() << "\r\n";
      oss << "connection: close\r\n";
      for (auto &[k, v] : headers) {
        oss << k << ": " << v << "\r\n";
      }
      oss << "\r\n"
          << body;
      return oss.str();
    }

    std::string mime(const std::string &ext) {
      if (ext == ".html")
        return "text/html";
      if (ext == ".css")
        return "text/css";
      if (ext == ".js")
        return "application/javascript";
      if (ext == ".json")
        return "application/json";
      if (ext == ".png")
        return "image/png";
      if (ext == ".jpg" or ext == ".jpeg")
        return "image/jpeg";
      if (ext == ".gif")
        return "image/gif";
      if (ext == ".svg")
        return "image/svg+xml";
      if (ext == ".ico")
        return "image/x-icon";
      if (ext == ".webp")
        return "image/webp";
      if (ext == ".pdf")
        return "application/pdf";
      if (ext == ".zip")
        return "application/zip";
      return "application/octet-stream";
    }

    void json(const json &j) {
      headers["content-type"] = mime(".json");
      body = j.dump();
    }
    void html(const std::string &s) {
      headers["content-type"] = mime(".html");
      body = s;
    }
    void js(const std::string &s) {
      headers["content-type"] = mime(".js");
      body = s;
    }
    void css(const std::string &s) {
      headers["content-type"] = mime(".css");
      body = s;
    }
  };

  void serve(int client_fd, const std::function<void(const request &req, response &res)> &f) {
    request req;
    std::string raw;
    std::array<char, 1024> buf; // arbitrarily chosen
    ssize_t n;
    // read until end-of-headers
    while ((n = recv(client_fd, buf.data(), buf.size(), 0)) > 0) {
      raw.append(buf.data(), n);
      if (raw.find("\r\n\r\n") != std::string::npos) {
        break;
      }
    }
    size_t header_end = raw.find("\r\n\r\n");
    std::string header_block = raw.substr(0, header_end);
    // manually parse content length from the header block
    std::istringstream iss(header_block);
    std::string line;
    std::getline(iss, line); // request line
    std::istringstream rl(line);
    rl >> req.method >> req.path; // who cares about the HTTP version
    int content_len = 0;
    while (std::getline(iss, line)) {
      if (!line.empty() and line.back() == '\r') {
        line.pop_back();
      }
      if (line.empty()) {
        break;
      }
      size_t colon = line.find(':');
      if (colon == std::string::npos) {
        continue;
      }
      std::string key = line.substr(0, colon);
      std::string val = line.substr(colon + 1);
      while (!val.empty() and val.front() == ' ') {
        val.erase(val.begin());
      }
      for (char &i : key) {
        if ('A' <= i and i <= 'Z') {
          i += 'a' - 'A';
        }
      }
      req.headers[key] = val;
      if (key == "content-length") {
        content_len = std::stoi(val);
      }
    }
    // read at max content_length more bytes
    const size_t body_off = header_end + 4;
    size_t have = (raw.size() > body_off) ? raw.size() - body_off : 0;
    size_t need = (content_len > 0) ? size_t(content_len) : 0;
    while (have < need && (n = recv(client_fd, buf.data(), buf.size(), 0)) > 0) {
      raw.append(buf.data(), n);
      have += size_t(n);
    }
    req.body = (need > 0) ? raw.substr(body_off, need) : std::string{};
    response res;
    f(req, res);
    std::string out = res;
    for (size_t sent = 0, n = 0; sent < out.length(); sent += n) {
      n = ::send(client_fd, out.data() + sent, out.length() - sent, 0);
      if (n <= 0) {
        std::cerr << "Warning: send() failed\n";
        break;
      }
    }
    close(client_fd);
  }

  void listen(int port, const std::function<void(const request &req, response &res)> &f) {
    bind(port);
    if (::listen(fd, SOMAXCONN) < 0) {
      throw std::runtime_error("Failed to listen!");
    }
    while (true) {
      int client_fd = accept(fd, nullptr, nullptr);
      if (client_fd < 0) {
        std::cerr << "accept failed: " << std::strerror(errno) << '\n';
        continue;
      }
      slots.acquire();
      std::jthread([this, client_fd, &f](std::stop_token) {
        slot_guard _{&slots};
        this->serve(client_fd, f);
      }).detach();
    }
  }
};
}; // namespace sock