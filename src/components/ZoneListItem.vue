<template>
  <div 
    class="zone-item" 
    :class="{ 'active': isActive }"
    @click="$emit('select', zone.id)"
  >
    <div class="zone-icon">
      <q-icon name="place" :color="isActive ? 'white' : 'primary'" />
    </div>
    
    <div class="zone-info">
      <div class="zone-name">{{ zone.name }}</div>
      <div class="zone-meta">
        {{ vertexCount }} {{ vertexCount === 1 ? 'point' : 'points' }}
        <q-chip
          v-if="isActive"
          size="sm"
          color="white"
          text-color="primary"
          label="Active"
          class="active-chip"
        />
      </div>
    </div>
    
    <div v-if="isActive" class="active-indicator">
      <q-icon name="check_circle" color="white" size="20px" />
    </div>
  </div>
</template>

<script setup>
import { computed } from 'vue';

const props = defineProps({
  zone: {
    type: Object,
    required: true
  },
  isActive: {
    type: Boolean,
    default: false
  }
});

defineEmits(['select']);

const vertexCount = computed(() => {
  return props.zone.vertices ? props.zone.vertices.length : 0;
});
</script>

<style scoped>
.zone-item {
  display: flex;
  align-items: center;
  padding: 16px;
  border-radius: 12px;
  background: white;
  border: 2px solid transparent;
  cursor: pointer;
  transition: all 0.2s ease;
  box-shadow: 0 2px 8px rgba(0, 0, 0, 0.1);
}

.zone-item:hover {
  border-color: rgba(45, 45, 45, 0.3);
  transform: translateY(-1px);
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.15);
}

.zone-item.active {
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  border-color: rgba(45, 45, 45, 0.5);
  color: white;
}

.zone-item.active:hover {
  border-color: rgba(255, 255, 255, 0.3);
}

.zone-icon {
  width: 40px;
  height: 40px;
  border-radius: 50%;
  background: rgba(rgba(45, 45, 45, 0.1));
  display: flex;
  align-items: center;
  justify-content: center;
  margin-right: 12px;
  transition: all 0.2s ease;
}

.zone-item.active .zone-icon {
  background: rgba(255, 255, 255, 0.2);
}

.zone-info {
  flex: 1;
  min-width: 0;
}

.zone-name {
  font-weight: 600;
  font-size: 16px;
  line-height: 1.2;
  margin-bottom: 4px;
  overflow: hidden;
  text-overflow: ellipsis;
  white-space: nowrap;
}

.zone-meta {
  font-size: 13px;
  opacity: 0.7;
  display: flex;
  align-items: center;
  gap: 8px;
}

.zone-item.active .zone-meta {
  color: rgba(255, 255, 255, 0.9);
}

.active-chip {
  font-size: 10px;
  height: 20px;
  font-weight: 600;
}

.active-indicator {
  margin-left: 12px;
  animation: pulse 2s infinite;
}

@keyframes pulse {
  0% {
    opacity: 1;
  }
  50% {
    opacity: 0.7;
  }
  100% {
    opacity: 1;
  }
}

/* Dark mode support */
@media (prefers-color-scheme: dark) {
  .zone-item {
    background: #3d3d3d;
    color: #e0e0e0;
  }
  
  .zone-item:not(.active) .zone-icon {
    background: rgba(45, 45, 45, 0.2);
  }
}
</style>