from bs4 import BeautifulSoup
import requests
import json
import re
from typing import Dict, Optional


def extract_prayer_times(url: str = 'https://mawaqit.net/de/islamisch-albanisches-zentrum-e-v-stuttgart-70376-germany') -> Optional[Dict[str, str]]:
    """
    Extract prayer times from Mawaqit website.
    
    Returns:
        Dict with prayer names and times, or None if extraction fails
    """
    try:
        # Fetch the page
        response = requests.get(url, timeout=10)
        response.raise_for_status()
        
        # Parse the HTML content
        soup = BeautifulSoup(response.text, "html.parser")
        
        # Extract the confData JSON from the script tag
        script_content = response.text
        
        # Find the confData JSON
        conf_data_match = re.search(r'let confData = ({.*?});', script_content, re.DOTALL)
        
        if conf_data_match:
            json_str = conf_data_match.group(1)
            conf_data = json.loads(json_str)
            
            # The times array contains: [Fajr, Dhuhr, Asr, Maghrib, Isha]
            times = conf_data.get('times', [])
            prayer_names = ['fajr', 'dhuhr', 'asr', 'maghrib', 'isha']
            
            # Create dictionary with prayer times
            prayer_times = {}
            for name, time in zip(prayer_names, times):
                prayer_times[name] = time
            
            return prayer_times
        else:
            print("Could not find prayer times data in the page.")
            return None
            
    except Exception as e:
        print(f"Error extracting prayer times: {e}")
        return None


if __name__ == "__main__":
    # Test the extraction
    result = extract_prayer_times()
    if result:
        print("=== Prayer Times ===")
        for name, time in result.items():
            print(f"{name.capitalize()}: {time}")
    else:
        print("Failed to extract prayer times")
