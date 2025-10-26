<template>
  <q-drawer
    :model-value="modelValue"
    @update:model-value="$emit('update:modelValue', $event)"
    :mini="miniState"
    @mouseover="miniState = false"
    @mouseout="miniState = true"
    mini-to-overlay
    :width="250"
    :breakpoint="500"
    bordered
    class="bg-grey-3"
  >
    <q-scroll-area class="fit">
      <q-list padding>
        <q-item class="q-my-md">
          <q-item-section avatar>
            <q-avatar size="lg">
              <img src="/logo.png" alt="SensorNest Logo" />
            </q-avatar>
          </q-item-section>
          <q-item-section>
            <q-toolbar-title>SensorNest</q-toolbar-title>
          </q-item-section>
        </q-item>

        <q-item clickable v-ripple to="/">
          <q-item-section avatar>
            <q-icon name="home" />
          </q-item-section>
          <q-item-section>Home</q-item-section>
        </q-item>

        <q-item clickable v-ripple to="/meshes">
          <q-item-section avatar>
            <q-icon name="grid_on" />
          </q-item-section>
          <q-item-section>Meshes</q-item-section>
        </q-item>

        <q-item clickable v-ripple to="/devices">
          <q-item-section avatar>
            <q-icon name="developer_board" />
          </q-item-section>
          <q-item-section>Devices</q-item-section>
        </q-item>

        <q-item clickable v-ripple to="/reports">
          <q-item-section avatar>
            <q-icon name="assessment" />
          </q-item-section>
          <q-item-section>My Reports</q-item-section>
        </q-item>

        <q-item clickable v-ripple to="/data">
          <q-item-section avatar>
            <q-icon name="analytics" />
          </q-item-section>
          <q-item-section>View Data</q-item-section>
        </q-item>

        <q-item clickable v-ripple to="/map-editor">
          <q-item-section avatar>
            <q-icon name="map" />
          </q-item-section>
          <q-item-section>Map Editor</q-item-section>
        </q-item>
      </q-list>
    </q-scroll-area>

    <div class="absolute-bottom q-mb-md">
      <q-separator class="q-my-md" />
      <q-item>
        <q-item-section avatar>
          <q-avatar icon="person" />
        </q-item-section>
        <q-item-section>
          <q-item-label>{{ user?.name || 'User' }}</q-item-label>
        </q-item-section>
        <q-item-section side>
          <q-btn flat round icon="logout" @click="$emit('logout')" />
        </q-item-section>
      </q-item>
    </div>
  </q-drawer>
</template>

<script setup>
import { ref } from 'vue';

defineProps({
  modelValue: {
    type: Boolean,
    required: true,
  },
  user: {
    type: Object,
    default: null,
  },
});

defineEmits(['update:modelValue', 'logout']);

const miniState = ref(true);
</script>

<style scoped>
.q-drawer {
  transition: width 0.3s;
}
</style>
