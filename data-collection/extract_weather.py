import requests
import os
from typing import Dict, Optional


def extract_weather(city: str = "Stuttgart", country_code: str = "DE", api_key: Optional[str] = None) -> Optional[Dict]:
    """
    Extract weather data from OpenWeatherMap API.
    
    Args:
        city: City name
        country_code: ISO 3166 country code
        api_key: OpenWeatherMap API key (or set OPENWEATHER_API_KEY env variable)
    
    Returns:
        Dict with weather information, or None if extraction fails
    """
    # Get API key from parameter or environment variable
    if api_key is None:
        api_key = os.environ.get('OPENWEATHER_API_KEY')
    
    if not api_key:
        print("Error: No API key provided. Set OPENWEATHER_API_KEY environment variable.")
        return None
    
    try:
        # OpenWeatherMap API endpoint
        url = "http://api.openweathermap.org/data/2.5/weather"
        
        params = {
            'q': f"{city},{country_code}",
            'appid': api_key,
            'units': 'metric'  # Use Celsius
        }
        
        response = requests.get(url, params=params, timeout=10)
        response.raise_for_status()
        
        data = response.json()
        
        # Extract relevant weather information
        weather_info = {
            'temperature': round(data['main']['temp']),
            'feels_like': round(data['main']['feels_like']),
            'humidity': data['main']['humidity'],
            'condition': data['weather'][0]['main'],
            'description': data['weather'][0]['description'],
            'icon': data['weather'][0]['icon'],
            'wind_speed': round(data['wind']['speed'], 1),
            'sunrise': data['sys']['sunrise'],
            'sunset': data['sys']['sunset']
        }
        
        return weather_info
        
    except requests.exceptions.RequestException as e:
        print(f"Error fetching weather data: {e}")
        return None
    except (KeyError, ValueError) as e:
        print(f"Error parsing weather data: {e}")
        return None


if __name__ == "__main__":
    # Test the extraction
    result = extract_weather()
    if result:
        print("=== Weather Data ===")
        print(f"Temperature: {result['temperature']}°C (feels like {result['feels_like']}°C)")
        print(f"Condition: {result['condition']} - {result['description']}")
        print(f"Humidity: {result['humidity']}%")
        print(f"Wind Speed: {result['wind_speed']} m/s")
    else:
        print("Failed to extract weather data")
        print("\nTo use this script, you need an OpenWeatherMap API key:")
        print("1. Sign up at https://openweathermap.org/api")
        print("2. Get your free API key")
        print("3. Set it as environment variable: export OPENWEATHER_API_KEY='your_key_here'")
