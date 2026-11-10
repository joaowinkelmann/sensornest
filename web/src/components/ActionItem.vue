<template>
  <div 
    class="action-item" 
    :class="{
      'disabled': action.disabled,
      'danger': action.variant === 'danger',
      'active': action.active
    }"
    @click="handleClick"
  >
    <q-icon 
      :name="action.icon" 
      size="20px" 
      color="white" 
    />
    <q-tooltip 
      v-if="action.tooltip"
      anchor="center left" 
      self="center right" 
      :offset="[10, 0]"
    >
      {{ action.tooltip }}
    </q-tooltip>
  </div>
</template>

<script setup>
const props = defineProps({
  action: {
    type: Object,
    required: true,
    validator: (action) => {
      return action.id && action.icon;
    }
  }
});

const emit = defineEmits(['click']);

const handleClick = () => {
  if (!props.action.disabled) {
    emit('click');
  }
};
</script>

<style scoped>
.action-item {
  width: 44px;
  height: 44px;
  border-radius: 50%;
  background: rgba(75, 75, 75, 0.55);
  display: flex;
  align-items: center;
  justify-content: center;
  cursor: pointer;
  transition: all 0.2s ease;
  backdrop-filter: blur(5px);
  border: 1px solid rgba(255, 255, 255, 0.3);
}

.action-item:not(.disabled):hover {
  background: rgba(45, 45, 45, 0.85);
  transform: scale(1.1);
}

.action-item.danger:not(.disabled):hover {
  background: rgba(244, 67, 54, 0.3);
  border-color: rgba(244, 67, 54, 0.5);
}

.action-item.disabled {
  opacity: 0.5;
  cursor: not-allowed;
}

.action-item.disabled:hover {
  transform: none;
}

.action-item.active {
  background: rgba(44, 12, 161, 0.85);
  transform: scale(1.1);
  border-color: rgba(94, 23, 235, 0.8);
}

/* save button */
.save-button {
  width: 56px;
  height: 56px;
  background: rgba(44, 12, 161, 0.55);
  box-shadow: 0 4px 24px rgba(0,0,0,0.10);
  backdrop-filter: blur(8px);
  border-radius: 50%;
}

.save-button:hover {
  background: rgba(44, 12, 161, 0.85) !important;
  transform: scale(1.05);
}

@media screen and (max-width: 768px) {
  .save-button {
    width: 48px !important;
    height: 48px !important;
  }
}

/* save button end */

/* Mobile responsiveness */
@media (max-width: 768px) {
  .action-item {
    width: 40px;
    height: 40px;
  }
}
</style>