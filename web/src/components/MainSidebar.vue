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
              <img src="/sensornest/logo.webp" :alt="t('sidebar.logoAlt')" />
            </q-icon>
          </q-item-section>
          <q-item-section>
            <q-item-label class="text-weight-bold" style="font-size: 1.25rem;">
              {{ t('sidebar.title') }}
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

        <!-- <q-item clickable v-ripple to="/reports">
          <q-item-section avatar>
            <q-icon name="assessment" />
          </q-item-section>
          <q-item-section>{{t('sidebar.reports')}}</q-item-section>
        </q-item> -->

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

    <div class="absolute-bottom">
      <q-separator />

      <q-list padding>
        <q-item clickable v-ripple @click="langDialog = true">
          <q-item-section avatar>
            <q-icon name="language" />
          </q-item-section>
          <q-item-section>
            <q-item-label class="ellipsis text-no-wrap">{{ currentLocaleLabel }}</q-item-label>
          </q-item-section>
        </q-item>

        <q-item clickable v-ripple to="/profile">
          <q-item-section avatar>
            <q-icon name="person" />
          </q-item-section>
          <q-item-section>
            <q-item-label class="ellipsis text-no-wrap">
              {{ displayUserName }}
            </q-item-label>
          </q-item-section>
        </q-item>

        <q-item clickable v-ripple @click="$emit('logout')">
          <q-item-section avatar>
            <q-icon name="logout" />
          </q-item-section>
          <q-item-section>
            <q-item-label class="ellipsis text-no-wrap">Logout</q-item-label>
          </q-item-section>
        </q-item>
      </q-list>
    </div>

    <q-dialog v-model="langDialog" transition-show="none" transition-hide="none">
      <q-card style="min-width: 320px">
        <q-card-section class="text-h6">
          {{ t('sidebar.language') || 'Language' }}
        </q-card-section>
        <q-separator />
        <q-list separator>
          <q-item
            v-for="opt in localeOptions"
            :key="opt.value"
            clickable
            v-ripple
            @click="selectLocale(opt.value)"
          >
            <q-item-section avatar>
              <q-icon name="language" />
            </q-item-section>
            <q-item-section>{{ opt.label }}</q-item-section>
            <q-item-section side>
              <q-icon v-if="opt.value === currentLocale" name="check" />
            </q-item-section>
          </q-item>
        </q-list>
        <q-card-actions align="right">
          <q-btn flat color="primary" :label="t('common.ok') || 'OK'" v-close-popup />
        </q-card-actions>
      </q-card>
    </q-dialog>
  </q-drawer>
</template>

<script setup>
import { ref, watch, computed } from 'vue';
import { useI18n } from 'vue-i18n';

const { t, locale: i18nLocale } = useI18n();

const { modelValue, user } = defineProps({
  modelValue: { type: Boolean, required: true },
  user: { type: Object, default: null },
});

defineEmits(['update:modelValue', 'logout']);

const miniState = ref(true);

// Language selector
const langDialog = ref(false);
const localeOptions = [
  { label: 'English', value: 'en-US' },
  { label: 'Español', value: 'es-ES' },
  { label: 'Português (Brasil)', value: 'pt-BR' }
];

// Guard when vue-i18n mock doesn't provide a locale ref
const currentLocale = ref(i18nLocale?.value ?? 'en-US');
const currentLocaleLabel = computed(() =>
  localeOptions.find(o => o.value === currentLocale.value)?.label || 'Language'
);
function selectLocale(val) {
  currentLocale.value = val;
  langDialog.value = false;
}
watch(currentLocale, (val) => {
  if (i18nLocale) i18nLocale.value = val;
});

// Cap user name to "First Last"
const displayUserName = computed(() => {
  const name = user?.name?.trim();
  if (!name) return t('sidebar.user');
  const parts = name.split(/\s+/);
  return parts.length === 1 ? parts[0] : `${parts[0]} ${parts.at(-1)}`;
});
</script>

<style scoped>
.q-drawer {
  transition: width 0.3s;
}

/* Ensure label sections truncate like the top list */
.absolute-bottom .q-item__section--main {
  min-width: 0;
}
</style>