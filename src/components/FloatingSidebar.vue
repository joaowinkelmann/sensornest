<template>
  <div class="sidebar-container">
    <!-- Floating Save + Undo Buttons -->
    <transition name="save-button-fade">
      <div v-if="showSaveButton || showUndoButton" class="floating-action-buttons">
        <div v-if="showUndoButton" class="floating-undo-button">
          <ActionItem class="undo-button"
            :action="{ id: 'undo-zone', icon: 'undo', tooltip: t('mapsidebar.undoTooltip') }"
            @click="$emit('undo-zone-clicked')" />
        </div>
        <div v-if="showSaveButton" class="floating-save-button">
          <ActionItem class="save-button" :action="{ id: 'save-zone', icon: 'save', tooltip: t('mapsidebar.saveTooltip') }"
            @click="$emit('save-zone-clicked')" />
        </div>
      </div>
    </transition>

    <div class="floating-sidebar" :class="{ expanded }" @transitionend="onTransitionEnd" ref="sidebarRef">
      <div class="sidebar-toggle" @click="toggleSidebar">
        <q-icon :name="expanded ? 'close' : 'build'" size="24px" color="white" />
      </div>
      <transition name="sidebar-actions-fade">
        <div v-if="expanded || showActions" class="sidebar-actions-wrapper" :class="{ visible: expanded }">
          <div class="sidebar-actions">
            <!-- Mesh Selector -->
            <div class="selector-container">
              <div class="selector-header">
                <q-icon name="grid_on" size="20px" />
                <span>{{ t('floatingSidebar.mesh') }}</span>
              </div>
              <q-select
                filled
                dense
                :model-value="selectedMeshId"
                :options="meshOptions"
                @update:model-value="handleMeshSelect"
                :label="t('floatingSidebar.selectMesh')"
                class="mesh-select"
                options-dense
                emit-value
                map-options
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

            <!-- Divider -->
            <q-separator class="selector-divider" />

            <!-- Zone Selector (only if mesh is selected) -->
            <div v-if="selectedMeshId && zones.length > 0" class="selector-container">
              <div class="selector-header">
                <q-icon name="layers" size="20px" />
                <span>{{ t('floatingSidebar.zones') }}</span>
              </div>
              <q-select
                filled
                dense
                :model-value="activeZoneId"
                :options="zoneOptions"
                @update:model-value="handleZoneSelected"
                :label="t('floatingSidebar.selectZone')"
                class="zone-select"
                options-dense
                emit-value
                map-options
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

            <!-- Divider -->
            <q-separator v-if="selectedMeshId" class="selector-divider" />

            <!-- Action Buttons -->
            <div class="action-buttons-container">
              <ActionGroup 
                v-for="(group, index) in actionGroups" 
                :key="index" 
                :actions="group"
                @action-click="handleActionClick" 
                :show-divider="false" 
              />
            </div>
          </div>
        </div>
      </transition>
    </div>
  </div>
</template>

<script setup>
import { ref, computed } from 'vue';
import { useQuasar } from 'quasar';
import { useZonesStore } from '@/stores/zones';
import ActionItem from './ActionItem.vue';
import ActionGroup from './ActionGroup.vue';
import { useI18n } from 'vue-i18n';

const { t } = useI18n();
const props = defineProps({
  meshes: { type: Array, default: () => [] },
  zones: { type: Array, default: () => [] },
  activeZoneId: { type: [String, Number], default: null },
  isZoneDirty: { type: Boolean, default: false },
  selectedMeshId: { type: [String, Number], default: null },
  canCreateZone: { type: Boolean, default: false },
  canSaveZone: { type: Boolean, default: false },
  showSaveButton: { type: Boolean, default: false },
  showUndoButton: { type: Boolean, default: false },
  editMode: { type: String, default: 'zones' }
});

const emit = defineEmits([
  'mesh-select-clicked',
  'zone-create-clicked',
  'save-zone-clicked',
  'undo-zone-clicked',
  'update:editMode'
]);

const $q = useQuasar();
const zonesStore = useZonesStore();

const expanded = ref(false);
const showActions = ref(false);

// Mesh options for dropdown
const meshOptions = computed(() => {
  return props.meshes.map(mesh => ({
    label: mesh.name,
    value: mesh.id
  }));
});

// Zone options for dropdown
const zoneOptions = computed(() => {
  return props.zones.map(zone => ({
    label: zone.name,
    value: zone.id
  }));
});

// Action groups (only create zone button now)
const actionGroups = computed(() => [
  [
    { 
      id: 'create-zone', 
      icon: 'add_location', 
      tooltip: t('mapsidebar.createZoneTooltip'), 
      action: 'zone-create-clicked', 
      disabled: !props.canCreateZone 
    }
  ]
]);

const toggleSidebar = () => {
  if (!expanded.value) {
    expanded.value = true;
    showActions.value = true;
  } else {
    expanded.value = false;
    showActions.value = false;
  }
};

const onTransitionEnd = () => {
  if (!expanded.value) showActions.value = false;
};

const handleActionClick = (actionId) => {
  const action = actionGroups.value.flat().find(item => item.id === actionId);
  if (action && !action.disabled) emit(action.action);
};

const handleMeshSelect = (meshId) => {
  if (meshId) {
    emit('mesh-select-clicked', meshId);
  }
};

