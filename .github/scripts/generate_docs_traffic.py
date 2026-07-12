#!/usr/bin/env python3
"""Export a public, aggregate Cloudflare Web Analytics traffic badge."""

from __future__ import annotations

import datetime as dt
import html
import json
import os
import urllib.error
import urllib.request
from pathlib import Path


API_URL = "https://api.cloudflare.com/client/v4/graphql"
OUTPUT_DIR = Path("analytics")
WINDOW_DAYS = 30


def require_env(name: str) -> str:
    value = os.environ.get(name)
    if not value:
        raise RuntimeError(f"Missing required environment variable: {name}")
    return value


def query_analytics(account_id: str, site_tag: str, token: str) -> list[dict]:
    end_date = dt.datetime.now(dt.UTC).date()
    start_date = end_date - dt.timedelta(days=WINDOW_DAYS - 1)
    query = """
        query WebAnalytics($accountTag: String!, $filter: RumPageloadEventsAdaptiveGroupsFilter_InputObject!) {
            viewer {
                accounts(filter: { accountTag: $accountTag }) {
                    rumPageloadEventsAdaptiveGroups(limit: 1000, filter: $filter, orderBy: [date_ASC]) {
                        dimensions { date }
                        count
                    }
                }
            }
        }
        """
    payload = {
        "query": query,
        "variables": {
            "accountTag": account_id,
            "filter": {
                "date_geq": start_date.isoformat(),
                "date_leq": end_date.isoformat(),
                "siteTag": site_tag,
            },
        },
    }
    request = urllib.request.Request(
        API_URL,
        data=json.dumps(payload).encode("utf-8"),
        headers={
            "Authorization": f"Bearer {token}",
            "Content-Type": "application/json",
            "User-Agent": "jinsonic-ai-sdk-docs-traffic-export",
        },
        method="POST",
    )
    try:
        with urllib.request.urlopen(request, timeout=30) as response:
            body = json.load(response)
    except urllib.error.HTTPError as error:
        raise RuntimeError(f"Cloudflare API request failed: HTTP {error.code}") from error

    if body.get("errors"):
        raise RuntimeError(f"Cloudflare GraphQL error: {body['errors']}")

    accounts = body.get("data", {}).get("viewer", {}).get("accounts", [])
    if len(accounts) != 1:
        raise RuntimeError("Cloudflare returned no matching account for the configured account ID")

    groups = accounts[0].get("rumPageloadEventsAdaptiveGroups")
    if not isinstance(groups, list):
        raise RuntimeError("Cloudflare response did not include RUM page-load analytics groups")
    return groups


def render_badge(page_views: int, updated_at: str) -> str:
    label = "Docs views / 30d"
    value = f"{page_views:,}"
    label_width = 122
    value_width = max(52, 16 + len(value) * 8)
    width = label_width + value_width
    escaped_label = html.escape(label)
    escaped_value = html.escape(value)
    escaped_updated = html.escape(updated_at)
    return f'''<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="20" role="img" aria-label="{escaped_label}: {escaped_value}">
  <title>{escaped_label}: {escaped_value}</title>
  <linearGradient id="b" x2="0" y2="100%"><stop offset="0" stop-opacity=".1" stop-color="#fff"/><stop offset="1" stop-opacity=".1"/></linearGradient>
  <mask id="a"><rect width="{width}" height="20" rx="3" fill="#fff"/></mask>
  <g mask="url(#a)"><path fill="#555" d="M0 0h{label_width}v20H0z"/><path fill="#0969da" d="M{label_width} 0h{value_width}v20H{label_width}z"/><path fill="url(#b)" d="M0 0h{width}v20H0z"/></g>
  <g fill="#fff" font-family="Verdana,Geneva,DejaVu Sans,sans-serif" font-size="11" text-anchor="middle"><text x="61" y="15" fill="#010101" fill-opacity=".3">{escaped_label}</text><text x="61" y="14">{escaped_label}</text><text x="{label_width + value_width / 2}" y="15" fill="#010101" fill-opacity=".3">{escaped_value}</text><text x="{label_width + value_width / 2}" y="14">{escaped_value}</text></g>
  <metadata>Updated {escaped_updated}</metadata>
</svg>
'''


def main() -> None:
    account_id = require_env("CF_ACCOUNT_ID")
    site_tag = require_env("CF_WEB_ANALYTICS_SITE_TAG")
    token = require_env("CF_API_TOKEN")
    groups = query_analytics(account_id, site_tag, token)

    daily = [
        {
            "date": group["dimensions"]["date"],
            "page_views": int(group.get("count") or 0),
        }
        for group in groups
    ]
    page_views = sum(day["page_views"] for day in daily)
    updated_at = dt.datetime.now(dt.UTC).replace(microsecond=0).isoformat().replace("+00:00", "Z")

    OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
    (OUTPUT_DIR / "docs-traffic.svg").write_text(render_badge(page_views, updated_at), encoding="utf-8")
    (OUTPUT_DIR / "docs-traffic.json").write_text(
        json.dumps(
            {
                "window_days": WINDOW_DAYS,
                "page_views": page_views,
                "updated_at": updated_at,
                "daily": daily,
            },
            ensure_ascii=True,
            indent=2,
        )
        + "\n",
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()