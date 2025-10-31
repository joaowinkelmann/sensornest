from playwright.sync_api import sync_playwright
import re

def run(playwright):
    browser = playwright.chromium.launch()
    context = browser.new_context()
    page = context.new_page()

    # Intercept the profile request to mock a logged-in user
    page.route(
        re.compile(".*api/auth/profile"),
        lambda route: route.fulfill(
            status=200,
            json={"user": {"id": "test", "name": "Test User", "email": "test@example.com"}},
        ),
    )

    # Intercept the meshes request to provide mock data
    page.route(
        re.compile(".*api/meshes"),
        lambda route: route.fulfill(
            status=200,
            json=[
                {"id": "mesh-1", "name": "Test Mesh 1"},
                {"id": "mesh-2", "name": "Test Mesh 2"},
            ],
        ),
    )

    # Intercept statistics call to provide mock data
    page.route(
        re.compile(".*api/statistics/mesh/mesh-1"),
        lambda route: route.fulfill(
            status=200,
            json={
                "day": {"averageTemperature": 22.5, "highestTemperature": 29.1, "averageHumidity": 55.2, "highestHumidity": 65.8},
                "week": {"averageTemperature": 20.3, "highestTemperature": 31.4, "averageHumidity": 50.1, "highestHumidity": 68.3},
                "month": {"averageTemperature": 18.7, "highestTemperature": 33.2, "averageHumidity": 45.9, "highestHumidity": 72.5},
                 "year": {"averageTemperature": 15.1, "highestTemperature": 35.6, "averageHumidity": 40.3, "highestHumidity": 75.1}
            }
        )
    )

    # Bypass login by setting a dummy token
    page.goto("http://localhost:3000/sensornest")
    page.evaluate("""
        localStorage.setItem('auth', JSON.stringify({
            token: 'dummy-token',
        }));
    """)

    page.goto("http://localhost:3000/sensornest/#/meshes")

    # Wait for the first mesh item to be visible
    page.locator(".q-item").first.wait_for()

    # Click the bar chart icon for the first mesh
    page.get_by_role("button", name="bar_chart").first.click()

    # Wait for the chart to be visible
    page.locator(".chart").first.wait_for()

    page.screenshot(path="jules-scratch/verification/mesh_statistics.png")
    browser.close()

with sync_playwright() as playwright:
    run(playwright)
