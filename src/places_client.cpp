#include "api/places_client.hpp"

#include <cmath>
#include <cstdint>
#include <unordered_set>

#include "api/http_client.hpp"
#include "app/config.hpp"
#include "app/domain.hpp"
#include "nlohmann/json.hpp"

namespace app {

using json = nlohmann::json;

namespace {

// More specific categories precede their parents (garden before park).
const std::vector<std::pair<std::string, PlaceKind>> kCategories = {
    {"entertainment.museum", PlaceKind::kMuseum},
    {"entertainment.culture.gallery", PlaceKind::kGallery},
    {"tourism.attraction.viewpoint", PlaceKind::kViewpoint},
    {"catering.cafe", PlaceKind::kCafe},
    {"catering.restaurant", PlaceKind::kRestaurant},
    {"catering.fast_food", PlaceKind::kFastFood},
    {"catering.bar", PlaceKind::kBar},
    {"catering.pub", PlaceKind::kPub},
    {"adult.nightclub", PlaceKind::kNightclub},
    {"leisure.park.garden", PlaceKind::kGarden},
    {"leisure.park", PlaceKind::kPark},
    {"commercial.food_and_drink.bakery", PlaceKind::kBakery},
    {"tourism.sights", PlaceKind::kHistoric},
    {"entertainment.culture.theatre", PlaceKind::kTheatre},
    {"entertainment.cinema", PlaceKind::kCinema},
};

std::string GetString(const json& object, const std::string& key) {
    if (!object.contains(key) || !object.at(key).is_string()) {
        return "";
    }
    return object.at(key).get<std::string>();
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

}  // namespace

std::expected<std::vector<Place>, std::string> PlacesClient::FetchPlaces(
    const Coordinates& coords, int radius_m) {
    std::string categories;
    for (const auto& [category, kind] : kCategories) {
        if (!categories.empty()) categories += ",";
        categories += category;
    }
    const std::string origin =
        std::to_string(coords.lon) + "," + std::to_string(coords.lat);
    HttpResponse response = client_.Get(
        "/v2/places",
        {
            {"apiKey", api_key_},
            {"categories", categories},
            {"filter", "circle:" + origin + "," + std::to_string(radius_m)},
            {"bias", "proximity:" + origin},
            {"limit", "100"},
        });
    if (IsErrorCode(response.status)) {
        return std::unexpected("Geoapify Places API returned status " +
                               std::to_string(response.status));
    }
    return ParsePlaces(response.body);
}

std::expected<std::vector<Place>, std::string> PlacesClient::ParsePlaces(
    const std::string& body) {
    try {
        json data = json::parse(body);

        const json& features = data.at("features");
        if (!features.is_array()) {
            return std::unexpected(
                "Geoapify response field 'features' is not array");
        }
        std::vector<Place> places;
        std::unordered_set<std::string> seen;
        for (const auto& feature : features) {
            if (!feature.is_object() || !feature.contains("properties") ||
                !feature.at("properties").is_object())
                continue;
            const auto& props = feature.at("properties");
            const json empty = json::object();
            const auto& source = props.contains("datasource") &&
                                         props.at("datasource").is_object()
                                     ? props.at("datasource")
                                     : empty;
            const auto& raw =
                source.contains("raw") && source.at("raw").is_object()
                    ? source.at("raw")
                    : empty;
            Place place{};
            place.name = GetString(props, "name");
            if (place.name.empty()) place.name = GetString(props, "brand");
            if (place.name.empty()) continue;
            if (props.contains("lat") && props.at("lat").is_number() &&
                props.contains("lon") && props.at("lon").is_number()) {
                place.coords = {props.at("lat").get<double>(),
                                props.at("lon").get<double>()};
            } else {
                if (!feature.contains("geometry") ||
                    !feature.at("geometry").is_object())
                    continue;
                const auto& geom = feature.at("geometry");
                if (GetString(geom, "type") != "Point" ||
                    !geom.contains("coordinates"))
                    continue;
                const auto& coords = geom.at("coordinates");
                if (!coords.is_array() || coords.size() < 2 ||
                    !coords[0].is_number() || !coords[1].is_number())
                    continue;
                place.coords = {coords[1].get<double>(),
                                coords[0].get<double>()};
            }
            if (!std::isfinite(place.coords.lat) ||
                !std::isfinite(place.coords.lon) ||
                std::abs(place.coords.lat) > 90 ||
                std::abs(place.coords.lon) > 180)
                continue;
            place.kind = ParsePlaceKind(raw);
            if (place.kind == PlaceKind::kUnknown &&
                props.contains("categories") &&
                props.at("categories").is_array()) {
                for (const auto& [category, kind] : kCategories) {
                    for (const auto& value : props.at("categories")) {
                        if (!value.is_string()) continue;
                        const auto text = value.get<std::string>();
                        if (text == category ||
                            text.starts_with(category + ".")) {
                            place.kind = kind;
                            break;
                        }
                    }
                    if (place.kind != PlaceKind::kUnknown) break;
                }
            }
            if (place.kind == PlaceKind::kUnknown) continue;
            place.environment = GetEnvironment(place.kind);
            place.osm_type = GetString(raw, "osm_type");
            if (place.osm_type == "n") place.osm_type = "node";
            if (place.osm_type == "w") place.osm_type = "way";
            if (place.osm_type == "r") place.osm_type = "relation";
            if (raw.contains("osm_id") &&
                raw.at("osm_id").is_number_unsigned()) {
                place.osm_id = raw.at("osm_id").get<uint64_t>();
            }
            std::string identity = GetString(props, "place_id");
            if (place.osm_id != 0 && !place.osm_type.empty()) {
                identity = place.osm_type + ":" + std::to_string(place.osm_id);
            }
            if (!identity.empty() && !seen.insert(identity).second) continue;
            place.address = GetString(props, "address_line2");
            if (place.address.empty())
                place.address = GetString(props, "formatted");
            if (place.address.empty()) place.address = BuildAddress(raw);
            place.website = GetWebsite(props);
            if (place.website.empty()) place.website = GetWebsite(raw);
            place.opening_hours = GetString(props, "opening_hours");
            if (place.opening_hours.empty())
                place.opening_hours = GetString(raw, "opening_hours");
            places.push_back(std::move(place));
        }

        return places;
    } catch (const std::exception& e) {
        return std::unexpected("Failed to parse places response: " +
                               std::string(e.what()));
    }
}

}  // namespace app
