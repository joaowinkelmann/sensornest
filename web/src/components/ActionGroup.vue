<template>
  <div class="action-group">
    <ActionItem 
      v-for="action in actions" 
      :key="action.id"
      :action="action"
      @click="handleClick(action)"
    />
    <div v-if="showDivider" class="sidebar-divider"></div>
  </div>
</template>

<script setup>
import ActionItem from './ActionItem.vue';

defineProps({
  actions: {
    type: Array,
    required: true
  },
  showDivider: {
    type: Boolean,
    default: false
  }
});

const emit = defineEmits(['action-click']);

const handleClick = (action) => {
  if (typeof action.action === 'function') {
    action.action();
  } else {
    emit('action-click', action.id);
  }
};
</script>

<style scoped>
.action-group {
  display: flex;
  flex-direction: column;
  align-items: center;
  gap: 12px;
}

.sidebar-divider {
  width: 2px;
  height: 12px;
  background: rgba(255, 255, 255, 0.3);
  margin: 8px auto;
  border-radius: 1px;
}
</style>