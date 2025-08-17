<template>
  <q-page padding class="q-pa-md row justify-center">
    <div class="col-12 col-sm-8 col-md-6">
      <q-card flat bordered>
        <q-card-section>
          <div class="text-h6">Wi‑Fi Setup</div>
          <div class="text-subtitle2">Connect your device to your Wi‑Fi</div>
        </q-card-section>

        <q-linear-progress :value="progressValue" color="primary" class="q-mx-md q-mb-sm" />

        <q-card-section class="q-pt-none">
          <q-banner v-if="error" class="q-mb-md" dense rounded inline-actions type="negative">
            {{ error }}
            <template #action>
              <q-btn flat color="white" label="Dismiss" @click="error = ''" />
            </template>
          </q-banner>

          <div class="q-gutter-md">
            <q-select
              v-model="ssid"
              :options="ssidOptions"
              label="Select Wi‑Fi network"
              :loading="loadingScan"
              use-input
              input-debounce="0"
              fill-input
              emit-value
              map-options
              @filter="filterSSIDs"
            >
              <template #after>
                <q-btn flat round dense icon="refresh" @click="scan" :loading="loadingScan" />
              </template>
            </q-select>

            <q-input
              v-model="password"
              type="password"
              label="Password"
              :disable="!ssid"
              :rules="[val => (val && val.length) || 'Password required']"
              clearable
            />

            <div class="row items-center q-gutter-sm">
              <q-btn
                color="primary"
                label="Connect"
                :disable="!ssid || !password"
                :loading="connecting"
                @click="connect"
              />
              <q-btn
                flat
                color="secondary"
                label="Rescan"
                :disable="connecting"
                @click="scan"
              />
            </div>

            <q-banner v-if="statusText" class="q-mt-sm" dense rounded type="info">
              {{ statusText }}
            </q-banner>

            <q-banner v-if="connected" class="q-mt-sm" dense rounded type="positive">
              Connected to {{ status?.ssid }} with IP {{ status?.ip || 'DHCP pending' }}
            </q-banner>
          </div>
        </q-card-section>
      </q-card>
    </div>
  </q-page>
</template>

<script setup>
import { onMounted, onUnmounted, ref, computed } from 'vue'
import { api } from '../services/api'

const ssid = ref('')
const password = ref('')
const ssidList = ref([])
const loadingScan = ref(false)
const connecting = ref(false)
const error = ref('')
const status = ref(null)
const pollTimer = ref(null)

const connected = computed(() => !!status.value?.connected)
const step = computed(() => {
  if (connected.value) return 3
  if (connecting.value) return 2
  if (ssid.value) return 1
  return 0
})
const progressValue = computed(() => [0, 0.34, 0.67, 1][step.value] || 0)
const statusText = computed(() => {
  if (connected.value) return 'Connected!'
  if (connecting.value) return 'Connecting...'
  if (ssid.value) return 'Network selected. Enter password to continue.'
  return 'Select a network to begin.'
})

const ssidOptions = computed(() =>
  ssidList.value.map(ap => ({
    label: `${ap.ssid}  (RSSI ${ap.rssi})`,
    value: ap.ssid,
  }))
)

function filterSSIDs (val, update) {
  update(() => {
    if (!val) return
  })
}

async function scan () {
  loadingScan.value = true
  error.value = ''
  try {
    const list = await api.scan()
    // Deduplicate and sort by RSSI desc
    const unique = Object.values(
      list.reduce((acc, it) => {
        const k = it.ssid
        if (!k) return acc
        if (!acc[k] || acc[k].rssi < it.rssi) acc[k] = it
        return acc
      }, {})
    )
    unique.sort((a, b) => b.rssi - a.rssi)
    ssidList.value = unique
  } catch (e) {
    error.value = e.message || 'Scan failed'
  } finally {
    loadingScan.value = false
  }
}

async function connect () {
  error.value = ''
  status.value = null
  connecting.value = true
  try {
    await api.saveCreds({ ssid: ssid.value, password: password.value })
    await api.retryConnect()
    startPolling()
  } catch (e) {
    connecting.value = false
    error.value = e.message || 'Failed to start connection'
  }
}

async function pollStatusOnce () {
  try {
    const st = await api.status()
    status.value = st
    if (st.connected) {
      stopPolling()
      connecting.value = false
    }
  } catch (e) {
    // Keep polling; user may have switched networks temporarily
  }
}

function startPolling () {
  stopPolling()
  pollStatusOnce()
  pollTimer.value = setInterval(pollStatusOnce, 1000)
}

function stopPolling () {
  if (pollTimer.value) {
    clearInterval(pollTimer.value)
    pollTimer.value = null
  }
}

onMounted(() => {
  scan()
})

onUnmounted(() => {
  stopPolling()
})
</script>

<style scoped>
/* minimal */
</style>