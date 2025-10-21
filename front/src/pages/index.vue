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

          <div v-if="role!=='WORKER' && connected && !loggedIn">
            <q-btn flat dense icon="arrow_back" @click="goBack" class="q-mb-md" :label="$t('back')" />
            <div class="text-h6 q-mt-lg">
              {{ isRegisterMode ? $t("createAccount") : $t("cloudLogin") }}
            </div>

            <q-input
              v-if="isRegisterMode"
              v-model="userName"
              class="q-mt-md"
              filled
              :label="$t('yourName')"
              type="text"
              :disable="loginLoading"
            />

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
                v-if="!isRegisterMode"
                color="primary"
                :label="$t('login')"
                :disable="!userEmail || !userPassword"
                :loading="loginLoading"
                @click="doLogin"
              />
              <q-btn
                v-if="!isRegisterMode"
                color="secondary"
                outline
                :label="$t('register')"
                :disable="loginLoading"
                @click="isRegisterMode = true"
              />

              <q-btn
                v-if="isRegisterMode"
                color="primary"
                :label="$t('createAccount')"
                :disable="!userEmail || !userPassword || !userName"
                :loading="loginLoading"
                @click="doRegister"
              />
              <q-btn
                v-if="isRegisterMode"
                flat
                :label="$t('backToLogin')"
                :disable="loginLoading"
                @click="isRegisterMode = false"
              />
            </div>
          </div>


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

            <div class="q-mt-sm row items-center q-gutter-sm">
              <span class="text-caption">{{ $t("needNewMesh") }}</span>
              <q-btn
                dense
                flat
                color="primary"
                :label="$t('createMeshBtn')"
                @click="showCreateMeshDialog = true"
              />
            </div>

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

            <q-btn
              v-if="device && !device.meshId"
              class="q-mt-md"
              color="primary"
              :label="$t('updateDeviceBtn')"
              :disable="!selectedMeshId"
              @click="updateDevice"
            />
          </div>

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

          <div v-if="setupDone">
             <q-icon name="cloud_done" color="positive" size="xl" class="q-mt-lg"/>
             <div class="text-h6">{{ $t('setupComplete') }}</div>
             <div class="text-subtitle2">{{ $t('deviceWillRestart') }}</div>
          </div>

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

    <!-- Dialog for creating a new mesh -->
    <q-dialog v-model="showCreateMeshDialog">
      <q-card style="min-width: 350px">
        <q-card-section>
          <div class="text-h6">{{ $t("createNewMesh") }}</div>
        </q-card-section>

        <q-card-section class="q-pt-none">
          <q-input
            v-model="newMeshName"
            filled
            :label="$t('meshName')"
            :hint="$t('meshNameHint')"
            autofocus
          />
          <div class="row q-col-gutter-sm q-mt-sm">
            <div class="col-6">
              <q-input
                v-model.number="newMeshLat"
                type="number"
                step="0.000001"
                filled
                :label="$t('latitude')"
                :hint="$t('latHint')"
              />
            </div>
            <div class="col-6">
              <q-input
                v-model.number="newMeshLon"
                type="number"
                step="0.000001"
                filled
                :label="$t('longitude')"
                :hint="$t('lonHint')"
              />
            </div>
          </div>
        </q-card-section>

        <q-card-actions align="right">
          <q-btn flat :label="$t('cancel')" color="primary" v-close-popup />
          <q-btn
            flat
            :label="$t('create')"
            color="primary"
            :loading="creatingMesh"
            :disable="!newMeshName"
            @click="createMesh"
          />
        </q-card-actions>
      </q-card>
    </q-dialog>
  </q-page>
</template>

<script setup>
import { ref, computed, onMounted, onUnmounted } from "vue";
import { api } from "@/services/api";
import { useI18n } from "vue-i18n";
import { useQuasar } from "quasar";

const { t } = useI18n();
const $q = useQuasar();

// Defaults for mesh center if user leaves blank
const DEFAULT_LAT = -23.5505;
const DEFAULT_LON = -46.6333;

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

const isRegisterMode = ref(false);
const userName = ref("");

const device = ref(null);
const newDeviceName = ref("");
const newDeviceReadingsPerBatch = ref(5); // Default value
const newDeviceWakeUpInterval = ref(15); // Default value
const meshes = ref([]);
const selectedMeshId = ref(null);
const changingMesh = ref(false);
const setupDone = ref(false);
const completingSetup = ref(false);
const updatingDeviceLoading = ref(false);

const controllers = ref([]);
const selectedControllerMac = ref("");
const workerName = ref("");
const controllersLoading = ref(false);
const bindingWorker = ref(false);

// Add missing dialog state refs (if not present)
const showCreateMeshDialog = ref(false);
const newMeshName = ref("");
const newMeshLat = ref(DEFAULT_LAT);
const newMeshLon = ref(DEFAULT_LON);
const creatingMesh = ref(false);

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
  if (!loggedIn.value) {
    return isRegisterMode.value ? t("status_register") : t("status_login");
  }
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
      isRegisterMode.value = false;
      userName.value = "";
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

