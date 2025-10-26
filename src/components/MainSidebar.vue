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
        <q-item class="q-my-md" clickable to="/">
          <q-item-section avatar>
            <q-icon>
              <img src="/sensornest/logo.webp" alt="SensorNest Logo" />
            </q-icon>
          </q-item-section>
          <q-item-section>
            <q-item-label class="text-weight-bold" style="font-size: 1.25rem;">
              SensorNest
            </q-item-label>
          </q-item-section>
        </q-item>

        <q-item clickable v-ripple to="/">
          <q-item-section avatar>
            <q-icon name="home" />
          </q-item-section>
          <q-item-section>{{t('sidebar.home')}}</q-item-section>
        </q-item>

        <q-item clickable v-ripple to="/meshes">
          <q-item-section avatar>
            <q-icon name="grid_on" />
          </q-item-section>
          <q-item-section>{{t('sidebar.meshes')}}</q-item-section>
        </q-item>

        <q-item clickable v-ripple to="/devices">
          <q-item-section avatar>
            <q-icon name="developer_board" />
          </q-item-section>
          <q-item-section>{{t('sidebar.devices')}}</q-item-section>
        </q-item>

        <q-item clickable v-ripple to="/reports">
          <q-item-section avatar>
            <q-icon name="assessment" />
          </q-item-section>
          <q-item-section>{{t('sidebar.reports')}}</q-item-section>
        </q-item>

        <q-item clickable v-ripple to="/data">
          <q-item-section avatar>
            <q-icon name="analytics" />
          </q-item-section>
          <q-item-section>{{t('sidebar.data')}}</q-item-section>
        </q-item>

        <q-item clickable v-ripple to="/map-editor">
          <q-item-section avatar>
            <q-icon name="map" />
          </q-item-section>
          <q-item-section>{{t('sidebar.mapEditor')}}</q-item-section>
        </q-item>
      </q-list>
    </q-scroll-area>

    <div class="absolute-bottom q-mb-md">
      <q-separator class="q-my-md" />
      <q-item clickable v-ripple to="/profile">
        <q-item-section avatar>
          <q-icon name="person" />
        </q-item-section>
        <q-item-section>
          <q-item-label>{{ user?.name || t('sidebar.user') }}</q-item-label>
        </q-item-section>
      </q-item>
      <q-item clickable v-ripple @click="$emit('logout')">
        <q-item-section avatar>
          <q-icon name="logout" />
        </q-item-section>
        <q-item-section>
          <q-item-label>Logout</q-item-label>
        </q-item-section>
      </q-item>
    </div>
  </q-drawer>
</template>

<script setup>
import { ref } from 'vue';
import { useI18n } from 'vue-i18n';

const { t } = useI18n();

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