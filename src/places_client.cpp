#include "api/places_client.hpp"

#include "api/http_client.hpp"
#include "app/config.hpp"
#include "app/domain.hpp"
#include "nlohmann/json.hpp"

namespace app {

using json = nlohmann::json;

namespace {

struct OsmTag {
    std::string key;
    std::string value;
};

std::vector<OsmTag> tags = {
    {"tourism", "museum"},     {"tourism", "gallery"},
    {"tourism", "attraction"}, {"tourism", "viewpoint"},
    {"amenity", "cafe"},       {"amenity", "restaurant"},
    {"amenity", "fast_food"},  {"amenity", "bar"},
    {"amenity", "pub"},        {"amenity", "nightclub"},
    {"leisure", "park"},       {"leisure", "garden"},
    {"shop", "bakery"},        {"historic", "monument"},
    {"historic", "memorial"},  {"historic", "building"},
};

std::string GetString(const json& object, const std::string& key) {
    if (!object.contains(key) || !object.at(key).is_string()) {
        return "";
    }
    return object.at(key).get<std::string>();
}

bool TryReadCoordinates(const json& element, Coordinates& coords) {
    if (element.contains("lat") && element.contains("lon")) {
        coords.lat = element.at("lat").get<double>();
        coords.lon = element.at("lon").get<double>();
        return true;
    }

    if (element.contains("center") && element.at("center").is_object()) {
        const json& center = element.at("center");
        if (center.contains("lat") && center.contains("lon")) {
            coords.lat = center.at("lat").get<double>();
            coords.lon = center.at("lon").get<double>();
            return true;
        }
    }

    return false;
}

PlaceKind ParsePlaceKind(const json& tags) {
    const std::string tourism = GetString(tags, "tourism");
    const std::string amenity = GetString(tags, "amenity");
    const std::string leisure = GetString(tags, "leisure");
    const std::string shop = GetString(tags, "shop");
    const std::string craft = GetString(tags, "craft");
    const std::string historic = GetString(tags, "historic");

    if (tourism == "museum") {
        return PlaceKind::kMuseum;
    }
    if (tourism == "gallery") {
        return PlaceKind::kGallery;
    }
    if (tourism == "attraction") {
        return PlaceKind::kAttraction;
    }
    if (tourism == "viewpoint") {
        return PlaceKind::kViewpoint;
    }

    if (amenity == "cafe") {
        return PlaceKind::kCafe;
    }
    if (amenity == "restaurant") {
        return PlaceKind::kRestaurant;
    }
    if (amenity == "fast_food") {
        return PlaceKind::kFastFood;
    }
    if (amenity == "bar") {
        return PlaceKind::kBar;
    }
    if (amenity == "pub") {
        return PlaceKind::kPub;
    }
    if (amenity == "nightclub") {
        return PlaceKind::kNightclub;
    }
    if (amenity == "theatre") {
        return PlaceKind::kTheatre;
    }
    if (amenity == "cinema") {
        return PlaceKind::kCinema;
    }

    if (leisure == "park") {
        return PlaceKind::kPark;
    }
    if (leisure == "garden") {
        return PlaceKind::kGarden;
    }

    if (shop == "bakery" || craft == "bakery") {
        return PlaceKind::kBakery;
    }

    if (!historic.empty()) {
        return PlaceKind::kHistoric;
    }

    return PlaceKind::kUnknown;
}

PlaceEnvironment GetEnvironment(PlaceKind kind) {
    switch (kind) {
        case PlaceKind::kPark:
        case PlaceKind::kGarden:
        case PlaceKind::kViewpoint: {
            return PlaceEnvironment::kOutdoor;
        }
        case PlaceKind::kAttraction:
        case PlaceKind::kHistoric: {
            return PlaceEnvironment::kMixed;
        }
        case PlaceKind::kMuseum:
        case PlaceKind::kGallery:
        case PlaceKind::kCafe:
        case PlaceKind::kRestaurant:
        case PlaceKind::kFastFood:
        case PlaceKind::kBar:
        case PlaceKind::kPub:
        case PlaceKind::kNightclub:
        case PlaceKind::kBakery:
        case PlaceKind::kTheatre:
        case PlaceKind::kCinema: {
            return PlaceEnvironment::kIndoor;
        }
        default: {
            return PlaceEnvironment::kMixed;
        }
    }
}

std::string BuildAddress(const json& tags) {
    const std::string full = GetString(tags, "addr:full");
    if (!full.empty()) {
        return full;
    }

    const std::string street = GetString(tags, "addr:street");
    const std::string house = GetString(tags, "addr:housenumber");
    if (!street.empty() && !house.empty()) {
        return street + ", " + house;
    }
    if (!street.empty()) {
        return street;
    }
    if (!house.empty()) {
        return house;
    }
    return "";
}

std::string GetWebsite(const json& tags) {
    std::string website = GetString(tags, "website");
    if (!website.empty()) {
        return website;
    }
    website = GetString(tags, "contact:website");
    if (!website.empty()) {
        return website;
    }
    return "";
}

bool AlreadyAdded(const std::vector<Place>& places, const std::string& osm_type,
                  long long osm_id) {
    for (const Place& place : places) {
        if (place.osm_type == osm_type && place.osm_id == osm_id) {
            return true;
        }
    }
    return false;
}

}  // namespace

std::string PlacesClient::BuildQuery(const Coordinates& coords, int radius_m) {
    std::string lat = std::to_string(coords.lat);
    std::string lon = std::to_string(coords.lon);
    std::string radius = std::to_string(radius_m);
    std::string query = "[out:json][timeout:25];\n(\n";
    for (const auto& tag : tags) {
        query += "  node[\"" + tag.key + "\"=\"" + tag.value +
                 "\"](around:" + radius + "," + lat + "," + lon + ");\n";
        query += "  way[\"" + tag.key + "\"=\"" + tag.value +
                 "\"](around:" + radius + "," + lat + "," + lon + ");\n";
        query += "  relation[\"" + tag.key + "\"=\"" + tag.value +
                 "\"](around:" + radius + "," + lat + "," + lon + ");\n";
    }
    query += ");\nout center tags;";
    return query;
}

std::expected<std::vector<Place>, std::string> PlacesClient::FetchPlaces(
    const Coordinates& coords, int radius_m) {
    std::string query = BuildQuery(coords, radius_m);
    HttpResponse response = client_.Post("/api/interpreter", {{"data", query}});
    if (IsErrorCode(response.status)) {
        return std::unexpected("Received an error status from API: " +
                               std::to_string(response.status));
    }
    return ParsePlaces(response.body);
}

std::expected<std::vector<Place>, std::string> PlacesClient::ParsePlaces(
    const std::string& body) {
    try {
        json data = json::parse(body);

        const json& elements = data.at("elements");
        if (!elements.is_array()) {
            return std::unexpected(
                "Overpass response field 'elements' is not array");
        }

        std::vector<Place> places;

        for (const json& element : elements) {
            if (!element.is_object()) {
                continue;
            }

            if (!element.contains("tags") || !element.at("tags").is_object()) {
                continue;
            }

            const json& tags = element.at("tags");

            std::string name = GetString(tags, "name");
            if (name.empty()) {
                name = GetString(tags, "brand");
            }
            if (name.empty()) {
                continue;
            }

            Coordinates coords;
            if (!TryReadCoordinates(element, coords)) {
                continue;
            }

            PlaceKind kind = ParsePlaceKind(tags);
            if (kind == PlaceKind::kUnknown) {
                continue;
            }

            std::string osm_type = GetString(element, "type");
            long long osm_id = element.at("id").get<long long>();

            if (AlreadyAdded(places, osm_type, osm_id)) {
                continue;
            }

            Place place;
            place.name = std::move(name);
            place.kind = kind;
            place.environment = GetEnvironment(kind);
            place.coords = coords;
            place.osm_type = std::move(osm_type);
            place.osm_id = osm_id;
            place.address = BuildAddress(tags);
            place.website = GetWebsite(tags);
            place.opening_hours = GetString(tags, "opening_hours");

            places.push_back(std::move(place));
        }

        return places;
    } catch (const std::exception& e) {
        return std::unexpected("Failed to parse places response: " +
                               std::string(e.what()));
    }
}

}  // namespace app
