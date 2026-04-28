#include "api/http_client.hpp"

#include <httplib.h>

#include <chrono>

#include "app/config.hpp"

namespace app {

namespace {

std::string BuildCacheKey(const std::string& method, const std::string& path,
                          const QueryParams& params) {
    std::string key = method + ":" + path + "?";
    for (const auto& param : params) {
        key += param.key + "=" + param.value + "&";
    }
    return key;
}

}  // namespace

std::optional<HttpResponse> HttplibHttpClient::TryGetCached(
    const std::string& key) {
    if (cache_ttl_.count() == 0) {
        return std::nullopt;
    }
    auto it = cache_.find(key);
    if (it == cache_.end()) {
        return std::nullopt;
    }
    auto list_it = it->second;
    if (std::chrono::steady_clock::now() > list_it->expires_at) {
        cache_order_.erase(list_it);
        cache_.erase(it);
        return std::nullopt;
    }
    cache_order_.splice(cache_order_.begin(), cache_order_, list_it);
    return cache_order_.front().response;
}

void HttplibHttpClient::StoreCache(const std::string& key,
                                   const HttpResponse& response) {
    if (cache_ttl_.count() == 0 || max_cache_size_ == 0 ||
        IsErrorCode(response.status)) {
        return;
    }
    auto it = cache_.find(key);
    if (it != cache_.end()) {
        cache_order_.erase(it->second);
        cache_.erase(it);
    }

    cache_order_.push_front(CacheEntry{
        .key = key,
        .response = response,
        .expires_at = std::chrono::steady_clock::now() + cache_ttl_});

    cache_[key] = cache_order_.begin();

    while (cache_order_.size() > max_cache_size_) {
        cache_.erase(cache_order_.back().key);
        cache_order_.pop_back();
    }
}

HttpResponse HttplibHttpClient::Get(const std::string& path,
                                    const QueryParams& params) {
    const std::string key = BuildCacheKey("Get", path, params);
    if (auto cached = TryGetCached(key)) {
        return *cached;
    }
    httplib::Params query_params;
    for (QueryParam param : params) {
        query_params.emplace(param.key, param.value);
    }
    auto res = cli_.Get(path, query_params, httplib::Headers{});
    HttpResponse response =
        res ? HttpResponse{res->status, res->body} : HttpResponse{0, ""};

    StoreCache(key, response);
    return response;
}

HttpResponse HttplibHttpClient::Post(const std::string& path,
                                     const QueryParams& params) {
    const std::string key = BuildCacheKey("Post", path, params);
    if (auto cached = TryGetCached(key)) {
        return *cached;
    }
    httplib::Params form_params;
    for (const QueryParam& param : params) {
        form_params.emplace(param.key, param.value);
    }

    auto res = cli_.Post(path, form_params);
    HttpResponse response =
        res ? HttpResponse{res->status, res->body} : HttpResponse{0, ""};

    StoreCache(key, response);
    return response;
}

}  // namespace app
