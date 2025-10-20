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
          <div v-if="role && role !== 'WORKER' && !connected">
            <q-btn flat dense icon="arrow_back" @click="goBack" class="q-mb-md" :label="$t('back')" />
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

          <!-- Worker controller selection (moved earlier for clarity) -->
          <div v-if="role==='WORKER' && !setupDone" class="q-mt-md">
            <q-btn flat dense icon="arrow_back" @click="goBack" class="q-mb-md" :label="$t('back')" />
            <div class="text-h6">{{$t('selectController')}}</div>
            <q-select
              v-model="selectedControllerMac"
              :options="controllers.map(c=>({label:`${c.name||'Controller'} (${c.mac})`, value:c.mac}))"
              emit-value
              map-options
              filled class="q-mt-sm"
              :label="$t('availableControllers')"
              :loading="controllersLoading"
            >
              <template #append>
                <q-btn dense flat round icon="refresh" @click="scanControllers"/>
              </template>
            </q-select>
            <q-input v-model="workerName" filled :label="$t('workerName')" class="q-mt-sm"/>
            <q-btn class="q-mt-md" color="primary"
                   :disable="!workerName || !selectedControllerMac"
                   :loading="bindingWorker"
                   :label="$t('bindWorker')" @click="bindWorker"/>
          </div>

          <!-- Login Form -->
          <div v-if="role!=='WORKER' && connected && !loggedIn">
            <q-btn flat dense icon="arrow_back" @click="goBack" class="q-mb-md" :label="$t('back')" />
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

          <!-- Mesh Selection -->
          <div v-if="role!=='WORKER' && loggedIn && (!device?.meshId || changingMesh)">
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

            <!-- Placeholder for creating a new mesh -->
            <div class="q-mt-sm text-caption">
              Need a new mesh? <a href="#" @click.prevent>Create one here</a> (coming soon).
            </div>

            <!-- If device is not yet registered, show registration form -->
            <div v-if="!device" class="q-mt-lg">
              <div class="text-subtitle1">{{ $t("registerDeviceTitle") }}</div>
              <q-input
                v-model="newDeviceName"
                class="q-mt-md"
                filled
                :label="$t('deviceName')"
                :hint="$t('deviceNameHint')"
              />
              <div class="row q-col-gutter-md q-mt-xs">
                <div class="col-6">
                  <q-input
                    v-model.number="newDeviceReadingsPerBatch"
                    type="number"
                    filled
                    :label="$t('readingsPerBatch')"
                    :hint="$t('readingsPerBatchHint')"
                  />
                </div>
                <div class="col-6">
                  <q-input
                    v-model.number="newDeviceWakeUpInterval"
                    type="number"
                    filled
                    :label="$t('wakeUpInterval')"
                    :hint="$t('wakeUpIntervalHint')"
                  />
                </div>
              </div>
              <q-btn
                class="q-mt-md"
                color="primary"
                :label="$t('registerDeviceBtn')"
                :disable="!selectedMeshId || !newDeviceName"
                @click="registerDevice"
              />
            </div>

            <!-- If device exists and user is editing params, show editable fields -->
            <div v-else-if="changingMesh" class="q-mt-lg">
              <div class="text-subtitle1">{{ $t("registerDeviceTitle") }}</div>
              <q-input
                v-model="newDeviceName"
                class="q-mt-md"
                filled
                :label="$t('deviceName')"
              />
              <div class="row q-col-gutter-md q-mt-xs">
                <div class="col-6">
                  <q-input
                    v-model.number="newDeviceReadingsPerBatch"
                    type="number"
                    filled
                    :label="$t('readingsPerBatch')"
                  />
                </div>
                <div class="col-6">
                  <q-input
                    v-model.number="newDeviceWakeUpInterval"
                    type="number"
                    filled
                    :label="$t('wakeUpInterval')"
                  />
                </div>
              </div>
              <div class="row q-mt-md q-gutter-sm">
                <q-btn
                  color="primary"
                  :label="$t('updateDeviceBtn')"
                  :disable="!selectedMeshId"
                  :loading="updatingDeviceLoading"
                  @click="asyncSaveDeviceParams"
                />
              </div>
            </div>

            <!-- Show bind button only if device exists but is not bound -->
            <q-btn
              v-if="device && !device.meshId"
              class="q-mt-md"
              color="primary"
              :label="$t('updateDeviceBtn')"
              :disable="!selectedMeshId"
              @click="updateDevice"
            />
          </div>

          <!-- Device Bound Status -->
          <div v-if="role!=='WORKER' && loggedIn && device && device.meshId && !changingMesh">
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
                :loading="completingSetup"
                :disable="completingSetup"
              />
              <q-btn
                color="secondary"
                outline
                :label="$t('changeDeviceParams')"
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
          <div class="text-caption">
            {{ statusText }}
            <template v-if="connected">
              • {{ $t("connected") }}
              • {{ $t("ipAddress") }} {{ status.ip }}
              • {{ $t("ssid") }} {{ status.ssid }}
            </template>
          </div>
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

