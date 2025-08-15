<template>
  <div class="action-group">
    <ActionItem 
      v-for="action in actions" 
      :key="action.id"
      :action="action"
      @click="$emit('action-click', action.id)"
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

defineEmits(['action-click']);
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