#include <filesystem>
#include <iostream>
#include <routes.hpp>
#include <sock.hpp>

#define route(_path, handler)   \
  if (req.path == _path) {      \
    chemapp::handler(req, res); \
    return;                     \
  }

#define route_method(_path, _method, handler)        \
  if (req.path == _path and req.method == _method) { \
    chemapp::handler(req, res);                      \
    return;                                          \
  }

int main() {
  using sock::socket;
  namespace fs = std::filesystem;

  auto path = [&](std::string p) {
    return "/Users/avighna/Desktop/webdev/chemapp/src/" + p;
  };

  auto file = [&](const std::string &abs) -> std::string {
    std::ifstream f(abs, std::ios::binary);
    if (!f) {
      return {}; // caller will handle 404}
    }
    f.seekg(0, std::ios::end);
    std::streamsize size = f.tellg();
    f.seekg(0, std::ios::beg);
    std::string s(size > 0 ? static_cast<size_t>(size) : 0, '\0');
    if (size > 0)
      f.read(s.data(), size);
    return s;
  };

  auto trim = [](std::string s) {
    auto l = s.find_first_not_of(" \t\r\n");
    auto r = s.find_last_not_of(" \t\r\n");
    return l == std::string::npos ? "" : s.substr(l, r - l + 1);
  };

  // load .env
  fs::path dir = fs::current_path();
  fs::path env;
  while (true) {
    fs::path p = dir / ".env";
    if (fs::exists(p) and fs::is_regular_file(p)) {
      env = p;
      break;
    }
    if (!dir.has_parent_path() or dir.parent_path() == dir) {
      break;
    }
    dir = dir.parent_path();
  }
  if (!env.empty()) {
    std::ifstream f(env);
    std::string line;
    while (std::getline(f, line)) {
      if (!line.empty() and line.back() == '\r') {
        line.pop_back(); // crlf
      }
      line = trim(line);
      if (line.empty() or line[0] == '#') {
        continue;
      }
      if (line.rfind("export ", 0) == 0) {
        line.erase(0, 7);
      }
      auto eq = line.find('=');
      if (eq == std::string::npos) {
        continue;
      }
      std::string key = trim(line.substr(0, eq));
      std::string val = trim(line.substr(eq + 1));
      if (val.size() >= 2 and
          ((val.front() == '"' and val.back() == '"') or
           (val.front() == '\'' and val.back() == '\''))) {
        val = val.substr(1, val.size() - 2);
      }
      if (!key.empty()) {
        setenv(key.c_str(), val.c_str(), 1);
      }
    }
  }

  auto safe = [](std::string_view p) {
    // very basic traversal guard
    return p.find("..") == std::string_view::npos;
  };

  // check .env
  const char *frontend_url = std::getenv("FRONTEND_URL");
  if (!frontend_url) {
    throw std::runtime_error("FRONTEND_URL not set");
  }

  socket sock;
  sock.listen(6942, [&](const socket::request &req, socket::response &res) {
    // allow cors
    res.headers["access-control-allow-origin"] = frontend_url;
    res.headers["access-control-allow-credentials"] = "true";
    res.headers["access-control-allow-headers"] = "Content-Type, Authorization";
    res.status = 200;
    if (req.method == "OPTIONS") {
      return; // preflight
    }
    // actual stuff
    std::cout << "[" << req.method << "]: " << req.path << '\n';
    // index.html
    if (req.path == "/") {
      res.html(file(path("static/html/index.html")));
      return;
    }
    // prevent ../ malicious stuff
    if (!safe(req.path)) {
      res.status = 404;
      res.json({{"error", "not found"}});
      return;
    }

    // html
    if (req.path.size() > 1 and req.path.find('/', 1) == std::string::npos and req.path.find('.') == std::string::npos) {
      fs::path abs = fs::path(path("static/html")) / (req.path.substr(1) + ".html");
      if (!fs::exists(abs)) {
        res.status = 404;
        return res.json({{"error", "not found"}});
      }
      res.html(file(abs.string()));
      return;
    }

    // js
    if (req.path.rfind("/js/", 0) == 0) {
      fs::path abs = fs::path(path("static/js")) / req.path.substr(4);
      // require .js explicitly
      if (abs.extension() != ".js" or !fs::exists(abs) or !fs::is_regular_file(abs)) {
        res.status = 404;
        return res.json({{"error", "not found"}});
      }
      return res.js(file(abs.string()));
    }

    // css
    if (req.path.rfind("/css/", 0) == 0) {
      fs::path abs = fs::path(path("static/css")) / req.path.substr(5);
      if (abs.extension() != ".css" or !fs::exists(abs) or !fs::is_regular_file(abs)) {
        res.status = 404;
        return res.json({{"error", "not found"}});
      }
      return res.css(file(abs.string()));
    }

    // images
    if (req.path.rfind("/images/", 0) == 0) {
      fs::path abs = fs::path(path("static/images")) / req.path.substr(8);
      if (!fs::exists(abs) or !fs::is_regular_file(abs)) {
        res.status = 404;
        return res.json({{"error", "not found"}});
      }
      res.headers["content-type"] = res.mime(abs.extension().string());
      res.body = file(abs.string());
      return;
    }

    // api endpoints
    // route("/api/whoami", whoami);

    res.status = 404;
    res.json({{"error", "not found"}});
  });
}
