from playwright.sync_api import sync_playwright
import time

def run(playwright):
    browser = playwright.chromium.launch()
    context = browser.new_context()
    page = context.new_page()

    # Register
    page.goto("http://localhost:3000/register")
    page.wait_for_load_state('networkidle')
    time.sleep(2) # Just in case
    page.get_by_label("Name").fill("Test User")
    page.get_by_label("Email").fill("test@example.com")
    page.get_by_label("Password").fill("password123")
    page.get_by_role("button", name="Register").click()

    # Wait for registration to complete and redirect to login
    page.wait_for_url("http://localhost:3000/login")
    page.wait_for_load_state('networkidle')
    time.sleep(2) # Just in case

    # Login
    page.get_by_label("Email address").fill("test@example.com")
    page.get_by_label("Password").fill("password123")
    page.get_by_role("button", name="Sign in").click()

    page.wait_for_url("http://localhost:3000/")
    page.wait_for_load_state('networkidle')

    page.screenshot(path="jules-scratch/verification/verification.png")

    context.close()
    browser.close()

with sync_playwright() as playwright:
    run(playwright)
