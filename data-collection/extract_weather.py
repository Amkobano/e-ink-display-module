import requests
import os
from typing import Dict, Optional, List
from datetime import datetime
from collections import defaultdict


def extract_weather(city: str = None, country_code: str = "DE", api_key: Optional[str] = None) -> Optional[Dict]:
    """
    Extract current weather and 3-day forecast from OpenWeatherMap API.
    
    Args:
        city: City name (defaults to LOCATION env var)
        country_code: ISO 3166 country code
        api_key: OpenWeatherMap API key (or set OPENWEATHER_API_KEY env variable)
    
    Returns:
        Dict with current weather and forecast, or None if extraction fails
    """
    if city is None:
        city = os.environ.get('LOCATION')
    
    if not city:
        print("Error: No location provided. Set LOCATION environment variable.")
        return None
    
    # Get API key from parameter or environment variable
    if api_key is None:
        api_key = os.environ.get('OPENWEATHER_API_KEY')
    
    if not api_key:
        print("Error: No API key provided. Set OPENWEATHER_API_KEY environment variable.")
        return None
    
    try:
        base_url = "http://api.openweathermap.org/data/2.5"
        params = {
            'q': f"{city},{country_code}",
            'appid': api_key,
            'units': 'metric'
        }
        
        # Fetch current weather
        current_response = requests.get(f"{base_url}/weather", params=params, timeout=10)
        current_response.raise_for_status()
        current_data = current_response.json()
        
        # Fetch 5-day forecast
        forecast_response = requests.get(f"{base_url}/forecast", params=params, timeout=10)
        forecast_response.raise_for_status()
        forecast_data = forecast_response.json()
        
        # Extract current weather
        current = {
            'temperature': round(current_data['main']['temp']),
            'feels_like': round(current_data['main']['feels_like']),
            'humidity': current_data['main']['humidity'],
            'condition': current_data['weather'][0]['main'],
            'description': current_data['weather'][0]['description'],
            'icon': current_data['weather'][0]['icon'],
            'wind_speed': round(current_data['wind']['speed'], 1),
            'sunrise': current_data['sys']['sunrise'],
            'sunset': current_data['sys']['sunset']
        }
        
        # Process forecast - group by day and find high/low
        forecast = _process_forecast(forecast_data['list'], days=3)
        
        return {
            'current': current,
            'forecast': forecast
        }
        
    except requests.exceptions.RequestException as e:
        print(f"Error fetching weather data: {e}")
        return None
    except (KeyError, ValueError) as e:
        print(f"Error parsing weather data: {e}")
        return None


def _process_forecast(forecast_list: List[Dict], days: int = 3) -> List[Dict]:
    """
    Process forecast data to get daily average temperature and total precipitation.
    
    The API returns data every 3 hours. We group by day and average temperature,
    sum precipitation, and find the most common condition.
    """
    daily_data = defaultdict(lambda: {'temps': [], 'conditions': [], 'pop': []})
    today = datetime.now().date()
    
    for item in forecast_list:
        # Parse timestamp
        dt = datetime.fromtimestamp(item['dt'])
        date_str = dt.strftime('%Y-%m-%d')
        
        # Skip today, we want future days
        if dt.date() == today:
            continue
        
        daily_data[date_str]['temps'].append(item['main']['temp'])
        daily_data[date_str]['conditions'].append(item['weather'][0]['main'])
        # pop = probability of precipitation (0.0 to 1.0)
        daily_data[date_str]['pop'].append(item.get('pop', 0.0))
    
    # Build forecast for next 3 days
    forecast = []
    for date_str in sorted(daily_data.keys())[:days]:
        data = daily_data[date_str]
        
        # Find most common condition (mode)
        condition_counts = {}
        for c in data['conditions']:
            condition_counts[c] = condition_counts.get(c, 0) + 1
        main_condition = max(condition_counts, key=condition_counts.get)
        
        forecast.append({
            'date': date_str,
            'temperature': round(sum(data['temps']) / len(data['temps'])),
            'rain_chance': round(max(data['pop']) * 100),
            'condition': main_condition
        })
    
    return forecast


if __name__ == "__main__":
    # Test the extraction
    result = extract_weather()
    if result:
        print("=== Current Weather ===")
        current = result['current']
        print(f"Temperature: {current['temperature']}°C (feels like {current['feels_like']}°C)")
        print(f"Condition: {current['condition']} - {current['description']}")
        print(f"Humidity: {current['humidity']}%")
        print(f"Wind Speed: {current['wind_speed']} m/s")
        
        print("\n=== 3-Day Forecast ===")
        for day in result['forecast']:
            print(f"{day['date']}: {day['low']}°C - {day['high']}°C, {day['condition']}")
    else:
        print("Failed to extract weather data")
        print("\nTo use this script, you need an OpenWeatherMap API key:")
        print("1. Sign up at https://openweathermap.org/api")
        print("2. Get your free API key")
        print("3. Set it as environment variable: export OPENWEATHER_API_KEY='your_key_here'")
