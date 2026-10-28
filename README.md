# Travel Recommender Backend

C++ backend service for recommending nearby places based on user's location, current weather and walking distance.

The project was built as a practical application for a custom task scheduler.  
The backend constructs a dependency graph of computations, fetches data from external APIs, scores nearby places and returns a ranked list of recommendations.

## Features

- Location-based recommendations
- Weather-aware scoring
- Walking distance and duration calculation
- Geoapify Places search by category and radius
- Route matrix optimization for faster distance requests
- In-memory HTTP cache
- Docker-ready deployment
- Custom C++ task scheduler used in the recommendation pipeline

## Tech Stack

- C++23
- CMake
- cpp-httplib
- nlohmann/json
- OpenSSL
- Docker

External APIs:

- [OpenWeather Current Weather API](https://openweathermap.org/api/current) — current weather
- [Geoapify Places API](https://apidocs.geoapify.com/docs/places/) — nearby places based on OpenStreetMap data
- openrouteservice — walking distances and durations
- 2IP — fallback IP-based geolocation

## Architecture

The backend is split into several layers:

```text
HTTP Server
   |
Recommendation Service
   |
Custom Task Scheduler
   |
API Clients
   |
External APIs
```

Main modules:

```text
scheduler/
  Custom DAG-based task scheduler

api/
  HTTP client abstraction and external API clients

app/
  Domain models, recommendation pipeline and scoring logic

server/
  HTTP endpoints and request handling
```

## Custom Task Scheduler

The project includes a custom `TTaskScheduler` implementation.

It supports:

- adding tasks with arbitrary callable objects;
- lazy execution of individual tasks;
- execution of the whole task graph;
- future-like task results;
- task composition with `Apply`;
- member function pointers;
- cycle detection;
- move-aware result access.

The recommendation pipeline is built as a graph of dependent tasks:

```text
location
   ├── weather
   └── places
          |
      scored places
          |
      top candidates
          |
      walking routes
          |
      reranked places
          |
      final response
```

This makes the project demonstrate the scheduler not only in unit tests, but also in a real backend scenario.

## API

### Health check

```http
GET /health
```

Response:

```text
OK
```

### Get recommendations

```http
GET /recommend
```

Query parameters:

| Parameter     |   Type | Required | Description                        |
| ------------- | -----: | -------: | ---------------------------------- |
| `lat`         | number |       no | User latitude                      |
| `lon`         | number |       no | User longitude                     |
| `radius_m`    | number |       no | Search radius in meters            |
| `max_results` | number |       no | Maximum number of places to return |

If `lat` and `lon` are not provided, the server tries to use IP-based geolocation.

Example:

```bash
curl "http://localhost:8080/recommend?lat=59.93&lon=30.3351&radius_m=1000&max_results=10"
```

Example response shape:

```json
{
  "location": {
    "lat": 59.93,
    "lon": 30.3351
  },
  "weather": {
    "temperature": 15,
    "apparent_temperature": 10,
    "relative_humidity": 77,
    "rain": 0,
    "snowfall": 0,
    "wind_speed": 17,
    "wind_gusts": 28,
    "wind_direction": 135,
    "weather_code": 3,
    "cloud_cover": 100,
    "is_day": true,
    "mood": "okay",
    "description": "Overcast"
  },
  "places": [
    {
      "place": {
        "name": "Example place",
        "kind": "park",
        "environment": "outdoor",
        "coords": {
          "lat": 59.93,
          "lon": 30.33
        },
        "osm_type": "node",
        "osm_id": 123456,
        "address": "",
        "website": "",
        "opening_hours": ""
      },
      "score": 82.5,
      "straight_distance_m": 300,
      "route_distance_m": 376,
      "walk_time_min": 5,
      "reason": "Great outdoor option for current weather"
    }
  ]
}
```

## Environment Variables

The server requires API keys for some external services:

```env
ORS_API_KEY=your_openrouteservice_key
TWO_IP_API_KEY=your_2ip_key
GEOAPIFY_KEY=your_geoapify_key
OPENWEATHER_API_KEY=your_openweather_key
```

Optional:

```env
PORT=8080
```

If `PORT` is not set, the server uses port `8080`.

`GEOAPIFY_KEY` is required for nearby places search. Create a key in
[Geoapify MyProjects](https://myprojects.geoapify.com/). The key is sent to
`api.geoapify.com`; it is not included in responses or request logs.
`ORS_API_KEY` is used for walking routes, and `TWO_IP_API_KEY` for IP geolocation.
`OPENWEATHER_API_KEY` is used for current weather from OpenWeather.
`OPEN_WEATHER_API_KEY` is also accepted; `OPENWEATHER_API_KEY` takes precedence
when both are set.
All four keys are required at startup, including when coordinates are provided.

## Running Locally

### Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### Run

You can store the variables above in a local `.env` file (excluded from Git).
On Linux/macOS, restrict file access and load it before starting the server:

```bash
chmod 600 .env
set -a
source .env
set +a
./build/src/app
```

The application reads process environment variables; it does not load `.env`
automatically. Alternatively, provide the variables directly:

```bash
ORS_API_KEY=your_ors_key TWO_IP_API_KEY=your_2ip_key GEOAPIFY_KEY=your_geoapify_key OPENWEATHER_API_KEY=your_openweather_key ./build/src/app
```

On Windows PowerShell:

```powershell
$env:ORS_API_KEY="your_key"
$env:TWO_IP_API_KEY="your_key"
$env:GEOAPIFY_KEY="your_geoapify_key"
$env:OPENWEATHER_API_KEY="your_openweather_key"
.\build\src\app.exe
```

Then check:

```bash
curl http://localhost:8080/health
```

## Running with Docker

Build image:

```bash
docker build -t travel-recommender-backend .
```

Run container:

```bash
docker run --rm \
  -p 8080:8080 \
  -e PORT=8080 \
  -e ORS_API_KEY=your_key \
  -e TWO_IP_API_KEY=your_key \
  -e GEOAPIFY_KEY=your_geoapify_key \
  -e OPENWEATHER_API_KEY=your_openweather_key \
  travel-recommender-backend
```

PowerShell:

```powershell
docker run --rm -p 8080:8080 -e PORT=8080 -e ORS_API_KEY="your_key" -e TWO_IP_API_KEY="your_key" -e GEOAPIFY_KEY="your_geoapify_key" -e OPENWEATHER_API_KEY="your_openweather_key" travel-recommender-backend
```

## Deployment

The backend is containerized and can be deployed to platforms that support Docker images.

Important deployment requirements:

- the server must listen on `0.0.0.0`;
- the port should be read from the `PORT` environment variable;
- API keys should be configured as environment variables;
- frontend URL should be allowed by CORS if strict CORS configuration is used.

## Recommendation Logic

The scoring model is heuristic-based.

It takes into account:

- place category;
- indoor/outdoor suitability;
- current weather mood;
- cloud cover;
- walking distance;
- walking duration;
- diversity of place categories.

Weather is fetched from `https://api.openweathermap.org/data/2.5/weather` using
`lat`, `lon`, `appid`, and `units=metric`. Use the standard Current Weather API;
no One Call subscription is needed. Create a key in your
[OpenWeather account](https://home.openweathermap.org/api_keys) and set
`OPENWEATHER_API_KEY` in the backend environment (including on Render).
Do not put it in frontend `VITE_*` variables.

The response fields used by the frontend are preserved. Temperatures are Celsius,
wind speed and gusts are converted from m/s to km/h, and rain is mm/h.
Snow water equivalent is converted to estimated snow depth in cm using a 7:1
snow/water ratio to retain the existing scoring thresholds. Missing rain/snow
values mean zero; missing gusts fall back to wind speed. Day/night comes from
the weather icon suffix; the description comes from OpenWeather. The internal
`weather_code` now contains an OpenWeather condition ID (not a WMO code).
Successful weather responses are cached in memory for ten minutes. Provider
errors include the service name and HTTP status, without exposing the API key.

Nearby places are fetched from `https://api.geoapify.com/v2/places` using a
`circle:longitude,latitude,radius` filter and proximity sorting. A single request
retrieves up to 100 candidates across supported categories: museums, galleries,
viewpoints, cafes, restaurants, fast food, bars, pubs, nightclubs, parks, gardens,
bakeries, historic sights, theatres, and cinemas. This bounded candidate list is
not an exhaustive listing of all places within the radius.

Geoapify categories and source tags are mapped to the application's place kinds.
Unnamed places, unsupported kinds, invalid coordinates, and duplicate places
are skipped. The existing response shape is preserved: `osm_type` and `osm_id`
come from Geoapify's source metadata when available; otherwise they remain
empty and `0`. Address, website, and opening hours are included when supplied.

The backend first selects a diverse set of candidates, then requests walking route information only for those candidates.
This avoids unnecessary routing API calls and makes the service significantly faster.

## Notes

- Place data is provided by Geoapify based on OpenStreetMap. Some fields such as address, website or opening hours may be missing.
- Display attribution to Geoapify and OpenStreetMap contributors when showing place data; follow the provider's [attribution requirements](https://www.geoapify.com/attribution/).
- Geoapify's free plan uses a daily credit allowance. Places requests cost credits according to the number of returned results; see [current pricing](https://apidocs.geoapify.com/docs/places/#pricing). Successful places responses are cached in memory for two hours.
- If device coordinates are not provided, IP-based geolocation can be inaccurate, especially when using a VPN.
- External APIs may rate-limit requests, so the backend uses HTTP caching where possible.
