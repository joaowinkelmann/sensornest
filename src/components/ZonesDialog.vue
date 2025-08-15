<template>
  <q-dialog v-model="dialogOpen" @hide="$emit('close')">
    <q-card style="min-width: 400px; max-height: 600px" class="modern-dialog">
      <q-card-section class="dialog-header">
        <div class="text-h6">Manage Zones</div>
        <q-btn 
          flat 
          round 
          dense 
          icon="close" 
          @click="$emit('close')"
          class="close-btn"
        />
      </q-card-section>
      
      <q-card-section class="dialog-content">
        <div v-if="zones.length === 0" class="empty-state">
          <q-icon name="location_off" size="48px" color="grey-5" />
          <div class="text-grey-6 q-mt-md">No zones available</div>
          <div class="text-grey-5 text-caption">Create a new zone to get started</div>
        </div>
        
        <div v-else class="zones-list">
          <ZoneListItem
            v-for="zone in zones"
            :key="zone.id"
            :zone="zone"
            :is-active="activeZoneId === zone.id"
            @select="handleZoneSelect"
          />
        </div>
      </q-card-section>
      
      <q-card-actions align="right" class="dialog-actions">
        <q-btn 
          flat 
          label="Close" 
          color="grey-7" 
          @click="$emit('close')"
          class="action-btn"
        />
      </q-card-actions>
    </q-card>
  </q-dialog>
</template>

<script setup>
import { ref, watch } from 'vue';
import ZoneListItem from './ZoneListItem.vue';

const props = defineProps({
  show: {
    type: Boolean,
    default: false
  },
  zones: {
    type: Array,
    default: () => []
  },
  activeZoneId: {
    type: [String, Number],
    default: null
  }
});

const emit = defineEmits(['close', 'zone-selected']);

const dialogOpen = ref(props.show);

watch(() => props.show, (newValue) => {
  dialogOpen.value = newValue;
});

const handleZoneSelect = (zoneId) => {
  emit('zone-selected', zoneId);
  emit('close');
};
</script>

<style scoped>
.modern-dialog {
  border-radius: 16px;
  box-shadow: 0 20px 60px rgba(0, 0, 0, 0.3);
  overflow: hidden;
}

.dialog-header {
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  color: white;
  display: flex;
  justify-content: space-between;
  align-items: center;
  padding: 20px 24px 16px;
}

.close-btn {
  color: rgba(255, 255, 255, 0.8);
}

.close-btn:hover {
  background: rgba(255, 255, 255, 0.1);
  color: white;
}

.dialog-content {
  padding: 16px;
  background: #fafafa;
  max-height: 400px;
  overflow-y: auto;
}

.empty-state {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  padding: 40px 20px;
  text-align: center;
}

.zones-list {
  display: flex;
  flex-direction: column;
  gap: 8px;
}

.dialog-actions {
  padding: 16px 24px 24px;
  background: #fafafa;
}

.action-btn {
  border-radius: 8px;
  padding: 8px 20px;
  font-weight: 500;
  text-transform: none;
  min-width: 80px;
}

/* Custom scrollbar for zones list */
.dialog-content::-webkit-scrollbar {
  width: 6px;
}

.dialog-content::-webkit-scrollbar-track {
  background: rgba(0, 0, 0, 0.1);
  border-radius: 3px;
}

.dialog-content::-webkit-scrollbar-thumb {
  background: rgba(0, 0, 0, 0.3);
  border-radius: 3px;
}

.dialog-content::-webkit-scrollbar-thumb:hover {
  background: rgba(0, 0, 0, 0.5);
}

/* Dark mode support */
@media (prefers-color-scheme: dark) {
  .dialog-content,
  .dialog-actions {
    background: #2d2d2d;
  }
}
</style>