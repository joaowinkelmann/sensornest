<template>
  <q-page padding class="q-pa-md row justify-center">
    <div class="col-12 col-sm-8 col-md-6">
      <q-card flat bordered>
        <q-card-section>
          <div class="text-h6">{{ $t("deviceSetup") }}</div>
          <div class="text-subtitle2">{{ $t("setupSubtitle") }}</div>
        </q-card-section>

        <q-linear-progress :value="progressValue" color="primary" class="q-mb-sm" />

        <q-card-section class="q-pt-none">
          <q-banner v-if="error" class="q-mb-md" dense rounded inline-actions type="negative">
            {{ error }}
            <template #action>
              <q-btn flat color="white" :label="$t('dismiss')" @click="error = ''" />
            </template>
          </q-banner>

          <!-- Role selection -->
          <div v-if="!role" class="row q-col-gutter-md q-mb-md">
            <div class="col-12 text-subtitle1">{{ $t("selectRole") }}</div>
            <div class="col-12 col-md-6">
              <q-card :class="cardClass('CONTROLLER')" @click="pickRole('CONTROLLER')">
                <q-card-section>
                  <div class="text-bold">{{ $t("controller") }}</div>
                  <div>{{ $t("controllerDescription") }}</div>
                </q-card-section>
              </q-card>
            </div>
            <div class="col-12 col-md-6">
              <q-card :class="cardClass('WORKER')" @click="pickRole('WORKER')">
                <q-card-section>
                  <div class="text-bold">{{ $t("worker") }}</div>
                  <div>{{ $t("workerDescription") }}</div>
                </q-card-section>
              </q-card>
            </div>
            <!-- DEBUG TOGGLE -->
            <div class="col-12 q-mt-md">
              <q-toggle
                v-model="debugEnabled"
                :label="$t('debugMode')"
                :hint="$t('debugModeHint')"
                @update:model-value="toggleDebug"
                color="primary"
              />
            </div>
          </div>

          <!-- Wi-Fi setup -->
          <div v-if="role && !connected">
            <q-select
              v-model="ssid"
              filled
              :label="$t('selectNetwork')"
              :options="ssidOptions"
              emit-value
              map-options
              :loading="loadingScan"
              use-input
              @filter="filterSSIDs"
            >
              <template #append>
                <q-btn round dense flat icon="refresh" @click="scan" />
              </template>
            </q-select>

            <q-input
              v-if="ssid"
              v-model="password"
              class="q-mt-md"
              filled
              :label="$t('enterPassword')"
              :type="passwordVisible ? 'text' : 'password'"
            >
              <template #append>
                <q-icon
                  :name="passwordVisible ? 'visibility_off' : 'visibility'"
                  class="cursor-pointer"
                  @click="passwordVisible = !passwordVisible"
                />
              </template>
            </q-input>

            <q-btn
              class="q-mt-md"
              color="primary"
              :label="$t('connect')"
              :disable="!ssid || !password"
              :loading="connecting"
              @click="connect"
            />
          </div>

          <!-- Connection status -->
          <div v-if="connected">
            <q-icon name="check_circle" color="positive" size="xl" />
            <div class="text-h6">{{ $t("connected") }}</div>
            <div>{{ $t("ipAddress") }} {{ status.ip }}</div>
            <div>{{ $t("ssid") }} {{ status.ssid }}</div>
          </div>

          <!-- Login Form -->
          <div v-if="connected && !loggedIn">
            <div class="text-h6 q-mt-lg">{{ $t("cloudLogin") }}</div>
            <q-input
              v-model="userEmail"
              class="q-mt-md"
              filled
              :label="$t('email')"
              type="email"
              :disable="loginLoading"
            />
            <q-input
              v-model="userPassword"
              class="q-mt-md"
              filled
              :label="$t('password')"
              :type="loginPasswordVisible ? 'text' : 'password'"
              :disable="loginLoading"
            >
              <template #append>
                <q-icon
                  :name="loginPasswordVisible ? 'visibility_off' : 'visibility'"
                  class="cursor-pointer"
                  @click="loginPasswordVisible = !loginPasswordVisible"
                />
              </template>
            </q-input>
            <div class="row q-mt-md q-gutter-sm">
              <q-btn
                color="primary"
                :label="$t('login')"
                :disable="!userEmail || !userPassword"
                :loading="loginLoading"
                @click="doLogin"
              />
              <q-btn
                color="secondary"
                outline
                :label="$t('register')"
                disable
              />
            </div>
          </div>

          <!-- Device Registration -->
          <div v-if="loggedIn && !device">
            <div class="text-h6 q-mt-lg">{{ $t("registerDeviceTitle") }}</div>
            <q-input
              v-model="newDeviceName"
              class="q-mt-md"
              filled
              :label="$t('deviceName')"
              :hint="$t('deviceNameHint')"
            />
            <q-btn
              class="q-mt-md"
              color="primary"
              :label="$t('registerDeviceBtn')"
              :disable="!newDeviceName"
              @click="registerDevice"
            />
          </div>

          <!-- Mesh Selection -->
          <div v-if="loggedIn && device && (!device.meshId || changingMesh)">
            <div class="text-h6 q-mt-lg">{{ $t("selectMeshTitle") }}</div>
            <q-select
              v-model="selectedMeshId"
              class="q-mt-md"
              filled
              :label="$t('selectMesh')"
              :options="meshOptions"
              emit-value
              map-options
              :loading="!meshes.length"
            />
            <q-btn
              class="q-mt-md"
              color="primary"
              :label="$t('bindDeviceBtn')"
              :disable="!selectedMeshId"
              @click="bindDevice"
            />
          </div>

          <!-- Device Bound Status -->
          <div v-if="loggedIn && device && device.meshId && !changingMesh">
            <q-icon name="cloud_done" color="positive" size="xl" class="q-mt-lg"/>
            <div class="text-h6">{{ $t("deviceBoundTitle") }}</div>
            <div v-if="currentMesh" class="text-subtitle1">
              {{ $t("currentMesh") }}: <strong>{{ currentMesh.name }}</strong>
            </div>
            <div class="row q-mt-md q-gutter-sm">
              <q-btn
                color="primary"
                :label="$t('completeSetupBtn')"
                @click="completeSetup"
              />
              <q-btn
                color="secondary"
                outline
                :label="$t('changeMeshBtn')"
                @click="changeMesh"
              />
            </div>
          </div>

          <!-- Final Status -->
          <div v-if="setupDone">
             <q-icon name="cloud_done" color="positive" size="xl" class="q-mt-lg"/>
             <div class="text-h6">{{ $t('setupComplete') }}</div>
             <div class="text-subtitle2">{{ $t('deviceWillRestart') }}</div>
          </div>

          <!-- Retry button -->
          <div v-if="status?.error">
            <q-btn color="negative" :label="$t('retry')" @click="retry" />
          </div>
        </q-card-section>

        <q-card-actions align="around" class="bg-grey-1">
          <div class="text-caption">{{ statusText }}</div>
        </q-card-actions>
      </q-card>
    </div>
  </q-page>
