const base = "";

async function handle(res) {
  if (!res.ok) {
    const text = await res.text().catch(() => "");
    throw new Error(text || `HTTP ${res.status}`);
  }
  const ct = res.headers.get("content-type") || "";
  return ct.includes("application/json") ? res.json() : res.text();
}

export const api = {
  scan: () => fetch(`${base}/api/v1/wifi/scan`).then(handle),
  saveCreds: ({ ssid, password }) =>
    fetch(`${base}/api/v1/provision/wifi`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ ssid, password }),
    }).then(handle),
  retryConnect: () =>
    fetch(`${base}/api/v1/provision/retry`, { method: "POST" }).then(handle),
  status: () => fetch(`${base}/api/v1/wifi/status`).then(handle),

  setupStatus: () => fetch(`${base}/api/v1/setup/status`).then(handle),
  setRole: (role) =>
    fetch(`${base}/api/v1/device/role`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ role }),
    }).then(handle),

  saveUserCreds: (email, password) =>
    fetch(`${base}/api/v1/provision/user`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ email, password }),
    }).then(handle),

  // --- NEW: Cloud device and mesh APIs ---
  getDevice: () => fetch(`${base}/api/v1/cloud/device`).then(handle),
  registerDevice: (name, role) =>
    fetch(`${base}/api/v1/cloud/device/register`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ name, role }),
    }).then(handle),
  getMeshes: () => fetch(`${base}/api/v1/cloud/meshes`).then(handle),
  bindDevice: (meshId) =>
    fetch(`${base}/api/v1/cloud/device/bind`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ meshId }),
    }).then(handle),

  setDebug: (enabled) =>
    fetch(`${base}/api/v1/device/debug`, {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ enabled }),
    }).then(handle),
};
