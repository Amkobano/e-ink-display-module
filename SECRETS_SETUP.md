# GitHub Secrets Setup Guide

## Overview
Your prayer times extraction logic is now completely private! The public repository contains no prayer times code, but GitHub Actions will generate it dynamically when it runs.

## Required GitHub Secrets

You need to set up **two secrets** in your GitHub repository:

### 1. OPENWEATHER_API_KEY
- **Purpose:** API key for fetching weather data
- **How to get:**
  1. Go to https://openweathermap.org/api
  2. Sign up for free account
  3. Get your API key from dashboard
- **Value:** Your API key from OpenWeatherMap

### 2. PRAYER_TIMES_URL
- **Purpose:** Your specific mosque's Mawaqit URL
- **Value:** Your mosque's Mawaqit URL

## How to Add Secrets to GitHub

### Step 1: Go to Your Repository Settings
1. Navigate to your GitHub repository
2. Click **Settings** (top menu)
3. In left sidebar, click **Secrets and variables** → **Actions**

### Step 2: Add Each Secret
For each secret:
1. Click **New repository secret**
2. Enter the **Name** (exactly as shown above)
3. Paste the **Value** (your API key or URL)
4. Click **Add secret**

## How It Works

### In Public Repo (What Everyone Sees):
```
data-collection/
├── extract_weather.py       ✓ Visible
├── aggregator.py            ✓ Visible (gracefully handles missing prayer times)
├── requirements.txt         ✓ Visible
└── extract_prayer_times.py  ✗ NOT IN REPO (.gitignore)
```

### During GitHub Actions:
```yaml
Step 1: Clone public repo
Step 2: Install dependencies
Step 3: Create extract_prayer_times.py dynamically
        - Uses PRAYER_TIMES_URL secret
        - File created at runtime
        - Never committed back to repo
Step 4: Run aggregator.py
        - Detects prayer times module exists
        - Extracts data using secret URL
        - Outputs to display_data.json
Step 5: Commit only the JSON output
```

### On Your Local Computer:
- You still have the real `extract_prayer_times.py` file
- It's in `.gitignore` so git won't track it
- You can run aggregator locally and it will work
- Your file never gets uploaded to GitHub

## Security Benefits

✅ **Private Logic:** Scraping code never appears in public repo  
✅ **Private URL:** Your mosque location stays in secrets  
✅ **Portfolio Friendly:** You can show this repo to employers  
✅ **Open Source:** Others can use weather/display parts without prayer times  

## Testing Your Setup

### Before Pushing to GitHub:
```bash
# Test locally (your private file should work)
cd data-collection
export PRAYER_TIMES_URL='your_url_here'
export OPENWEATHER_API_KEY='your_key_here'
python aggregator.py
```

### After Setting Up Secrets:
1. Push code to GitHub
2. Go to **Actions** tab in your repo
3. Click on the workflow "Update E-Ink Display Data"
4. Click **Run workflow** (manual trigger button)
5. Watch it run - should complete successfully
6. Check that `display_data.json` was updated

## What Gets Updated in the Repo

Only one file gets committed by GitHub Actions:
```
data-collection/output/display_data.json
```

This file contains the processed data (prayer times + weather) but no code or URLs.

## Troubleshooting

**If GitHub Actions fails:**
1. Check secrets are spelled correctly (case-sensitive!)
2. Verify OpenWeatherMap API key is valid
3. Test prayer times URL in browser (should load a page)
4. Check Actions logs for specific error messages

**If running locally fails:**
1. Make sure `extract_prayer_times.py` exists in `data-collection/`
2. Verify environment variables are set
3. Check dependencies are installed: `pip install -r requirements.txt`
