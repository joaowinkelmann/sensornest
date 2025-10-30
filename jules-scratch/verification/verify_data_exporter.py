from playwright.sync_api import sync_playwright

def run(playwright):
    browser = playwright.chromium.launch(headless=True)
    context = browser.new_context()
    page = context.new_page()
    page.goto("http://localhost:3000/sensornest/#/login")
    page.get_by_label("Email address").fill("test@example.com")
    page.get_by_label("Password").fill("password123")
    page.get_by_role("button", name="Sign in").click()
    page.wait_for_timeout(5000) # wait for 5 seconds
    page.screenshot(path="jules-scratch/verification/debug.png")
    page.goto("http://localhost:3000/sensornest/#/data")
    page.get_by_label("Entity Type").click()
    page.get_by_text("Mesh").click()
    page.screenshot(path="jules-scratch/verification/verification.png")
    browser.close()

with sync_playwright() as playwright:
    run(playwright)
