<template>
  <q-dialog :model-value="show" @update:model-value="$emit('close')">
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
        />
      </q-card-section>

      <q-card-actions align="right">
        <q-btn flat :label="t('buttons.cancel')" color="primary" @click="$emit('close')" />
        <q-btn flat :label="t('buttons.save')" color="primary" @click="saveZone" />
      </q-card-actions>
    </q-card>
  </q-dialog>
</template>

<script setup>
import { ref, computed, watch } from 'vue';
import { useI18n } from 'vue-i18n';
import { useZonesStore } from '@/stores/zones';

const props = defineProps({
  show: {
    type: Boolean,
    required: true,
  },
  device: {
    type: Object,
    default: null,
  },
});

const emit = defineEmits(['close', 'zone-changed']);

const { t } = useI18n();
const zonesStore = useZonesStore();

const selectedZoneId = ref(props.device?.zoneId || null);

const zoneOptions = computed(() => {
  if (!props.device) return [];
  const zonesInMesh = zonesStore.allZones.filter(z => z.meshId === props.device.meshId);
  return zonesInMesh.map(zone => ({
    label: zone.name,
    value: zone.id,
  }));
});

watch(() => props.device, (newDevice) => {
  selectedZoneId.value = newDevice?.zoneId || null;
});

const saveZone = () => {
  if (props.device && selectedZoneId.value !== props.device.zoneId) {
    emit('zone-changed', { deviceId: props.device.id, zoneId: selectedZoneId.value });
  }
};
</script>
