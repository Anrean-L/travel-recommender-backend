#pragma once
#include <httplib.h>

#include <string>
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

class IHttpClient {
   public:
    virtual HttpResponse Get(const std::string& path,
                             const QueryParams& params = {}) = 0;

    virtual HttpResponse Post(const std::string& path,
                              const QueryParams& params = {}) = 0;

    virtual ~IHttpClient() = default;
};

class HttplibHttpClient : public IHttpClient {
   public:
    HttplibHttpClient(const std::string& host) : cli_(host) {}
    HttpResponse Get(const std::string& path,
                     const QueryParams& params = {}) override;

    HttpResponse Post(const std::string& path,
                      const QueryParams& params) override;

   private:
    httplib::SSLClient cli_;
};

}  // namespace app
