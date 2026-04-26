#include "api/http_client.hpp"

#include <httplib.h>

namespace app {

HttpResponse HttplibHttpClient::Get(const std::string& path,
                                    const QueryParams& params) {
    httplib::Params query_params;
    for (QueryParam param : params) {
        query_params.insert({param.key, param.value});
    }
    auto res = cli_.Get(path, query_params, httplib::Headers{});
    if (!res) {
        return HttpResponse{0, ""};
    }
    return HttpResponse{res->status, res->body};
}

HttpResponse HttplibHttpClient::Post(const std::string& path,
                                     const QueryParams& params) {
    httplib::Params form_params;
    for (const QueryParam& param : params) {
        form_params.emplace(param.key, param.value);
    }

    auto res = cli_.Post(path, form_params);
    if (!res) {
        return HttpResponse{0, ""};
    }
    return HttpResponse{res->status, res->body};
}

}  // namespace app
