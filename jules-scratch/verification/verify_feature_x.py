
import asyncio
from playwright.async_api import async_playwright

async def main():
    async with async_playwright() as p:
        browser = await p.chromium.launch(headless=True)
        page = await browser.new_page()

        # Mock API responses
        await page.route("**/auth/profile", lambda route: route.fulfill(json={"user": {"id": "1", "name": "Test User", "email": "test@example.com"}}))
        await page.route("**/meshes", lambda route: route.fulfill(json=[]))
        await page.route("**/zones", lambda route: route.fulfill(json=[]))
        await page.route("**/devices", lambda route: route.fulfill(json=[{"id": "1", "name": "Test Device", "macAddress": "00:00:00:00:00:00"}]))
        await page.route(
            "**/statistics/device/1/graph",
            lambda route: route.fulfill(
                json={
                    "day": [{"bucket": "2023-10-27T10", "avgTemperature": 25, "avgHumidity": 50}],
                    "week": [],
                    "month": [],
                }
            ),
        )

        await page.goto("http://localhost:3000/sensornest/")

        # Set a mock token in local storage
        await page.evaluate("() => { localStorage.setItem('token', 'mock-token'); }")

        # Go to devices page
        await page.goto("http://localhost:3000/sensornest/#/devices")

        # Click the statistics button on the first device
        await page.wait_for_selector('button[icon="bar_chart"]')
        await page.click('button[icon="bar_chart"]')

        # Wait for the dialog to appear
        await page.wait_for_selector('.q-dialog')

        await page.screenshot(path="jules-scratch/verification/verification.png")
        await browser.close()

asyncio.run(main())
