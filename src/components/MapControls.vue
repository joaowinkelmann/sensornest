<template>
  <div class="map-controls-container">
    <!-- Instruction Display -->
    <transition name="fade">
      <div v-if="currentInstruction" class="mode-instruction">
        <q-icon :name="currentInstruction.icon" class="instruction-icon" />
        <span>{{ currentInstruction.text }}</span>
      </div>
    </transition>

    <div class="controls-group">
      <!-- Vertex Editing Controls -->
      <q-btn-group push class="mode-controls">
        <ControlButton
          v-for="control in vertexControls"
          :key="control.mode"
          :label="control.label"
          :icon="control.icon"
          :active="mode === control.mode"
          :color="control.color"
          @click="$emit('mode-changed', control.mode)"
        />
      </q-btn-group>

      <!-- Divider -->
      <div class="divider"></div>

      <!-- Other Controls -->
      <q-btn-group push class="mode-controls">
        <ControlButton
          v-for="control in otherControls"
          :key="control.mode"
          :label="control.label"
          :icon="control.icon"
          :active="mode === control.mode"
          :color="control.color"
          @click="handleOtherClick(control)"
        />
      </q-btn-group>
    </div>
  </div>
</template>

<script setup>
import { computed } from 'vue';
import ControlButton from './ControlButton.vue';

const props = defineProps({
  mode: {
    type: String,
    default: null
  },
  activeZone: {
    type: Object,
    default: null
  }
});

const emit = defineEmits(['mode-changed', 'draw-zone-clicked']);

const handleOtherClick = (control) => {
  if (control.action) {
    emit(control.action);
  } else {
    emit('mode-changed', control.mode);
  }
};

// Control definitions
const vertexControls = [
  { mode: 'add', label: 'Add Vertex', icon: 'add_circle', color: 'primary' },
  { mode: 'remove', label: 'Remove Vertex', icon: 'remove_circle', color: 'negative' },
  { mode: 'move', label: 'Move Vertex', icon: 'open_with', color: 'info' }
];

const otherControls = [
  { mode: 'move-devices', label: 'Move Devices', icon: 'swipe', color: 'teal' },
  { mode: 'draw-zone', label: 'New Zone', icon: 'edit', color: 'accent', action: 'draw-zone-clicked' },
  { mode: null, label: 'Pan Map', icon: 'pan_tool', color: 'positive' }
];

// Instructions for each mode
const modeInstructions = {
  add: { icon: 'touch_app', text: 'Click on the map to add a vertex to the selected zone.' },
  remove: { icon: 'delete', text: 'Click on a vertex marker to remove it.' },
  move: { icon: 'drag_indicator', text: 'Drag a vertex marker to move it.' },
  'move-devices': { icon: 'swipe', text: 'Drag a device to a new position.' },
  null: { icon: 'pan_tool', text: 'Pan and zoom the map freely.' }
};

const currentInstruction = computed(() => {
  if (!props.activeZone && ['add', 'remove', 'move'].includes(props.mode)) {
    return { icon: 'warning', text: 'Please select a zone before editing vertices.' };
  }
  return modeInstructions[props.mode];
});
</script>

<style scoped>
.map-controls-container {
  position: absolute;
  bottom: 20px;
  left: 50%;
  transform: translateX(-50%);
  z-index: 1000;
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 12px;
  max-width: 90vw;
}

.controls-group {
  display: flex;
  align-items: center;
  gap: 16px;
  background: rgba(255, 255, 255, 0.45);
  padding: 12px 16px;
  border-radius: 12px;
  box-shadow: 0 4px 20px rgba(0, 0, 0, 0);
  border: 1px solid rgba(255, 255, 255, 0.2);
  /* glassmorphism effect */
  backdrop-filter: blur(4px);
}

.divider {
  height: 32px;
  width: 1px;
  background-color: rgba(0, 0, 0, 0.15);
}

.mode-controls {
  border-radius: 8px;
  overflow: hidden;
}

.action-controls {
  display: flex;
  gap: 8px;
}

.mode-instruction {
  background: rgba(0, 0, 0, 0.8);
  color: white;
  padding: 8px 16px;
  border-radius: 20px;
  font-size: 14px;
  font-weight: 500;
  display: flex;
  align-items: center;
  gap: 8px;
  backdrop-filter: blur(10px);
  box-shadow: 0 2px 12px rgba(0, 0, 0, 0.3);
}

.instruction-icon {
  font-size: 16px;
  opacity: 0.9;
}

/* Smooth transitions */
.fade-enter-active,
.fade-leave-active {
  transition: all 0.15s ease;
}

.fade-enter-from,
.fade-leave-to {
  opacity: 0;
  transform: translateY(10px);
}

/* Mobile responsiveness */
@media (max-width: 768px) {
  .controls-group {
    flex-direction: column;
    gap: 12px;
    padding: 16px;
  }
  
  .mode-controls {
    width: 100%;
  }
  
  :deep(.q-btn-group) {
    width: 100%;
  }
  
  :deep(.q-btn-group .q-btn) {
    flex: 1;
  }
  
  .mode-instruction {
    font-size: 13px;
    padding: 6px 12px;
  }
}

@media (max-width: 480px) {
  .map-controls-container {
    bottom: 16px;
  }
  
  .controls-group {
    padding: 12px;
  }
  
  .mode-instruction {
    max-width: 280px;
    text-align: center;
  }
}

/* Dark mode support */
@media (prefers-color-scheme: dark) {
  .controls-group {
    background: rgba(45, 45, 45, 0.45);
    border-color: rgba(255, 255, 255, 0.1);
    /* glassmorphism blur effect */
    backdrop-filter: blur(4px);
  }
}

/* High contrast mode */
@media (prefers-contrast: high) {
  .controls-group {
    background: white;
    border: 2px solid #000;
  }
  
  .mode-instruction {
    background: black;
    border: 1px solid white;
  }
}
</style>