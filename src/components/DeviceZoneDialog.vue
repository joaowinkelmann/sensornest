<template>
  <q-dialog :model-value="show" @update:model-value="handleClose">
    <q-card style="min-width: 350px">
      <q-card-section>
        <div class="text-h6">{{ t('devices.changeZone.title') }}</div>
      </q-card-section>

      <q-card-section class="q-pt-none">
        <q-select
          v-model="selectedZoneId"
          :options="zoneOptions"
          :label="t('devices.changeZone.selectZone')"
          emit-value
          map-options
          :disable="zoneOptions.length === 0"
        />
        <div v-if="zoneOptions.length === 0" class="text-caption text-grey q-mt-sm">
          {{ t('devices.changeZone.noZones') }}
        </div>
      </q-card-section>

      <q-card-actions align="right">
        <q-btn
          flat
          :label="t('buttons.cancel')"
          color="primary"
          data-testid="zone-cancel-btn"
          @click="handleClose"
        />
        <q-btn
          flat
          :label="t('buttons.save')"
          color="primary"
          :disable="!canSave"
          data-testid="zone-save-btn"
          @click="saveZone"
        />
      </q-card-actions>
    </q-card>
  </q-dialog>
</template>

<script setup>
import { ref, computed, watch } from 'vue';
import { useI18n } from 'vue-i18n';
import { useZonesStore } from '@/stores/zones';

const props = defineProps({
  show: { type: Boolean, default: false },
  device: { type: Object, default: null }
});

const emit = defineEmits(['close', 'zone-changed']);

const { t } = useI18n();
const zonesStore = useZonesStore();

const selectedZoneId = ref(null);

// Initialize selected zone when device changes
watch(() => props.device, (newDevice) => {
  if (newDevice) {
    selectedZoneId.value = newDevice.zoneId || null;
  }
}, { immediate: true });

// Ensure zones for the device's mesh are available when the dialog opens
watch(() => props.show, async (isShown) => {
  if (isShown && props.device?.meshId) {
    try {
      // Always fetch zones for the device's mesh to guarantee availability in the Map Editor
      await zonesStore.fetchZones(props.device.meshId);
    } catch (e) {
      console.warn('Failed to fetch mesh zones for dialog.', e);
    }

    // Optionally fetch all zones (cross-mesh), but not required for Map Editor
    if (!Array.isArray(zonesStore.allZones) || zonesStore.allZones.length === 0) {
      try {
        await zonesStore.fetchAllZones?.();
      } catch (e) {
        console.warn('Failed to fetch all zones for dialog.', e);
        // ignore; we already fetched mesh zones above
      }
    }
    selectedZoneId.value = props.device.zoneId || null;
  }
});

const zoneOptions = computed(() => {
  if (!props.device?.meshId) return [];

  // Prefer allZones only when it's non-empty; otherwise fallback to zones fetched for this mesh
  const source = (Array.isArray(zonesStore.allZones) && zonesStore.allZones.length > 0)
    ? zonesStore.allZones
    : (Array.isArray(zonesStore.zones) ? zonesStore.zones : []);

  return source
    .filter(z => z.meshId === props.device.meshId)
    .map(zone => ({ label: zone.name, value: zone.id }));
});

const canSave = computed(() => {
  return !!props.device &&
         selectedZoneId.value !== null &&
         selectedZoneId.value !== props.device.zoneId;
});

const handleClose = () => emit('close');

const saveZone = () => {
  if (!canSave.value) return;
  emit('zone-changed', { deviceId: props.device.id, zoneId: selectedZoneId.value });
  emit('close');
};
</script>
