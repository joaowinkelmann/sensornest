<template>
  <div class="sidebar-container">
    <!-- Floating Save + Undo Buttons -->
    <transition name="save-button-fade">
      <div v-if="showSaveButton || showUndoButton" class="floating-action-buttons">
        <div v-if="showUndoButton" class="floating-undo-button">
          <ActionItem class="undo-button"
            :action="{ id: 'undo-zone', icon: 'undo', tooltip: t('sidebar.undoTooltip') }"
            @click="$emit('undo-zone-clicked')" />
        </div>
        <div v-if="showSaveButton" class="floating-save-button">
          <ActionItem class="save-button" :action="{ id: 'save-zone', icon: 'save', tooltip: t('sidebar.saveTooltip') }"
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
            <ActionGroup v-for="(group, index) in actionGroups" :key="index" :actions="group"
              @action-click="handleActionClick" :show-divider="index < actionGroups.length - 1" />
            <div v-if="editMode === 'zones' && zones.length > 0" class="zone-selector-wrapper">
              <ZoneSelector :zones="zones" :active-zone-id="activeZoneId" @zone-selected="handleZoneSelected" />
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
import ZoneSelector from './ZoneSelector.vue';
import { useI18n } from 'vue-i18n';

const { t } = useI18n();
const props = defineProps({
  meshes: { type: Array, default: () => [] },
  zones: { type: Array, default: () => [] },
  activeZoneId: { type: String, default: null },
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

const actionGroups = computed(() => [
  [
    { id: 'select-mesh', icon: 'grid_on', tooltip: t('sidebar.selectMeshTooltip'), action: 'mesh-select-clicked' },
  ],
  [
    { id: 'edit-zones', icon: 'layers', tooltip: t('sidebar.editZonesTooltip'), action: () => emit('update:editMode', 'zones'), active: props.editMode === 'zones' },
  ],
  [
    { id: 'create-zone', icon: 'add_location', tooltip: t('sidebar.createZoneTooltip'), action: 'zone-create-clicked', disabled: !props.canCreateZone }
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

const handleZoneSelected = (zoneId) => {
  if (props.activeZoneId === zoneId) return;

  const switchZone = () => {
    emit('undo-zone-clicked'); // to restore changes
    zonesStore.setActiveZone(zoneId);
  };

  if (props.isZoneDirty) {
    $q.dialog({
      title: t('sidebar.confirmTitle'),
      message: t('sidebar.unsavedChanges'),
      cancel: true,
      persistent: true,
      ok: { label: t('sidebar.discardButton'), color: 'negative' }
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
  flex-direction: column;
  align-items: center;
  background: rgba(45, 45, 45, 0.45);
  box-shadow: 0 4px 24px rgba(0, 0, 0, 0.10);
  backdrop-filter: blur(8px);
  border: 1px solid rgba(255, 255, 255, 0.18);
  overflow: hidden;
  width: 56px;
  height: 56px;
  border-radius: 50%;
  padding: 0;
  transition:
    width 0.15s cubic-bezier(0.4, 0, 0.2, 1),
    height 0.15s cubic-bezier(0.4, 0, 0.2, 1),
    border-radius 0.15s cubic-bezier(0.4, 0, 0.2, 1),
    background 0.15s cubic-bezier(0.4, 0, 0.2, 1),
    box-shadow 0.15s cubic-bezier(0.4, 0, 0.2, 1);
  justify-content: center;
  /* Center hamburger vertically */
}

.floating-sidebar.expanded {
  width: 56px;
  min-width: 56px;
  height: auto;
  border-radius: 28px;
  background: rgba(45, 45, 45, 0.45);
  box-shadow: 0 8px 32px rgba(0, 0, 0, 0.18);
  align-items: center;
}

.sidebar-toggle {
  width: 56px;
  height: 56px;
  display: flex;
  align-items: center;
  justify-content: center;
  cursor: pointer;
  flex-shrink: 0;
  z-index: 2;
  margin: 0 auto;
  /* Center horizontally */
}

.floating-sidebar.expanded .sidebar-toggle {
  background: rgba(45, 45, 45, 0.65);
  border-radius: 28px 28px 0 0;
  width: 56px;
  min-width: 56px;
}

/* 
.sidebar-toggle:hover {
  transform: scale(1.05);
} */

.floating-sidebar.expanded .sidebar-toggle:hover {
  transform: none;
}

.sidebar-actions-wrapper {
  width: 100%;
  opacity: 0;
  pointer-events: none;
  transform: translateY(-10px) scaleY(0.95);
  transition: opacity 0.10s 0.05s, transform 0.10s 0.05s;
  z-index: 1;
  display: flex;
  flex-direction: column;
  align-items: center;
}

.sidebar-actions-wrapper.visible {
  opacity: 1;
  pointer-events: auto;
  transform: translateY(0) scaleY(1);
}

.sidebar-actions {
  padding: 8px 0;
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 12px;
}

.sidebar-actions-fade-enter-active,
.sidebar-actions-fade-leave-active {
  transition: opacity 0.05s, transform 0.05s;
}

.sidebar-actions-fade-enter-from,
.sidebar-actions-fade-leave-to {
  opacity: 0;
  transform: translateY(0) scaleY(0.95);
}

.sidebar-actions-fade-enter-to,
.sidebar-actions-fade-leave-from {
  opacity: 1;
  transform: translateY(0) scaleY(1);
}

/* new wrapper for both buttons */
.floating-action-buttons {
  display: flex;
  flex-direction: row;
  gap: 8px;
  align-items: center;
  margin-right: 8px;
}

/* Undo button style (left of save) */
.floating-undo-button .undo-button {
  /* match save-button size and glassmorphism exactly */
  width: 56px;
  height: 56px;
  background: rgba(184, 150, 14, 0.65);
  /* light yellow (translucent) */
  box-shadow: 0 4px 24px rgba(0, 0, 0, 0.10);
  backdrop-filter: blur(8px);
  border-radius: 50%;
  border: 1px solid rgba(255, 255, 255, 0.12);
}

.floating-undo-button .undo-button:hover {
  transform: scale(1.05);
  background: rgba(250, 204, 21, 0.85);
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
    width: 48px;
    min-width: 48px;
  }

  .sidebar-toggle {
    width: 48px;
    height: 48px;
  }

  .floating-sidebar.expanded .sidebar-toggle {
    min-width: 48px;
    width: 48px;
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

  .floating-sidebar.expanded .sidebar-toggle {
    background: rgba(45, 45, 45, 0.65);
  }
}
</style>