</template>

<script setup>
import { api } from "@/services/api";
import { useI18n } from "vue-i18n";
import { useQuasar } from "quasar";

const { t } = useI18n();
const $q = useQuasar();

const role = ref(null); // 'CONTROLLER' or 'WORKER'
const ssid = ref("");
const password = ref("");
const ssidList = ref([]);
const passwordVisible = ref(false);
const loadingScan = ref(false);
const connecting = ref(false);
const error = ref("");
const status = ref(null);
const pollTimer = ref(null);
const debugEnabled = ref(false);

// --- NEW: Login and device/mesh state ---
const userEmail = ref("");
const userPassword = ref("");
const loginPasswordVisible = ref(false);
const loginLoading = ref(false);
const loggedIn = ref(false);
const device = ref(null);
const newDeviceName = ref("");
const meshes = ref([]);
const selectedMeshId = ref(null);
const changingMesh = ref(false);
const setupDone = ref(false);


const connected = computed(() => !!status.value?.connected);
const step = computed(() => {
  if (!role.value) return 0;
  // After role selection, if we are not connected to Wi-Fi yet, we are at the Wi-Fi steps.
  if (!connected.value) {
    if (connecting.value) return 2; // Connecting to Wi-Fi
    return 1; // Selecting network / entering password
  }
  // Once connected to Wi-Fi, the steps depend on cloud status
  if (!loggedIn.value) return 3; // Login to cloud
  if (!device.value) return 4; // Register device
  if (!device.value.meshId || changingMesh.value) return 5; // Select mesh
  return 6; // Complete
});

