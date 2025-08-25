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
              type="password"
            />

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

const { t } = useI18n();

const role = ref(null); // 'CONTROLLER' or 'WORKER'
const ssid = ref("");
const password = ref("");
const ssidList = ref([]);
const loadingScan = ref(false);
const connecting = ref(false);
const error = ref("");
const status = ref(null);
const pollTimer = ref(null);

const connected = computed(() => !!status.value?.connected);
const step = computed(() => {
  if (!role.value) return 0;
  if (connected.value) return 3;
  if (connecting.value) return 2;
  if (ssid.value) return 1;
  return 1;
});
const progressValue = computed(() => [0, 0.34, 0.67, 1][step.value] || 0);
const statusText = computed(() => {
  if (!role.value) return t("status_chooseRole");
  if (connected.value) return t("status_connected");
  if (connecting.value) return t("status_connecting");
  if (ssid.value) return t("status_enterPassword");
  return t("status_selectNetwork");
});
const ssidOptions = computed(() =>
  ssidList.value.map((ap) => ({
    label: `${ap.ssid}  (RSSI ${ap.rssi})`,
    value: ap.ssid,
  }))
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
    role.value = setupStatus.role;
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