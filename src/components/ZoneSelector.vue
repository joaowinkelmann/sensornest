<template>
  <div class="zone-selector-container">
    <div class="selector-header">
      <q-icon name="layers" />
      <span>Active Zone</span>
    </div>
    <q-select
      filled
      dense
      :model-value="activeZoneId"
      :options="zoneOptions"
      @update:model-value="handleSelect"
      label="Select a zone"
      class="zone-select"
      options-dense
    >
      <template v-slot:option="scope">
        <q-item v-bind="scope.itemProps">
          <q-item-section>
            <q-item-label>{{ scope.opt.label }}</q-item-label>
          </q-item-section>
        </q-item>
      </template>
    </q-select>
  </div>
</template>

<script setup>
import { computed } from 'vue';

const props = defineProps({
  zones: {
    type: Array,
    required: true
  },
  activeZoneId: {
    type: String,
    default: null
  }
});

const zoneOptions = computed(() => {
  return props.zones.map(zone => ({
    label: zone.name,
    value: zone.id
  }));
});

const emit = defineEmits(['zone-selected']);

const handleSelect = (option) => {
  if (option) {
    emit('zone-selected', option.value);
  }
};
</script>

<style scoped>
.zone-selector-container {
  padding: 8px;
  width: 100%;
}
.selector-header {
  display: flex;
  align-items: center;
  gap: 8px;
  font-weight: 500;
  margin-bottom: 8px;
  color: #333;
}
.zone-select {
  width: 100%;
}
</style>
