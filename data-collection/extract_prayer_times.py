from bs4 import BeautifulSoup
import requests
import json
import re
import time
from typing import Dict, Optional
from datetime import datetime
import os


def extract_prayer_times(url: str = None, retries: int = 3,
                         backoff: float = 2.0) -> Optional[Dict[str, str]]:
    """
    Extract prayer times from Mawaqit website.

    Retries transient fetch failures (timeout, network, HTTP, parse error) with a
    growing backoff. A successful fetch whose calendar simply lacks today's date is
    NOT retried — the page is static, so retrying won't change the result.

    Returns:
        Dict with prayer names and times, or None if extraction fails after retries
    """
    if url is None:
        url = os.environ.get('PRAYER_TIMES_URL')

    if not url:
        print("Error: PRAYER_TIMES_URL is not set. Please set the environment variable.")
        return None

    prayer_names = ['fajr', 'shuruq', 'dhuhr', 'asr', 'maghrib', 'isha']

    for attempt in range(retries):
        try:
            # Fetch the page
            response = requests.get(url, timeout=15)
            response.raise_for_status()

            # Find the confData JSON embedded in the page's script tag
            conf_data_match = re.search(r'let confData = ({.*?});', response.text, re.DOTALL)
            if not conf_data_match:
                raise ValueError("Could not find confData in the page")

            conf_data = json.loads(conf_data_match.group(1))

            # Calendar structure: month (0-indexed) -> day (string) ->
            # [fajr, shuruq, dhuhr, asr, maghrib, isha]
            calendar = conf_data.get('calendar', [])

            # Use system clock (controlled by TZ env var in workflow)
            now = datetime.now()
            month_index = now.month - 1  # Calendar is 0-indexed
            day_str = str(now.day)
            print(f"[DEBUG] Fetching prayer times for local date: {now.strftime('%Y-%m-%d %H:%M:%S')}")

            prayer_times = {}
            if calendar and month_index < len(calendar) and day_str in calendar[month_index]:
                day_times = calendar[month_index][day_str]
                for name, time_str in zip(prayer_names, day_times):
                    prayer_times[name] = time_str

            # Page fetched and parsed successfully — return whatever we found.
            # (Empty dict here means the date is missing; retrying won't help.)
            return prayer_times

        except Exception as e:
            print(f"Error extracting prayer times (attempt {attempt + 1}/{retries}): {e}")
            if attempt < retries - 1:
                delay = backoff * (attempt + 1)
                print(f"Retrying in {delay:.0f}s...")
                time.sleep(delay)

    print("Giving up on prayer times after all retries.")
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