const userEmail = ref("");
const userPassword = ref("");
const loginPasswordVisible = ref(false);
const loginLoading = ref(false);
const loggedIn = ref(false);
const device = ref(null);
const newDeviceName = ref("");
const newDeviceReadingsPerBatch = ref(5); // Default value
const newDeviceWakeUpInterval = ref(15); // Default value
const meshes = ref([]);
const selectedMeshId = ref(null);
const changingMesh = ref(false);
const setupDone = ref(false);
const completingSetup = ref(false); // <--- ADDED
const updatingDeviceLoading = ref(false);

const controllers = ref([]);
const selectedControllerMac = ref("");
const workerName = ref("");
const controllersLoading = ref(false);
const bindingWorker = ref(false); // <--- MISSING REF (caused bind to abort with ReferenceError)

const connected = computed(() => !!status.value?.connected);
const step = computed(() => {
  if (!role.value) return 0;
  if (role.value === 'WORKER') return 1; // worker single step
  // After role selection, if we are not connected to Wi-Fi yet, we are at the Wi-Fi steps.
  if (!connected.value) {
    if (connecting.value) return 2; // Connecting to Wi-Fi
    return 1; // Selecting network / entering password
  }
  // Once connected to Wi-Fi, the steps depend on cloud status
  if (!loggedIn.value) return 3; // Login to cloud
  if (!device?.meshId || changingMesh.value) return 4; // Select mesh and/or register device
  return 5; // Complete
});

