<template>
  <q-btn
    push
    :label="showLabel ? label : ''"
    :icon="icon"
    :color="active ? color : 'white'"
    :text-color="active ? 'white' : 'black'"
    :disabled="disabled"
    :loading="loading"
    :class="[
      'control-button',
      { 'active': active, 'disabled': disabled, 'no-label': !showLabel }
    ]"
    @click="handleClick"
  >
    <q-tooltip v-if="tooltip" anchor="top middle" self="bottom middle">
      {{ tooltip }}
    </q-tooltip>
  </q-btn>
</template>

<script setup>
import { computed, ref, onMounted, onUnmounted } from 'vue';

const props = defineProps({
  label: {
    type: String,
    required: true
  },
  icon: {
    type: String,
    required: true
  },
  color: {
    type: String,
    default: 'primary'
  },
  active: {
    type: Boolean,
    default: false
  },
  disabled: {
    type: Boolean,
    default: false
  },
  loading: {
    type: Boolean,
    default: false
  },
  tooltip: {
    type: String,
    default: null
  }
});

const emit = defineEmits(['click', 'tool-selected']);

const isMobile = ref(window.innerWidth <= 768);

const updateIsMobile = () => {
  isMobile.value = window.innerWidth <= 768;
};

onMounted(() => {
  window.addEventListener('resize', updateIsMobile);
});
onUnmounted(() => {
  window.removeEventListener('resize', updateIsMobile);
});

const showLabel = computed(() => !isMobile.value);

const tooltip = computed(() => {
  if (props.tooltip) return props.tooltip;
  if (props.disabled) {
    return `${props.label} (disabled)`;
  }
  if (props.active) {
    return `${props.label} mode active`;
  }
  return `Switch to ${props.label.toLowerCase()} mode`;
});

const handleClick = () => {
  if (!props.disabled && !props.loading) {
    emit('click');
    emit('tool-selected', { label: props.label, icon: props.icon, tooltip: tooltip.value });
  }
};
</script>

<style scoped>
.control-button {
  min-width: 80px;
  font-weight: 600;
  text-transform: none;
  border-radius: 6px;
  transition: all 0.2s ease;
  position: relative;
  overflow: hidden;
  display: inline-flex;
  align-items: center;
  justify-content: center;
}

.control-button.no-label {
  min-width: 44px;
  padding-left: 0;
  padding-right: 0;
}

.control-button:not(.disabled):hover {
  transform: translateY(-1px);
  box-shadow: 0 4px 12px rgba(0, 0, 0, 0.2);
}

.control-button.active {
  box-shadow: 0 0 0 2px rgba(255, 255, 255, 0.3);
}

.control-button.disabled {
  opacity: 0.6;
}

/* Ripple effect */
.control-button::before {
  content: '';
  position: absolute;
  top: 50%;
  left: 50%;
  width: 0;
  height: 0;
  border-radius: 50%;
  background: rgba(255, 255, 255, 0.3);
  transform: translate(-50%, -50%);
  transition: width 0.6s, height 0.6s;
}

.control-button:active::before {
  width: 300px;
  height: 300px;
}

/* Icon animations */
:deep(.q-icon) {
  transition: transform 0.2s ease;
}

.control-button.active :deep(.q-icon) {
  transform: scale(1.1);
}

/* Loading state */
.control-button[loading] {
  pointer-events: none;
}

/* Mobile optimizations */
@media (max-width: 768px) {
  .control-button {
    min-width: 44px;
    font-size: 0;
    padding: 6 6px;
  }
  .control-button :deep(.q-btn__content) {
    flex-direction: column;
    gap: 0;
  }
  .control-button :deep(.q-icon) {
    font-size: 22px;
  }
}

/* Hide label on mobile */
@media (max-width: 768px) {
  .control-button :deep(.q-btn__content > span:not(.q-icon)) {
    display: none !important;
  }
}

/* High contrast mode */
@media (prefers-contrast: high) {
  .control-button {
    border: 2px solid currentColor;
  }
  .control-button.active {
    border-width: 3px;
  }
}

/* Reduced motion */
@media (prefers-reduced-motion: reduce) {
  .control-button {
    transition: none;
  }
  .control-button:not(.disabled):hover {
    transform: none;
  }
  .control-button::before {
    display: none;
  }
}
</style>