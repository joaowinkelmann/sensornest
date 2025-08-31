const base = "";

async function handle(res) {
  if (!res.ok) {
    const text = await res.text().catch(() => "");
    throw new Error(text || `HTTP ${res.status}`);
  }
  const ct = res.headers.get("content-type") || "";
  return ct.includes("application/json") ? res.json() : res.text();
}

async function doFetch(url, options = {}) {
  const startTime = performance.now();

  console.debug("API Request:", {
    url,
    method: options.method || "GET",
    headers: options.headers || {},
    body: options.body ? JSON.parse(options.body) : null,
    timestamp: new Date().toISOString(),
  });

  try {
    const res = await fetch(url, options);
    const endTime = performance.now();
    const duration = Math.round(endTime - startTime);

    // Extract response headers into a clean object
    const responseHeaders = {};
    res.headers.forEach((value, key) => {
      responseHeaders[key] = value;
    });

    let responseBody;
    let bodyType;

    try {
      responseBody = await res.clone().json();
      bodyType = "json";
    } catch {
      try {
        responseBody = await res.clone().text();
        bodyType = "text";
      } catch {
        responseBody = "[Unable to read response body]";
        bodyType = "error";
      }
    }

    // Create a comprehensive log object
    const logData = {
      request: {
        url,
        method: options.method || "GET",
        headers: options.headers || {},
        body: options.body ? JSON.parse(options.body) : null,
      },
      response: {
        status: res.status,
        statusText: res.statusText,
        ok: res.ok,
        headers: responseHeaders,
        body: responseBody,
        bodyType,
        size: responseHeaders["content-length"]
          ? `${responseHeaders["content-length"]} bytes`
          : "unknown",
      },
      timing: {
        duration: `${duration}ms`,
        timestamp: new Date().toISOString(),
      },
    };

    // Log with appropriate emoji based on status
    const statusEmoji = res.ok ? "✅" : "❌";

    console.debug(`${statusEmoji} | API Response [${res.status}]:`, logData);

    // Also log a compact version for quick scanning
    console.debug(
      `Quick Summary: ${options.method || "GET"} ${url} → ${res.status} (${duration}ms)`,
    );

    return handle(res);
  } catch (error) {
    const endTime = performance.now();
    const duration = Math.round(endTime - startTime);

    console.error("API Request Failed:", {
      request: {
        url,
        method: options.method || "GET",
        headers: options.headers || {},
        body: options.body ? JSON.parse(options.body) : null,
      },
      error: {
        message: error.message,
        name: error.name,
        stack: error.stack,
      },
      timing: {
        duration: `${duration}ms`,
        timestamp: new Date().toISOString(),
      },
    });

    throw error;
  }
}

export const api = {
  scan: () => doFetch(`${base}/api/v1/wifi/scan`),
  saveCreds: ({ ssid, password }) =>
    doFetch(`${base}/api/v1/provision/wifi`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ ssid, password }),
    }),
  retryConnect: () =>
    doFetch(`${base}/api/v1/provision/retry`, { method: "POST" }),
  status: () => doFetch(`${base}/api/v1/wifi/status`),

  setupStatus: () => doFetch(`${base}/api/v1/setup/status`),
  setRole: (role) =>
    doFetch(`${base}/api/v1/device/role`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ role }),
    }),

  saveUserCreds: (email, password) =>
    doFetch(`${base}/api/v1/provision/user`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ email, password }),
    }),

  // --- NEW: Cloud device and mesh APIs ---
  getDevice: () => doFetch(`${base}/api/v1/cloud/device`),
  registerDevice: (name, role, meshId, readingsPerBatch, wakeUpInterval) =>
    doFetch(`${base}/api/v1/cloud/device/register`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        name,
        role,
        meshId,
        readingsPerBatch,
        wakeUpInterval,
      }),
    }),
  getMeshes: () => doFetch(`${base}/api/v1/cloud/meshes`),

  updateDevice: (name, readingsPerBatch, wakeUpInterval, meshId, role) =>
    doFetch(`${base}/api/v1/cloud/device/update`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({
        name,
        readingsPerBatch,
        wakeUpInterval,
        meshId,
        role,
      }),
    }),

  setDebug: (enabled) =>
    doFetch(`${base}/api/v1/device/debug`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ enabled }),
    }),

  completeSetup: () =>
    doFetch(`${base}/api/v1/setup/complete`, { method: "POST" }),
};
