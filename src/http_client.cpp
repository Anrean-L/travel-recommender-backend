#include "api/http_client.hpp"

#include <httplib.h>

#include "app/config.hpp"

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
    if (IsErrorCode(res->status)) {
        return HttpResponse{res->status};
    }
    return HttpResponse{res->status, res->body};
}

}  // namespace app