async function doRegister() {
  loginLoading.value = true;
  try {
    await api.registerUser(userEmail.value, userPassword.value, userName.value);
    
    $q.notify({
      type: "positive",
      message: t("registerSuccess"),
      caption: t("pleaseLogin"),
    });
    
    // Reset fields and go back to login mode
    isRegisterMode.value = false;
    userPassword.value = ""; // Clear password for security
    userName.value = ""; 

  } catch (e) {
    showError(e, "registerFailed");
  } finally {
    loginLoading.value = false;
  }
}

async function doLogin() {
  loginLoading.value = true;
  try {
    const loginResponse = await api.saveUserCreds(userEmail.value, userPassword.value);
    console.log("Login Response:", loginResponse);
    console.log("Device object from login:", loginResponse.device);
    loggedIn.value = true;
    $q.notify({
      type: "positive",
      message: t("loginSuccess")
    });
    device.value = loginResponse.device;
    console.log("Device state after login:", device.value);
    // Always fetch meshes after login to prepare for the next step
    await fetchMeshes();
  } catch (e) {
    showError(e, "loginFailed");
  } finally {
    loginLoading.value = false;
  }
}

// --- Methods for device and mesh flow ---
async function checkDevice() {
  console.log("Checking device status from server...");
  try {
    const dev = await api.getDevice();
    console.log("getDevice API response:", dev);
    device.value = dev;
    console.log("Device state after checkDevice:", device.value);
    if (device.value?.id) {
      console.log("Device has ID, fetching meshes...");
      await fetchMeshes();
    }
  } catch (e) {
    if (e.message.includes("404")) {
       device.value = null;
    } else {
      showError(e, "failedToCheckDevice");
    }
  }
}

async function registerDevice() {
  try {
    const regResponse = await api.registerDevice(
      newDeviceName.value,
      role.value,
      selectedMeshId.value,
      newDeviceReadingsPerBatch.value,
      newDeviceWakeUpInterval.value
    );
    console.log("Register Response:", regResponse);
    $q.notify({ type: "positive", message: t("deviceRegistered") });
    await checkDevice();
  } catch (e)
 {
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
    const resp = await api.updateDevice(
      newDeviceName.value || undefined,
      newDeviceReadingsPerBatch.value || undefined,
      newDeviceWakeUpInterval.value || undefined,
      selectedMeshId.value || undefined
    );
    // Typo fix: updatingDeviceLoagind -> updatingDeviceLoading
    updatingDeviceLoading.value = false; 
    console.log("Update Response:", resp);
    $q.notify({ type: "positive", message: t("deviceBound") });
    await checkDevice();
    changingMesh.value = false;
  } catch (e) {
    // Also ensure loading is false on error
    updatingDeviceLoading.value = false; 
    showError(e, "failedToupdateDevice");
  }
}

async function changeMesh() {
  return changeDeviceParams();
}

async function changeDeviceParams() {
  changingMesh.value = true;
  selectedMeshId.value = device.value?.meshId || null;
  newDeviceName.value = device.value?.name || newDeviceName.value || "";
  newDeviceReadingsPerBatch.value = device.value?.readingsPerBatch || newDeviceReadingsPerBatch.value;
  newDeviceWakeUpInterval.value = device.value?.wakeUpInterval || newDeviceWakeUpInterval.value;
  await fetchMeshes();
}

async function asyncSaveDeviceParams() {
  // *** NEW: Set loading state ***
  updatingDeviceLoading.value = true;
  try {
    if (!device.value) {
      await registerDevice();
    } else {
      await api.updateDevice(
        newDeviceName.value || undefined,
        newDeviceReadingsPerBatch.value || undefined,
        newDeviceWakeUpInterval.value || undefined,
        selectedMeshId.value || device.value.meshId || undefined,
        role.value
      );
      $q.notify({ type: "positive", message: t("deviceBound") });
      changingMesh.value = false;
      await checkDevice();
    }
  } catch (e) {
    showError(e, "failedToupdateDevice");
  } finally {
    updatingDeviceLoading.value = false;
  }
}

async function completeSetup() {
  if (completingSetup.value) return;
  completingSetup.value = true;
  try {
    await api.completeSetup();
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
      scanControllers();
      return;
    }
    if (setupStatus.stage === 'user_saved' || setupStatus.stage === 'mesh_selected') {
        console.log("Stage is 'user_saved' or 'mesh_selected', setting loggedIn and checking device.");
        loggedIn.value = true;
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

async function createMesh() {
  console.log("[createMesh] clicked");
  creatingMesh.value = true;
  try {
    const lat = Number.isFinite(newMeshLat.value) ? newMeshLat.value : DEFAULT_LAT;
    const lon = Number.isFinite(newMeshLon.value) ? newMeshLon.value : DEFAULT_LON;

    await api.createMesh(newMeshName.value.trim(), lat, lon);
    $q.notify({ type: "positive", message: t("meshCreated") });

    showCreateMeshDialog.value = false;

    // Refresh meshes and try selecting the one just created by name
    await fetchMeshes();
    const m = meshes.value.find(m => m.name === newMeshName.value.trim());
    if (m) selectedMeshId.value = m.id;

    // Reset fields
    newMeshName.value = "";
    newMeshLat.value = DEFAULT_LAT;
    newMeshLon.value = DEFAULT_LON;
  } catch (e) {
    showError(e, "failedToCreateMesh");
  } finally {
    creatingMesh.value = false;
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