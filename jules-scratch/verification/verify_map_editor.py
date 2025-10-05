import re
from playwright.sync_api import sync_playwright, expect

def run(playwright):
    browser = playwright.chromium.launch(headless=True)
    try:
        context = browser.new_context()
        page = context.new_page()

        # Register a new user
        page.goto("http://localhost:3000/sensornest/register")
        page.wait_for_load_state("networkidle")

        page.get_by_label("Name").fill("Jules")
        page.get_by_label("Email").fill("jules@test.com")
        page.get_by_label("Password").fill("password")
        page.get_by_role("button", name="Register").click()

        # Screenshot for debugging
        page.screenshot(path="jules-scratch/verification/debug_after_register.png")

        # Wait for registration to complete and redirect to login
        expect(page.get_by_text("Registration successful! Please log in.")).to_be_visible()
        page.wait_for_url("http://localhost:3000/sensornest/login")

        # Log in with the new user
        page.wait_for_load_state("networkidle")

        email_input = page.get_by_label("Email address")
        expect(email_input).to_be_visible()
        email_input.fill("jules@test.com")

        password_input = page.get_by_label("Password")
        expect(password_input).to_be_visible()
        password_input.fill("password")

        login_button = page.get_by_role("button", name="Sign in")
        expect(login_button).to_be_enabled()
        login_button.click()

        # Wait for login to complete by checking for home page content
        expect(page.locator(".text-h6:has-text('Meshes')")).to_be_visible()

        # Go to map editor
        page.goto("http://localhost:3000/sensornest/map-editor")
        page.wait_for_load_state("networkidle")

        # Wait for map to load
        expect(page.locator(".leaflet-pane")).to_be_visible()

        # Initial screenshot
        page.screenshot(path="jules-scratch/verification/01_initial_view.png")

        # Click sidebar
        sidebar_button = page.get_by_role("button", name="menu")
        expect(sidebar_button).to_be_visible()
        sidebar_button.click()
        page.screenshot(path="jules-scratch/verification/02_sidebar_expanded.png")

        # There might not be any zones for a new user, so we will skip this part.
        # # Click a zone to activate it
        # zone_polygon = page.locator(".leaflet-pane .leaflet-interactive").first
        # expect(zone_polygon).to_be_visible()
        # zone_polygon.click()
        # page.screenshot(path="jules-scratch/verification/03_zone_activated.png")

        # Click move devices
        move_devices_button = page.get_by_role("button", name="Move Devices")
        expect(move_devices_button).to_be_visible()
        move_devices_button.click()
        page.screenshot(path="jules-scratch/verification/04_move_devices_mode.png")

        # Final verification screenshot
        page.screenshot(path="jules-scratch/verification/verification.png")

    finally:
        browser.close()

with sync_playwright() as playwright:
    run(playwright)