const handleZoneSelected = (zoneId) => {
  if (props.activeZoneId === zoneId) return;

  const switchZone = () => {
    emit('undo-zone-clicked'); // to restore changes
    zonesStore.setActiveZone(zoneId);
  };

  if (props.isZoneDirty) {
    $q.dialog({
      title: t('mapsidebar.confirmTitle'),
      message: t('mapsidebar.unsavedChanges'),
      cancel: true,
      persistent: true,
      ok: { label: t('mapsidebar.discardButton'), color: 'negative' }
    }).onOk(() => {
      switchZone();
    });
  } else {
    switchZone();
  }
};

defineExpose({
  closeSidebar: () => { expanded.value = false; showActions.value = false; },
  openSidebar: () => { expanded.value = true; showActions.value = true; }
});
</script>

<style scoped>
.sidebar-container {
  display: flex;
  align-items: flex-start;
  gap: 12px;
}

.floating-save-button {
  flex-shrink: 0;
}

.floating-undo-button {
  flex-shrink: 0;
  margin-right: 8px;
}

.save-button-fade-enter-active,
.save-button-fade-leave-active {
  transition: opacity 0.15s, transform 0.15s;
}

.save-button-fade-enter-from,
.save-button-fade-leave-to {
  opacity: 0;
  transform: scale(0.8) translateX(10px);
}

.floating-sidebar {
  display: flex;
  flex-direction: row;
  align-items: center;
  background: rgba(45, 45, 45, 0.45);
  box-shadow: 0 4px 24px rgba(0, 0, 0, 0.10);
  backdrop-filter: blur(8px);
  border-radius: 50px;
  border: 1px solid rgba(255, 255, 255, 0.1);
  transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1);
  overflow: hidden;
  width: 56px;
  height: 56px;
}

.floating-sidebar.expanded {
  width: auto;
  max-width: calc(100vw - 100px);
  height: auto;
  border-radius: 28px;
  padding: 8px;
}

.sidebar-toggle {
  display: flex;
  align-items: center;
  justify-content: center;
  width: 56px;
  height: 56px;
  min-width: 56px;
  cursor: pointer;
  transition: all 0.2s ease;
  border-radius: 50%;
  flex-shrink: 0;
}

.sidebar-toggle:hover {
  background: rgba(255, 255, 255, 0.1);
  transform: scale(1.05);
}

.sidebar-actions-wrapper {
  max-height: 0;
  opacity: 0;
  overflow: hidden;
  transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1);
}

.sidebar-actions-wrapper.visible {
  max-height: 500px;
  opacity: 1;
  overflow: visible;
}

.sidebar-actions {
  display: flex;
  flex-direction: row;
  align-items: center;
  gap: 16px;
  padding: 0 8px;
  flex-wrap: wrap;
}

.selector-container {
  min-width: 200px;
  max-width: 250px;
}

.selector-header {
  display: flex;
  align-items: center;
  gap: 8px;
  font-weight: 500;
  margin-bottom: 8px;
  color: white;
  font-size: 14px;
}

.mesh-select,
.zone-select {
  width: 100%;
}

.selector-divider {
  height: 40px;
  width: 1px;
  background-color: rgba(255, 255, 255, 0.2);
}

.action-buttons-container {
  display: flex;
  gap: 8px;
}

/* preserve existing save-button styles */
.save-button {
  width: 56px;
  height: 56px;
  background: rgba(44, 12, 161, 0.55);
  box-shadow: 0 4px 24px rgba(0, 0, 0, 0.10);
  backdrop-filter: blur(8px);
  border-radius: 50%;
}

.save-button:hover {
  background: rgba(44, 12, 161, 0.85) !important;
  transform: scale(1.05);
}

.undo-button {
  width: 56px;
  height: 56px;
  background: rgba(161, 98, 12, 0.55);
  box-shadow: 0 4px 24px rgba(0, 0, 0, 0.10);
  backdrop-filter: blur(8px);
  border-radius: 50%;
}

.undo-button:hover {
  background: rgba(161, 98, 12, 0.85) !important;
  transform: scale(1.05);
}

/* Responsive design */
@media (max-width: 768px) {
  .sidebar-container {
    top: 64px;
    right: 16px;
    gap: 8px;
  }

  .floating-sidebar {
    width: 48px;
    height: 48px;
  }

  .floating-sidebar.expanded {
    max-width: calc(100vw - 80px);
    padding: 8px;
  }

  .sidebar-toggle {
    width: 48px;
    height: 48px;
    min-width: 48px;
  }

  .sidebar-actions {
    flex-direction: column;
    gap: 12px;
    align-items: stretch;
  }

  .selector-container {
    min-width: 180px;
    max-width: 100%;
  }

  .selector-divider {
    width: 100%;
    height: 1px;
  }

  .action-buttons-container {
    width: 100%;
    justify-content: center;
  }
}

@media (max-width: 480px) {
  .selector-container {
    min-width: 160px;
  }
}

/* Dark mode support */
@media (prefers-color-scheme: dark) {
  .floating-sidebar {
    background: rgba(45, 45, 45, 0.45);
    border: 1px solid rgba(255, 255, 255, 0.08);
    box-shadow: 0 4px 24px rgba(0, 0, 0, 0.35);
  }

  .floating-sidebar.expanded {
    background: rgba(45, 45, 45, 0.45);
  }

  .sidebar-toggle {
    background: rgba(45, 45, 45, 0.65);
  }
}

.sidebar-actions-fade-enter-active,
.sidebar-actions-fade-leave-active {
  transition: opacity 0.2s ease;
}

.sidebar-actions-fade-enter-from,
.sidebar-actions-fade-leave-to {
  opacity: 0;
}
</style>