const progressValue = computed(() => {
  // Map step to a progress value from 0 to 1
  const stepProgress = {
    0: 0,       // Role
    1: 1/6,     // Wi-Fi Select
    2: 2/6,     // Wi-Fi Connecting
    3: 3/6,     // Login
    4: 4/6,     // Register
    5: 5/6,     // Bind Mesh
    6: 1,       // Done
  };
  return stepProgress[step.value] || 0;
});
const statusText = computed(() => {
  if (!role.value) return t("status_chooseRole");
  if (!connected.value) {
    if (connecting.value) return t("status_connecting");
    if (ssid.value) return t("status_enterPassword");
    return t("status_selectNetwork");
  }
  if (!loggedIn.value) return t("status_login");
  if (!device.value) return t("status_registerDevice");
  if (!device.value.meshId || changingMesh.value) return t("status_selectMesh");
  return t("status_complete");
});
const ssidOptions = computed(() =>
  ssidList.value.map((ap) => ({
    label: `${ap.ssid}  (RSSI ${ap.rssi})`,
    value: ap.ssid,
  }))
);

const meshOptions = computed(() =>
  meshes.value.map((mesh) => ({ label: mesh.name, value: mesh.id }))
);

const currentMesh = computed(() =>
  meshes.value.find((m) => m.id === device.value?.meshId)
);

function cardClass(r) {
  return role.value === r ? "bg-grey-2" : "";
}

async function pickRole(r) {
  try {
    await api.setRole(r);
    role.value = r;
    // After choosing role, start scanning Wi-Fi
    scan();
  } catch (e) {
    error.value = e.message || t("failedToSaveRole");
  }
}

async function toggleDebug(enabled) {
  try {
    const response = await api.setDebug(enabled);
    if (response.restarting) {
      $q.notify({
        type: "positive",
        message: t("debugModeToggled"),
        caption: t("deviceRestarting"),
        position: "top",
      });
      setTimeout(() => {
      }, 1000);
    }
  } catch (e) {
    error.value = e.message || t("failedToToggleDebug");
    // Revert the toggle on error
    debugEnabled.value = !enabled;
  }
}

function filterSSIDs(val, update) {
  update(() => {
    if (!val) return;
  });
}

async function scan() {
  loadingScan.value = true;
  error.value = "";
  try {
    const list = await api.scan();
    const unique = Object.values(
      list.reduce((acc, it) => {
        const k = it.ssid;
        if (!k) return acc;
        if (!acc[k] || acc[k].rssi < it.rssi) acc[k] = it;
        return acc;
      }, {})
    );
    ssidList.value = unique.sort((a, b) => b.rssi - a.rssi);
  } catch (e) {
    error.value = e.message || t("failedToScan");
  } finally {
    loadingScan.value = false;
  }
}

async function doLogin() {
  loginLoading.value = true;
  error.value = "";
  try {
    const loginResponse = await api.saveUserCreds(userEmail.value, userPassword.value);
    console.log("Login Response:", loginResponse); // <-- Existing log
    console.log("Device object from login:", loginResponse.device); // <-- ADDED
    loggedIn.value = true;
    // q notify ta crashando por enquanto, debugar o que tem de errado com esses toasts
    // $q.notify({
    //   type: "positive",
    //   message: t("loginSuccess")
    // });
    // --- Process device info from the login response ---
    device.value = loginResponse.device;
    console.log("Device state after login:", device.value); // <-- ADDED
    if (device.value?.id) {
      console.log("Device has ID, fetching meshes..."); // <-- ADDED
      await fetchMeshes(); // Fetch meshes to show name or for selection
    } else {
      console.log("Device has no ID, proceeding to registration step."); // <-- ADDED
    }
  } catch (e) {
    if (e.response?.status === 401) {
      error.value = t("invalidCredentials");
    } else {
      error.value = e.message || t("loginFailed");
    }
  } finally {
    loginLoading.value = false;
  }
}

