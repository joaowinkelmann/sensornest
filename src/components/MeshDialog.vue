<template>
  <q-dialog v-model="dialogOpen" @hide="$emit('close')">
    <q-card style="min-width: 300px" class="modern-dialog">
      <q-card-section class="dialog-header">
        <div class="text-h6">Select Mesh</div>
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
        <q-select
          filled
          v-model="selectedMesh"
          :options="meshOptions"
          label="Selected Mesh"
          emit-value
          map-options
          class="modern-select"
        >
          <template v-slot:no-option>
            <q-item>
              <q-item-section class="text-grey">
                No meshes available
              </q-item-section>
            </q-item>
          </template>
        </q-select>
      </q-card-section>
      
      <q-card-actions align="right" class="dialog-actions">
        <q-btn 
          flat 
          label="Cancel" 
          color="grey-7" 
          @click="$emit('close')"
          class="action-btn"
        />
        <q-btn 
          unelevated
          label="Select" 
          color="primary" 
          :disable="!selectedMesh"
          @click="handleSelect"
          class="action-btn primary-btn"
        />
      </q-card-actions>
    </q-card>
  </q-dialog>
</template>

<script setup>
import { ref, computed, watch } from 'vue';

const props = defineProps({
  show: {
    type: Boolean,
    default: false
  },
  meshes: {
    type: Array,
    default: () => []
  },
  currentMeshId: {
    type: [String, Number],
    default: null
  }
});

const emit = defineEmits(['close', 'mesh-selected']);

const dialogOpen = ref(props.show);
const selectedMesh = ref(props.currentMeshId);

const meshOptions = computed(() =>
  props.meshes.map(mesh => ({ 
    label: mesh.name, 
    value: mesh.id 
  }))
);

watch(() => props.show, (newValue) => {
  dialogOpen.value = newValue;
});

watch(() => props.currentMeshId, (newValue) => {
  selectedMesh.value = newValue;
});

const handleSelect = () => {
  if (selectedMesh.value) {
    emit('mesh-selected', selectedMesh.value);
    emit('close');
  }
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
  padding: 24px;
  background: #fafafa;
}

.modern-select {
  background: white;
  border-radius: 8px;
}

.dialog-actions {
  padding: 16px 24px 24px;
  background: #fafafa;
  gap: 12px;
}

.action-btn {
  border-radius: 8px;
  padding: 8px 20px;
  font-weight: 500;
  text-transform: none;
  min-width: 80px;
}

.primary-btn {
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  color: white;
}

.primary-btn:not(.disabled):hover {
  background: linear-gradient(135deg, #5a6fd8 0%, #6a4190 100%);
}

/* Dark mode support */
@media (prefers-color-scheme: dark) {
  .dialog-content,
  .dialog-actions {
    background: #2d2d2d;
  }
  
  .modern-select {
    background: #3d3d3d;
  }
}
</style>