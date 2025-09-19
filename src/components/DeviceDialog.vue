<template>
  <q-dialog :model-value="show" @update:model-value="$emit('close')" persistent>
    <q-card style="min-width: 400px;">
      <q-card-section>
        <div class="text-h6">Edit Device</div>
      </q-card-section>

      <q-card-section v-if="localDevice">
        <q-input v-model="localDevice.name" label="Name" outlined class="q-mb-md" />
        <q-input v-model="localDevice.deviceColor" label="Device Color" outlined class="q-mb-md">
          <template v-slot:append>
            <q-icon name="colorize" class="cursor-pointer">
              <q-popup-proxy cover transition-show="scale" transition-hide="scale">
                <q-color v-model="localDevice.deviceColor" />
              </q-popup-proxy>
            </q-icon>
          </template>
        </q-input>
        <q-input
          v-model.number="localDevice.wakeUpInterval"
          label="Wake Up Interval (seconds)"
          type="number"
          outlined
          class="q-mb-md"
        />
        <q-input
          v-model.number="localDevice.readingsPerBatch"
          label="Readings Per Batch"
          type="number"
          outlined
          class="q-mb-md"
        />
      </q-card-section>

      <q-card-actions align="right">
        <q-btn flat label="Cancel" color="primary" @click="$emit('close')" />
        <q-btn label="Save" color="primary" @click="saveDevice" />
      </q-card-actions>
    </q-card>
  </q-dialog>
</template>

<script setup>
import { ref, watch } from 'vue';
import { useDevicesStore } from '@/stores/devices';

const props = defineProps({
  show: Boolean,
  deviceToEdit: Object,
});

const emit = defineEmits(['close', 'device-updated']);

const devicesStore = useDevicesStore();
const localDevice = ref(null);

watch(() => props.deviceToEdit, (newVal) => {
  if (newVal) {
    localDevice.value = { ...newVal };
  } else {
    localDevice.value = null;
  }
}, { immediate: true });

const saveDevice = async () => {
  if (!localDevice.value) return;

  const payload = {
    name: localDevice.value.name,
    deviceColor: localDevice.value.deviceColor,
    wakeUpInterval: localDevice.value.wakeUpInterval,
    readingsPerBatch: localDevice.value.readingsPerBatch,
  };

  try {
    await devicesStore.updateDevice(localDevice.value.id, payload);
    emit('device-updated');
  } catch (error) {
    console.error('Failed to update device:', error);
    // Optionally, show an error message to the user
  }
};
</script>
