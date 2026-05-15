#pragma once
#include <httplib.h>

#include <chrono>
#include <list>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace app {

struct HttpResponse {
    int status;
    std::string body;
};

struct QueryParam {
    std::string key;
    std::string value;
};

using QueryParams = std::vector<app::QueryParam>;

struct HttpRequestBody {
    std::string content;
    std::string content_type;
};

class IHttpClient {
   public:
    virtual HttpResponse Get(const std::string& path,
                             const QueryParams& params = {}) = 0;

    virtual HttpResponse Post(const std::string& path,
                              const QueryParams& params = {}) = 0;

    virtual HttpResponse Post(const std::string& path,
                              const HttpRequestBody& body,
                              const QueryParams& params = {}) = 0;

    virtual ~IHttpClient() = default;
};

class HttplibHttpClient : public IHttpClient {
   public:
    HttplibHttpClient(const std::string& host, std::size_t max_cache_size,
                      std::chrono::seconds cache_ttl)
        : cli_(host), max_cache_size_(max_cache_size), cache_ttl_(cache_ttl) {}
    HttpResponse Get(const std::string& path,
                     const QueryParams& params = {}) override;

    HttpResponse Post(const std::string& path,
                      const QueryParams& params) override;

    HttpResponse Post(const std::string& path, const HttpRequestBody& body,
                      const QueryParams& params = {}) override;

   private:
    struct CacheEntry {
        std::string key;
        HttpResponse response;
        std::chrono::steady_clock::time_point expires_at;
    };
    std::list<CacheEntry> cache_order_;
    std::unordered_map<std::string, std::list<CacheEntry>::iterator> cache_;
    std::size_t max_cache_size_;
    std::chrono::seconds cache_ttl_;

    std::optional<HttpResponse> TryGetCached(const std::string& key);

    void StoreCache(const std::string& key, const HttpResponse& response);

    httplib::SSLClient cli_;
};

}  // namespace app
