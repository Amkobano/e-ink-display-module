# Data Collection Service

This service aggregates data from various sources for the E-Ink display project.

## Structure

- `extract_prayer_times.py` - Extracts prayer times from Mawaqit website
- `extract_weather.py` - Fetches weather data from OpenWeatherMap API
- `aggregator.py` - Combines all data sources into a single JSON file
- `requirements.txt` - Python dependencies
- `output/` - Directory containing the generated `display_data.json`

## Setup

1. Install dependencies:
```bash
pip install -r requirements.txt
```

2. Set up OpenWeatherMap API key (free tier available):
```bash
export OPENWEATHER_API_KEY='your_api_key_here'
```

Get your free API key at: https://openweathermap.org/api

## Usage

### Test individual extractors

```bash
# Test prayer times extraction
python extract_prayer_times.py

# Test weather extraction
python extract_weather.py
```

### Run full aggregation

```bash
python aggregator.py
```

This will generate `output/display_data.json` with all collected data.

## Output Format

The generated JSON file has the following structure:

```json
{
  "timestamp": "2026-01-12T08:00:00Z",
  "location": "Stuttgart",
  "next_update": "2026-01-13T06:00:00Z",
  "prayer_times": {
    "fajr": "06:15",
    "dhuhr": "12:30",
    "asr": "14:45",
    "maghrib": "17:10",
    "isha": "18:45"
  },
  "weather": {
    "temperature": 15,
    "feels_like": 13,
    "humidity": 65,
    "condition": "Cloudy",
    "description": "broken clouds",
    "icon": "04d",
    "wind_speed": 3.5,
    "sunrise": 1736667600,
    "sunset": 1736698800
  },
  "status": "success"
}
```

## GitHub Actions

This service is designed to run automatically via GitHub Actions. See the workflow file in `.github/workflows/` for the scheduled execution configuration.

## Customization

- To change the location, edit the `location` parameter in `aggregator.py`
- To add more data sources, create a new `extract_*.py` file and import it in `aggregator.py`
- Prayer times URL can be customized in `extract_prayer_times.py`
