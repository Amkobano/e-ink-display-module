import json
import sys
from datetime import datetime, timedelta
from pathlib import Path
from typing import Dict, Any
import os
from extract_weather import extract_weather
from extract_prayer_times import extract_prayer_times
PRAYER_TIMES_AVAILABLE = True

# Load environment variables from .env file if it exists
env_path = Path(__file__).parent / '.env'
if env_path.exists():
    with open(env_path) as f:
        for line in f:
            line = line.strip()
            if line and not line.startswith('#') and '=' in line:
                key, value = line.split('=', 1)
                os.environ[key] = value


def load_previous_output(output_path = None) -> Dict[str, Any]:
    """
    Load the previously committed display_data.json so we can fall back to its
    values when a fresh extraction fails. Returns {} if missing or unreadable.
    """
    if output_path is None:
        output_path = Path(__file__).parent / "output" / "display_data.json"
    try:
        with open(output_path, encoding='utf-8') as f:
            return json.load(f)
    except (FileNotFoundError, json.JSONDecodeError, OSError):
        return {}


def aggregate_data(location: str = None) -> Dict[str, Any]:
    """
    Aggregate all data sources into a single JSON structure.
    
    Returns:
        Dict containing all display data
    """
    # System time will be local time thanks to TZ environment variable
    now = datetime.now()
    
    # Use environment variable for location
    if location is None:
        location = os.environ.get('LOCATION', '')

    # Calculate next update time (next day at 6 AM)
    next_update = (now + timedelta(days=1)).replace(hour=6, minute=0, second=0, microsecond=0)

    # Initialize data structure — location is intentionally omitted to avoid
    # leaking the city name (stored as a GitHub secret) into the committed JSON.
    aggregated_data = {
        'timestamp': now.isoformat(),
        'next_update': next_update.isoformat(),
        'prayer_times': {},
        'weather': {},
        'status': 'success'
    }

    if not location:
        print("✗ Error: LOCATION environment variable is not set. Weather will be skipped.")
        aggregated_data['status'] = 'partial'

    # Extract prayer times (if available)
    if PRAYER_TIMES_AVAILABLE:
        print("Extracting prayer times...")
        prayer_times = extract_prayer_times()
        if prayer_times:
            aggregated_data['prayer_times'] = prayer_times
            print("✓ Prayer times extracted successfully")
        else:
            # Fetch failed — reuse last-known-good prayer times so a single bad
            # run doesn't blank the display. Times drift only ~1 min/day, so a
            # stale day is far better than nothing.
            aggregated_data['status'] = 'partial'
            previous = load_previous_output()
            prev_prayer_times = previous.get('prayer_times') or {}
            if prev_prayer_times:
                aggregated_data['prayer_times'] = prev_prayer_times
                print(f"⚠ Using last-known-good prayer times from "
                      f"{previous.get('timestamp', 'unknown date')}")
            else:
                print("✗ Failed to extract prayer times (no previous data to fall back on)")
    else:
        print("⊘ Skipping prayer times (module not available)")
    
    # Extract weather data
    print("Extracting weather data...")
    country_code = os.environ.get('COUNTRY_CODE', 'DE')
    weather = extract_weather()
    if weather:
        aggregated_data['weather'] = weather
        print("✓ Weather data extracted successfully")
    else:
        aggregated_data['status'] = 'partial'
        print("✗ Failed to extract weather data")
    
    return aggregated_data


def save_to_file(data: Dict[str, Any], output_path = None) -> bool:
    """
    Save aggregated data to JSON file.
    
    Returns:
        True if successful, False otherwise
    """
    # Default to output directory relative to this script
    if output_path is None:
        output_file = Path(__file__).parent / "output" / "display_data.json"
    else:
        output_file = Path(output_path)
    try:
        print(f"[DEBUG] Preparing to save file: {output_file}")
        output_file.parent.mkdir(parents=True, exist_ok=True)
        print(f"[DEBUG] Directory ensured: {output_file.parent.resolve()}")
        # Write JSON file with pretty formatting
        with open(output_file, 'w', encoding='utf-8') as f:
            json.dump(data, f, indent=2, ensure_ascii=False)
        print(f"\n✓ Data saved to {output_file}")
        print(f"[DEBUG] File write complete: {output_file.resolve()}")
        return True
    except Exception as e:
        print(f"✗ Error saving data to file: {e}")
        return False


def main():
    """Main execution function"""
    print("=" * 50)
    print("E-Ink Display Data Aggregator")
    print("=" * 50)
    print()
    
    # Aggregate all data
    data = aggregate_data()
    
    # Save to file
    if not save_to_file(data):
        sys.exit(1)
    
    # Print summary
    print("\n" + "=" * 50)
    print("Summary:")
    print(f"Status: {data['status']}")
    print(f"Timestamp: {data['timestamp']}")
    print(f"Prayer times available: {len(data['prayer_times'])} times")
    print(f"Weather data available: {bool(data['weather'])}")
    print("=" * 50)
    print(data)

if __name__ == "__main__":
    main()
