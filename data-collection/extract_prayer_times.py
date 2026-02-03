from bs4 import BeautifulSoup
import requests
import json
import re
from typing import Dict, Optional
from datetime import datetime


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
        
        # Extract the confData JSON from the script tag
        script_content = response.text
        
        # Find the confData JSON
        conf_data_match = re.search(r'let confData = ({.*?});', script_content, re.DOTALL)
        
        if conf_data_match:
            json_str = conf_data_match.group(1)
            conf_data = json.loads(json_str)
            
            # Extract from calendar - contains astronomical times
            # Calendar structure: month (0-indexed) -> day (string) -> [fajr, shuruq, dhuhr, asr, maghrib, isha]
            calendar = conf_data.get('calendar', [])
            prayer_names = ['fajr', 'shuruq', 'dhuhr', 'asr', 'maghrib', 'isha']
            
            prayer_times = {}
            if calendar:
                now = datetime.now()
                month_index = now.month - 1  # Calendar is 0-indexed
                day_str = str(now.day)
                
                if month_index < len(calendar) and day_str in calendar[month_index]:
                    day_times = calendar[month_index][day_str]
                    for name, time in zip(prayer_names, day_times):
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
