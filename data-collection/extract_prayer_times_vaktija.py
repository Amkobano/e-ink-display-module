import requests
import json
import re
from typing import Dict, Optional


def extract_prayer_times_vaktija(url: str = 'https://vaktija.eu/de/stuttgart') -> Optional[Dict[str, str]]:
    """
    Extract prayer times from Vaktija.eu website.
    
    Args:
        url: The vaktija.eu URL (can include location, e.g., 'https://vaktija.eu/de/stuttgart')
    
    Returns:
        Dict with prayer names and times, or None if extraction fails
    """
    try:
        # Fetch the page
        headers = {
            'User-Agent': 'Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36'
        }
        response = requests.get(url, headers=headers, timeout=10)
        response.raise_for_status()
        
        # Extract the Data JSON from the script
        script_content = response.text
        
        # Find the Data variable - it contains all prayer times
        # Pattern: var Data = {...}
        data_match = re.search(r'var\s+Data\s*=\s*({.*?});', script_content, re.DOTALL)
        
        if data_match:
            json_str = data_match.group(1)
            data = json.loads(json_str)
            
            # Structure:
            # data['v'] = array of arrays, each containing times for one day
            # data['v'][0] = today's times
            # Times order: [fajr, sunrise, dhuhr, asr, maghrib, isha]
            
            prayer_names = ['fajr', 'shuruq', 'dhuhr', 'asr', 'maghrib', 'isha']
            times_array = data.get('v', [])
            
            if times_array and len(times_array) > 0:
                today_times = times_array[0]  # First entry is today
                
                prayer_times = {}
                for name, time in zip(prayer_names, today_times):
                    prayer_times[name] = time
                
                # Also extract location if available
                location_id = data.get('locationId')
                locations = data.get('locations', [])
                if location_id is not None and location_id < len(locations):
                    prayer_times['location'] = locations[location_id]
                
                return prayer_times
            else:
                print("No prayer times found in data.")
                return None
        else:
            print("Could not find Data variable in the page.")
            return None
            
    except json.JSONDecodeError as e:
        print(f"Error parsing JSON: {e}")
        return None
    except Exception as e:
        print(f"Error extracting prayer times: {e}")
        return None


if __name__ == "__main__":
    # Test the extraction
    print("Testing Vaktija.eu prayer times extraction...")
    print("=" * 40)
    
    result = extract_prayer_times_vaktija()
    if result:
        location = result.pop('location', 'Unknown')
        print(f"Location: {location}")
        print("-" * 40)
        for name, time in result.items():
            print(f"{name.capitalize():12} {time}")
    else:
        print("Failed to extract prayer times")