// --- NEW: Methods for device and mesh flow ---
async function checkDevice() {
  console.log("Checking device status from server..."); // <-- UPDATED
  error.value = "";
  try {
    // This call is redundant if login response already provides the device.
    // We keep it for flows where login is not the entry point (e.g. page refresh).
    const dev = await api.getDevice();
    console.log("getDevice API response:", dev); // <-- UPDATED
    device.value = dev;
    console.log("Device state after checkDevice:", device.value); // <-- ADDED
    // Always fetch meshes to display the name if already bound
    if (device.value?.id) {
      console.log("Device has ID, fetching meshes..."); // <-- ADDED
      await fetchMeshes();
    }
  } catch (e) {
    if (e.response?.status === 404) {
      // Device not found, needs registration
      device.value = null;
    } else {
      error.value = e.message || t("failedToCheckDevice");
    }
  }
}

async function registerDevice() {
  error.value = "";
  try {
    const regResponse = await api.registerDevice(newDeviceName.value, role.value);
    console.log("Register Response:", regResponse); // <-- Added log
    $q.notify({ type: "positive", message: t("deviceRegistered") });
    await checkDevice(); // Re-check to proceed to mesh selection
  } catch (e) {
    error.value = e.message || t("failedToRegisterDevice");
  }
}

async function fetchMeshes() {
  error.value = "";
  try {
    const meshesResponse = await api.getMeshes();
    console.log("Meshes:", meshesResponse); // <-- Added log
    meshes.value = meshesResponse;
  } catch (e) {
    error.value = e.message || t("failedToFetchMeshes");
  }
}

async function bindDevice() {
  error.value = "";
  try {
    const bindResponse = await api.bindDevice(selectedMeshId.value);
    console.log("Bind Response:", bindResponse); // <-- Added log
    $q.notify({ type: "positive", message: t("deviceBound") });
    await checkDevice(); // Re-check to update UI
    changingMesh.value = false;
  } catch (e) {
    error.value = e.message || t("failedToBindDevice");
  }
}

async function changeMesh() {
  changingMesh.value = true;
  selectedMeshId.value = null;
  await fetchMeshes();
}

function completeSetup() {
  setupDone.value = true;
  // Optionally, you could trigger a device restart here
  setTimeout(() => window.location.reload(), 5000);
}


async function connect() {
  connecting.value = true;
  error.value = "";
  try {
    await api.saveCreds({ ssid: ssid.value, password: password.value });
    await api.retryConnect();
    startPolling();
  } catch (e) {
    error.value = e.message || t("failedToConnect");
    connecting.value = false;
  }
}

async function retry() {
  connecting.value = true;
  error.value = "";
  try {
    await api.retryConnect();
    startPolling();
  } catch (e) {
    error.value = e.message || t("failedToConnect");
    connecting.value = false;
  }
}

async function checkStatus() {
  try {
    const newStatus = await api.status();
    status.value = newStatus;
    if (newStatus.connected) {
      stopPolling();
      connecting.value = false;
    }
  } catch (e) {
    // Ignore poll errors
  }
}

function startPolling() {
  stopPolling();
  pollTimer.value = setInterval(checkStatus, 2000);
  checkStatus(); // immediate check
}

function stopPolling() {
  if (pollTimer.value) {
    clearInterval(pollTimer.value);
    pollTimer.value = null;
  }
}

async function fetchInitialStatus() {
  try {
    const setupStatus = await api.setupStatus();
    console.log("Initial setup status:", setupStatus); // <-- ADDED
    role.value = setupStatus.role;
    debugEnabled.value = setupStatus.debug;

    if (setupStatus.stage === 'user_saved' || setupStatus.stage === 'mesh_selected') {
        console.log("Stage is 'user_saved' or 'mesh_selected', setting loggedIn and checking device."); // <-- ADDED
        loggedIn.value = true;
        // If logged in, check device status immediately
        await checkDevice();
    }
    if (setupStatus.connected) {
      status.value = {
        connected: true,
        ip: setupStatus.ip,
        ssid: setupStatus.ssid,
      };
    } else if (setupStatus.stage === "connecting") {
      connecting.value = true;
      startPolling();
    } else if (setupStatus.role) {
      scan();
    }
  } catch (e) {
    error.value = e.message || "Failed to get initial status";
  }
}

onMounted(() => {
  fetchInitialStatus();
});

onUnmounted(() => {
  stopPolling();
});
</script>

<style scoped>
/* minimal */
</style>