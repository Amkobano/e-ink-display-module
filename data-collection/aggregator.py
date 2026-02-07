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


def aggregate_data(location: str = "Stuttgart") -> Dict[str, Any]:
    """
    Aggregate all data sources into a single JSON structure.
    
    Returns:
        Dict containing all display data
    """
    # Current timestamp
    now = datetime.utcnow()
    
    # Calculate next update time (next day at 6 AM UTC)
    next_update = (now + timedelta(days=1)).replace(hour=6, minute=0, second=0, microsecond=0)
    
    # Initialize data structure
    aggregated_data = {
        'timestamp': now.isoformat() + 'Z',
        'location': location,
        'next_update': next_update.isoformat() + 'Z',
        'prayer_times': {},
        'weather': {},
        'status': 'success'
    }
    
    # Extract prayer times (if available)
    if PRAYER_TIMES_AVAILABLE:
        print("Extracting prayer times...")
        prayer_times = extract_prayer_times()
        if prayer_times:
            aggregated_data['prayer_times'] = prayer_times
            print("✓ Prayer times extracted successfully")
        else:
            aggregated_data['status'] = 'partial'
            print("✗ Failed to extract prayer times")
    else:
        print("⊘ Skipping prayer times (module not available)")
    
    # Extract weather data
    print("Extracting weather data...")
    weather = extract_weather(city=location)
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
    data = aggregate_data(location="Stuttgart")
    
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