const progressValue = computed(() => {
  if (role.value === 'WORKER') {
    return selectedControllerMac.value ? 1 : 0.5;
  }
  // Map step to a progress value from 0 to 1
  const stepProgress = {
    0: 0,       // Role
    1: 1/5,     // Wi-Fi Select
    2: 2/5,     // Wi-Fi Connecting
    3: 3/5,     // Login
    4: 4/5,     // Select Mesh / Register
    5: 1,       // Done
  };
  return stepProgress[step.value] || 0;
});
const statusText = computed(() => {
  if (!role.value) return t("status_chooseRole");
  if (role.value === 'WORKER') return t("selectController");
  if (!connected.value) {
    if (connecting.value) return t("status_connecting");
    if (ssid.value) return t("status_enterPassword");
    return t("status_selectNetwork");
  }
  if (!loggedIn.value) return t("status_login");
  if (!device?.meshId || changingMesh.value) return t("status_selectMesh");
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

function showError(e, fallbackKey) {
  let message = "";
  try {
    // API errors are expected to be JSON strings like '{"error":"key"}'
    const errorBody = JSON.parse(e.message);
    if (errorBody.error) {
      message = t("error_" + errorBody.error) || t(fallbackKey) || errorBody.error;
    }
  } catch (jsonError) {
    // Fallback for non-JSON errors or if the key doesn't exist
    console.warn("Error parsing error message:", jsonError);
    message = t(fallbackKey) || e.message;
  }
  $q.notify({
    type: "negative",
    message: message,
  });
}

async function goBack() {
  // Simple case: just go back to role selection
  if (role.value) {
    try {
      await api.clearRole();
      role.value = null;
      // Reset dependent state
      status.value = null;
      loggedIn.value = false;
      device.value = null;
      ssid.value = "";
      password.value = "";
    } catch (e) {
      showError(e, "failedToGoBack");
    }
  }
}

async function pickRole(r) {
  try {
    await api.setRole(r);
    role.value = r;
    if (r === 'WORKER') {
      scanControllers();
    } else {
      scan();
    }
  } catch (e) {
    showError(e, "failedToSaveRole");
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
    showError(e, "failedToToggleDebug");
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

    // Automatically select the strongest SSID if the user hasn't picked one yet
    if (!ssid.value && ssidList.value.length > 0) {
      ssid.value = ssidList.value[0].ssid;
    }
  } catch (e) {
    showError(e, "failedToScan");
  } finally {
    loadingScan.value = false;
  }
}

async function scanControllers() {
  controllersLoading.value = true;
  try {
    controllers.value = await api.getControllers();
    
    // pega o primeiro controller pra preencher
    if (controllers.value.length > 0 && !selectedControllerMac.value) {
      selectedControllerMac.value = controllers.value[0].mac;
    }
  } catch (e) {
    showError(e, "failedToScanControllers");
  } finally {
    controllersLoading.value = false;
  }
}

async function bindWorker() {
  if (!workerName.value || !selectedControllerMac.value) {
    return;
  }
  const controller = controllers.value.find(c => c.mac === selectedControllerMac.value);
  if (!controller) {
    showError(t("controllerNotFount"), "failedToBindWorker");
    return;
  }

  bindingWorker.value = true;
  connecting.value = true;
  try {
    await api.bindWorker(workerName.value, selectedControllerMac.value, controller.channel);
    $q.notify({
      type: "positive",
      message: t("workerBound"),
    });
    setupDone.value = true;
  } catch (e) {
    console.error("[bindWorker] error", e);
    showError(e, "failedToBindWorker");
  } finally {
    bindingWorker.value = false;
    connecting.value = false;
    console.log("[bindWorker] finished");
  }
}

async function doLogin() {
  loginLoading.value = true;
  try {
    const loginResponse = await api.saveUserCreds(userEmail.value, userPassword.value);
    console.log("Login Response:", loginResponse); // <-- Existing log
    console.log("Device object from login:", loginResponse.device); // <-- ADDED
    loggedIn.value = true;
    $q.notify({
      type: "positive",
      message: t("loginSuccess")
    });
    device.value = loginResponse.device;
    console.log("Device state after login:", device.value); // <-- ADDED
    // Always fetch meshes after login to prepare for the next step
    await fetchMeshes();
  } catch (e) {
    showError(e, "loginFailed");
  } finally {
    loginLoading.value = false;
  }
}

async function checkDevice() {
  console.log("Checking device status from server...");
  try {
    // This call is redundant if login response already provides the device.
    // We keep it for flows where login is not the entry point (e.g. page refresh).
    const dev = await api.getDevice();
    console.log("getDevice API response:", dev);
    device.value = dev;
    console.log("Device state after checkDevice:", device.value);
    // Always fetch meshes to display the name if already bound
    if (device.value?.id) {
      console.log("Device has ID, fetching meshes...");
      await fetchMeshes();
    }
  } catch (e) {
    // Don't show error for 404, it's an expected state (device not registered)
    if (e.message.includes("404")) {
       device.value = null;
    } else {
      showError(e, "failedToCheckDevice");
    }
  }
}

async function registerDevice() {
  try {
    // Pass selected meshId and new params during registration
    const regResponse = await api.registerDevice(
      newDeviceName.value,
      role.value,
      selectedMeshId.value,
      newDeviceReadingsPerBatch.value,
      newDeviceWakeUpInterval.value
    );
    console.log("Register Response:", regResponse);
    $q.notify({ type: "positive", message: t("deviceRegistered") });
    await checkDevice(); // Re-check to proceed to mesh selection
  } catch (e) {
    showError(e, "failedToRegisterDevice");
  }
}

async function fetchMeshes() {
  try {
    const meshesResponse = await api.getMeshes();
    console.log("Meshes:", meshesResponse);
    meshes.value = meshesResponse;
  } catch (e) {
    showError(e, "failedToFetchMeshes");
  }
}

async function updateDevice() {
  updatingDeviceLoading.value = true;
  try {
    // Use updateDevice to set mesh + optional name/readings/interval
    const resp = await api.updateDevice(
      newDeviceName.value || undefined,
      newDeviceReadingsPerBatch.value || undefined,
      newDeviceWakeUpInterval.value || undefined,
      selectedMeshId.value || undefined
    );
    updatingDeviceLoagind.value = false;
    console.log("Update Response:", resp);
    $q.notify({ type: "positive", message: t("deviceBound") });
    await checkDevice(); // Re-check to update UI
    changingMesh.value = false;
  } catch (e) {
    showError(e, "failedToupdateDevice");
  }
}

async function changeMesh() {
  // kept for backward compatibility: redirect to the new "changeDeviceParams" flow
  return changeDeviceParams();
}

async function changeDeviceParams() {
  changingMesh.value = true;
  // prefill with current device values when available
  selectedMeshId.value = device.value?.meshId || null;
  newDeviceName.value = device.value?.name || newDeviceName.value || "";
  newDeviceReadingsPerBatch.value = device.value?.readingsPerBatch || newDeviceReadingsPerBatch.value;
  newDeviceWakeUpInterval.value = device.value?.wakeUpInterval || newDeviceWakeUpInterval.value;
  await fetchMeshes();
}

// Save handler used by the edit screen: bind if mesh changed, or register if device missing
async function asyncSaveDeviceParams() {
  try {
    if (!device.value) {
      // device not registered yet -> register
      await registerDevice();
    } else {
      // If mesh changed or other params changed, call updateDevice
      await api.updateDevice(
        newDeviceName.value || undefined,
        newDeviceReadingsPerBatch.value || undefined,
        newDeviceWakeUpInterval.value || undefined,
        selectedMeshId.value || device.value.meshId || undefined,
        role.value
      );
      // Note: device name / interval update endpoint now implemented
      $q.notify({ type: "positive", message: t("deviceBound") });
      changingMesh.value = false;
      await checkDevice();
    }
  } catch (e) {
    showError(e, "failedToupdateDevice");
  }
}

async function completeSetup() {
  if (completingSetup.value) return;
  completingSetup.value = true;
  try {
    // Call endpoint (device will actually restart almost immediately)
    await api.completeSetup();
    // Inform user and keep UI for ~10s so they can safely disconnect
    $q.notify({
      type: "positive",
      message: t("setupFinalizing"),
      caption: t("setupDisconnectHint"),
      timeout: 10000,
      actions: [{ label: t("dismiss"), color: "white" }]
    });
    await new Promise(r => setTimeout(r, 10000));
    setupDone.value = true;
  } catch (e) {
    showError(e, "failedToCompleteSetup");
  } finally {
    completingSetup.value = false;
  }
}

async function connect() {
  connecting.value = true;
  try {
    await api.saveCreds({ ssid: ssid.value, password: password.value });
    await api.retryConnect();
    startPolling();
  } catch (e) {
    showError(e, "failedToConnect");
    connecting.value = false;
  }
}

async function retry() {
  connecting.value = true;
  try {
    await api.retryConnect();
    startPolling();
  } catch (e) {
    showError(e, "failedToConnect");
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
    role.value = setupStatus.role;
    debugEnabled.value = setupStatus.debug;
    if (role.value === 'WORKER') {
      // Skip Wi-Fi / cloud; directly begin controller discovery
      scanControllers();
      return;
    }
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
    showError(e, "failedToGetInitialStatus");
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