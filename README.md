# Travel Recommender Backend

C++ backend service for recommending nearby places based on user's location, current weather and walking distance.

The project was built as a practical application for a custom task scheduler.  
The backend constructs a dependency graph of computations, fetches data from external APIs, scores nearby places and returns a ranked list of recommendations.

## Features

- Location-based recommendations
- Weather-aware scoring
- Walking distance and duration calculation
- OpenStreetMap places search
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

- Open-Meteo — current weather
- Overpass API — nearby places from OpenStreetMap
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
```

Optional:

```env
PORT=8080
```

If `PORT` is not set, the server uses the default port.

## Running Locally

### Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

### Run

```bash
ORS_API_KEY=your_key TWO_IP_API_KEY=your_key ./build/src/app
```

On Windows PowerShell:

```powershell
$env:ORS_API_KEY="your_key"
$env:TWO_IP_API_KEY="your_key"
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
  travel-recommender-backend
```

PowerShell:

```powershell
docker run --rm -p 8080:8080 -e PORT=8080 -e ORS_API_KEY="your_key" -e TWO_IP_API_KEY="your_key" travel-recommender-backend
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

The backend first selects a diverse set of candidates, then requests walking route information only for those candidates.
This avoids unnecessary routing API calls and makes the service significantly faster.

## Notes

- Place data comes from OpenStreetMap, so some fields such as address, website or opening hours may be missing.
- If device coordinates are not provided, IP-based geolocation can be inaccurate, especially when using a VPN.
- External APIs may rate-limit requests, so the backend uses HTTP caching where possible.
