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

## Security: The "Refresh" Button's GitHub Token

> This applies to the optional on-demand **refresh button** — a physical button
> that makes the device fetch the very latest data by triggering the GitHub
> Actions workflow itself. Here's the security picture in plain language.

### Why a token is needed
The refresh button tells GitHub: *"run the data update for this project now."*
GitHub won't do that for just anyone, so the device has to prove it's allowed —
it does that with a small **key** (a token) stored inside the device.

### The risks, in plain words
- 🔑 **The key lives inside the device.** If someone physically took the device
  apart, they could copy the key. But this key is **almost worthless** — all it
  can do is press "refresh data" on this one project.
- 📶 **The device also holds your WiFi password** (this is already true today,
  with or without the button). That's the more sensitive item — keeping the
  device **indoors** protects it.
- 📤 **The classic mistake is uploading a secret to the internet.** Your secret
  file is already ignored by git, so this won't happen by accident — just don't
  manually upload `secrets.h` or the built firmware binary.
- 👤 **Worst case is someone breaking into your GitHub *account*** — that has
  nothing to do with the device; it's about protecting your login.

### What someone could (and could NOT) do with a stolen key
**Could:** trigger your data-refresh workflow over and over — annoying, but it's
free on a public repo and you can shut it off instantly.
**Could NOT:** read or change your code, push commits, reach any other repo, read
your other secrets (weather/prayer URLs), or get into your account.

### How to reduce the risk (easy wins)
- ✅ **Turn on 2-factor authentication (2FA) on GitHub** — the single biggest win.
- ✅ **Use a "limited" key:** a fine-grained Personal Access Token scoped to **this
  repo only**, with **"Actions: read and write"** permission and nothing else, and
  a **90-day expiry**. Even if copied, it's harmless and dies on its own.
- ✅ **Make the device verify it's really talking to GitHub** (check the TLS
  certificate) when it sends the key — stops a sneaky same-WiFi trick.
- ✅ **Never upload** `secrets.h` or the compiled firmware binary.
- ✅ **Keep the device indoors.**

### If a key is ever exposed
Go to GitHub → **Settings → Developer settings → Personal access tokens**, and
**revoke** it — one click, instant. Then generate a new one. The 90-day expiry
also closes the window automatically even if you forget.

> **Bottom line:** the refresh key is low-risk and easy to cancel. The two things
> that matter most aren't even on the device — **turn on GitHub 2FA** and **don't
> upload secrets**.

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
